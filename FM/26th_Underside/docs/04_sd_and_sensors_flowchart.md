# SDロギング・LEDインジケータ・高度計測仕様

`26th_Underside` の運用において、確実にデータが保存されているか、またセンサーが正常に動作しているかを確認することは極めて重要です。本ドキュメントでは、NeoPixel を用いた直感的な状態通知の仕組みと、超音波センサー・LiDAR からの正確な高度計測ロジックを図解します。

---

## 1. SD ロギングタイミングと NeoPixel 状態インジケータ

Under 基板の `loop()` (Core 0) では、SD カードの書き込み状態を 3 色の NeoPixel で常時通知しています。
また、通信が行われるたびに基板内蔵の通常 LED (`intLED`) が点滅するため、地上で「通信」と「記録」の双方が生きているか一目で判別可能です。

```mermaid
flowchart TD
    Start_Rx(("ループ開始 (100Hz)")) --> Rx_UART{"UART受信データあり ?<br>(receive_available)"}
    
    Rx_UART -->|Yes| LED_On["write_intLED(HIGH) 点灯<br>↓<br>receiveLog() 受信<br>save_SD_BUF() バッファ退避<br>↓<br>write_intLED(LOW) 消灯"]
    Rx_UART -->|No| SD_Check

    LED_On --> SD_Check{"SDisActive() == true ?<br>(SD初期化・認識に成功したか)"}

    SD_Check -->|Yes| Neo_Green["Lightup_NeoPixel(GREEN)<br>緑色点灯"]
    SD_Check -->|No| Neo_Red["Lightup_NeoPixel(RED)<br>赤色点灯 (SD未挿入・エラー)"]

    Neo_Green --> Count_Check{"sd_flash_counter >= 25 ?<br>(25ループ = 250ms = 4Hz)"}
    Count_Check -->|Yes| SD_Flash["sd.flash()<br>バッファ内容を SD カードの物理セクタへ書き込み<br>sd_flash_counter = 0"]
    Count_Check -->|No| End_Flow

    SD_Flash --> End_Flow
    Neo_Red --> End_Flow

    End_Flow["NeoPixel_off() 消灯<br>↓<br>transmitLog() 送信"] --> Next_Loop(("次ループへ"))
```

### なぜ 4Hz で SD に書き込むのか
毎回（100Hz）`flashSD()` を実行すると、SPI 通信のオーバーヘッドや SD カード内部のウェアレベリング処理によって書き込み遅延が発生しやすくなります。250ms ごと（4Hz）にまとめてフラッシュすることで、マイコンの負荷を下げつつ、万が一の電源断時にも直前までのデータを安全に保持できるバランスを取っています。

---

## 2. URM37 超音波高度センサー計測回路 (`update_echo_distance()`)

URM37 超音波センサーは、パルス (`trigger_echo`) を送ったあと、エコーが戻ってくるまでのピンの LOW 時間（パルス幅）に比例して距離が導出されます。ブロッキング関数である `pulseIn()` を使用せず、**ハードウェア `CHANGE` 割り込み** を用いることで、RP2040 のメイン処理を止めることなく計測を実現しています。

```mermaid
flowchart LR
    subgraph Trigger ["10Hz 定期トリガー (Core 1)"]
        Trig_Start["trigger_echo()<br>10ループ (100ms) に1回"] --> Pin_Low["URTRIG ピンを LOW に落とす<br>(50μs 待機)"]
        Pin_Low --> Pin_High["URTRIG ピンを HIGH に戻す<br>↓<br>echo_data_ready = false<br>waiting_for_echo = true"]
    end

    subgraph ISR ["ハードウェア割り込み (echo_isr)"]
        Echo_Pin{"URECHO ピン<br>CHANGE 検知"}
        Echo_Pin -->|立ち下がり (LOW)| Rec_Start["echo_start_time = micros()"]
        Echo_Pin -->|立ち上がり (HIGH)| Calc_Diff["echo_low_time = micros() - start_time<br>echo_data_ready = true"]
    end

    subgraph Parser ["パルス幅 -> 高度変換 (Core 1)"]
        Check_Ready{"echo_data_ready<br>== true ?"} -->|Yes| Check_Range{"low_time が<br>200 〜 40,000μs の範囲内か ?<br>(4cm 〜 8m)"}
        
        Check_Range -->|Yes| Calc_Alt["距離(cm) = low_time / 50<br>高度(m) = 距離 / 100"]
        Check_Range -->|No (近すぎ・遠すぎ)| Alt_10["エラー扱い<br>高度(m) = 10.0"]

        Calc_Alt --> Save_URM["data_under_urm_altitude_m へ代入"]
        Alt_10 --> Save_URM
        
        Check_Ready -->|No| Check_Timeout{"waiting_for_echo == true かつ<br>トリガーから 60ms 以上経過 ?"}
        Check_Timeout -->|Yes (タイムアウト)| Alt_10_TO["エラー扱い<br>高度(m) = 10.0"] --> Save_URM
    end

    Trigger -.- ISR
    ISR -.- Parser
```

---

## 3. TSD20 LiDAR UART パース回路 (`read_tsd20()`)

TSD20 は、UART (460,800 bps) 経由で 4 バイトのバイナリフレームを連続送信してきます。
Core 1 は `SerialPIO` を用いてこのデータを受け取り、フレームヘッダ (`0x5C`) とチェックサムを照合して距離を抽出します。

```mermaid
flowchart TD
    TSD_Rx(("read_tsd20() 呼び出し<br>【100Hz】")) --> Check_Avail{"tsd20_Serial.available()<br>>= 4 ?"}
    
    Check_Avail -->|Yes| Read_Head{"tsd20_Serial.read()<br>== 0x5C ?<br>(フレームヘッダ確認)"}
    Check_Avail -->|No| End_TSD(("処理終了"))
    
    Read_Head -->|No| End_TSD
    Read_Head -->|Yes| Read_Bytes["dist_L = read()<br>dist_H = read()<br>recv_chk = read()"]

    Read_Bytes --> Calc_Chk["calcChecksum(dist_L, dist_H)<br>ビット反転サム (NOT(L + H)) を計算"]
    
    Calc_Chk --> Verify_Chk{"recv_chk == calc_chk ?"}
    Verify_Chk -->|No| Err_Chk["チェックサムエラー<br>高度(m) = -1.0"]
    
    Verify_Chk -->|Yes| Calc_Dist["リトルエンディアン結合<br>distance_mm = (dist_H << 8) | dist_L"]

    Calc_Dist --> Check_Err{"distance_mm が<br>50000 または 20000 か？<br>(センサ側の測定エラー報告)"}

    Check_Err -->|Yes| Err_Val["範囲外エラー<br>高度(m) = -1.0"]
    Check_Err -->|No| Ok_Val["正常計測<br>高度(m) = distance_mm / 1000.0"]

    Err_Chk --> Save_TSD["data_under_tsd20_altitude_m へ代入"]
    Err_Val --> Save_TSD
    Ok_Val --> Save_TSD

    Save_TSD --> End_TSD
```
