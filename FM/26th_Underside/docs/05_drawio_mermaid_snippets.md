# draw.io 貼り付け専用 Mermaid スニペット集 (`26th_Underside` / RP2040)

## 1. システム全体アーキテクチャ図 (`README.md`)

```mermaid
flowchart TD
    subgraph External ["外部デバイス＆基板"]
        Sens_URM["URM37 超音波センサー<br>・URTRIG (トリガー)<br>・URECHO (エコー時間)"]
        Sens_TSD["TSD20 LiDAR センサー<br>・UART (460,800 bps)"]
        Board_Bico["Bico メイン基板 (Serial1)<br>・UART (460,800 bps 8E1)"]
        SD_Card["SDカード (SPI)"]
        NeoPixel["NeoPixel LED<br>・ステータスインジケータ"]
    end

    subgraph Core0 ["Core 0 : UART通信 ＆ SDロギングタスク (100Hz / 10ms周期)"]
        Task_C0["setup() / loop()<br>core0_timer_callback (-10ms)"]
        UART_Rx["receiveLog()<br>Bico からの CSV 生文字列受信"]
        SD_Write["save_SD_BUF() / flashSD()<br>バッファ保存と 4Hz 周期のフラッシュ"]
        UART_Tx["transmitLog()<br>測距データを Bico へ返信"]
        LED_Ctrl["Lightup_NeoPixel() / write_intLED()<br>SD 状態と通信状態の可視化"]
        WD_Check["watchdog_update()<br>Core 1 生存監視と WDT クリア"]
    end

    subgraph Globals ["グローバル共有状態 (parameters.h)"]
        Global_Vars["対地高度データ<br>・data_under_urm_altitude_m<br>・data_under_tsd20_altitude_m"]
        Shared_Buf["readUART_BUF<br>受信生文字列バッファ"]
    end

    subgraph Core1 ["Core 1 : 測距センサー専用タスク (100Hz / 10ms周期)"]
        Task_C1["setup1() / loop1()<br>core1_timer_callback (-10ms)"]
        URM_Logic["update_echo_distance()<br>trigger_echo() (10Hz)"]
        TSD_Logic["read_tsd20()<br>チェックサム検証と距離パース"]
        ISR_URM["echo_isr() (CHANGE割り込み)<br>エコーパルス幅計測"]
    end

    Sens_URM -->|"パルス立ち上がり/立ち下がり"| ISR_URM
    ISR_URM -->|"時間差"| URM_Logic
    URM_Logic -->|"トリガー生成"| Sens_URM
    Sens_TSD -->|"UART受信"| TSD_Logic

    URM_Logic & TSD_Logic -->|"高度代入"| Global_Vars
    Task_C1 -->|"core1_alive = true"| WD_Check

    Board_Bico -->|"54項目 CSV"| UART_Rx
    UART_Rx -->|"文字列格納"| Shared_Buf
    Shared_Buf -->|"そのままコピー"| SD_Write
    SD_Write -->|"SPI 書き込み"| SD_Card

    Global_Vars -->|"Sprintf フォーマット"| UART_Tx
    UART_Tx -->|"5項目 CSV 返信"| Board_Bico

    Task_C0 -->|"SD アクティブ判定"| LED_Ctrl
    LED_Ctrl -->|"色制御 (緑/赤/青)"| NeoPixel
```

## 2. インクルード関係グラフ (`01_file_relationships.md`)

```mermaid
flowchart TD
    subgraph Core_Headers ["共通基盤層 (ヘッダーと設定)"]
        Config_H["Underside_config.h<br>(ピン配置・ペリフェラル設定)"]
        Params_H["parameters.h<br>(volatile 変数・LogData 構造体宣言)"]
    end

    subgraph Hardware_Drivers ["ハードウェア＆通信ドライバ層"]
        SD_H["SD_Under.h"]
        SD_C["SD_Under.cpp"]
        UART_H["UARTHelper_Under.h"]
        UART_C["UARTHelper_Under.cpp"]
        URM_H["URM37.h"]
        URM_C["URM37.cpp"]
        TSD_H["tsd20.h"]
        TSD_C["tsd20.cpp"]
        Neo_H["NeoPixel.h"]
        Neo_C["NeoPixel.cpp"]
        LED_H["LED.h"]
        LED_C["LED.cpp"]
    end

    subgraph Logic_Modules ["物理計算モジュール層"]
        Alt_H["calculate_altitude.h"]
        Alt_C["calculate_altitude.cpp"]
    end

    subgraph Main_Entry ["メインエントリポイント"]
        Main_INO["26th_Underside.ino"]
    end

    SD_C --> SD_H
    SD_C --> Config_H
    SD_C --> Params_H

    UART_C --> UART_H
    UART_C --> SD_H
    UART_C --> Params_H

    URM_C --> URM_H
    URM_C --> Config_H
    URM_C --> Params_H

    TSD_C --> TSD_H
    TSD_C --> Config_H
    TSD_C --> Params_H

    Neo_C --> Neo_H
    LED_C --> LED_H

    Alt_C --> Alt_H
    Alt_C --> Params_H

    Main_INO --> Params_H
    Main_INO --> Config_H
    Main_INO --> UART_H
    Main_INO --> SD_H
    Main_INO --> URM_H
    Main_INO --> TSD_H
    Main_INO --> Neo_H
    Main_INO --> LED_H
    Main_INO --> Alt_H
```

## 3. 初期化シーケンス図 (`02_core_tasks_flowchart.md`)

```mermaid
sequenceDiagram
    autonumber
    participant Core0 as Core 0 : setup() [main]
    participant Core1 as Core 1 : setup1() [multicore]
    participant UART as UART通信 (Serial1)
    participant SD as SDカード (TORICA_SD)
    participant Sens as 測距センサー / LED
    participant Timer as ハードウェアタイマー (100Hz)
    participant WDT as Watchdog Timer

    Note over Core0,Core1: システム起動（電源投入またはリセット）
    Core0->>UART: Serial.begin(460800) & initUART() [Bico用 Serial1 初期化]
    Core0->>Sens: Wire.begin() & init_NeoPixel() & init_intLED()
    Core0->>Sens: init_echo() [URM37 割り込みピン設定]
    Core0->>Sens: init_tsd20() [LiDAR用 UART (SerialPIO) 初期化]
    Core0->>SD: initSD() [SPI ピン設定と sd.begin()]

    Core0->>Timer: add_repeating_timer_ms(-10, core0_timer_callback, &core0_timer)
    Core0->>WDT: watchdog_enable(2000, 1) [2秒タイムアウト設定]
    Core0->>Sens: write_intLED(LOW) & "Setup Done." 通知

    Note over Core1: Core 1 の個別初期化
    Core1->>Timer: add_repeating_timer_ms(-10, core1_timer_callback, &core1_timer)
    Note over Core0,Core1: 双方のコアでタイマー割り込みによる 100Hz 定周期ループへ移行
```

## 4. Core 0 vs Core 1 タスクループフローチャート (`02_core_tasks_flowchart.md`)

```mermaid
flowchart TD
    subgraph Core0_Loop ["Core 0 ループ : loop() 【実行周期: 100Hz / 10ms】"]
        C0_Start(("Core 0 ループ開始")) --> C0_CheckFlag{"core0_timer_triggered<br>== true ?"}
        C0_CheckFlag -->|No| C0_Idle["待機 (次のタイマー割り込みまで)"]
        C0_CheckFlag -->|Yes| C0_Clear["core0_timer_triggered = false"]

        C0_Clear --> C0_CheckUART{"receive_available() ?<br>(Bico からの受信があるか)"}
        
        C0_CheckUART -->|Yes| C0_UART_Rx["LED 点灯 (write_intLED)<br>receiveLog() で文字列受信<br>save_SD_BUF() で SD 保存準備<br>LED 消灯"]
        C0_CheckUART -->|No| C0_CheckSD

        C0_UART_Rx --> C0_CheckSD{"SDisActive() == true ?<br>(SDカードが有効か)"}

        C0_CheckSD -->|Yes| C0_SD_Green["NeoPixel 緑点灯<br>sd_flash_counter++"]
        C0_CheckSD -->|No| C0_SD_Red["NeoPixel 赤点灯 (異常)"]

        C0_SD_Green --> C0_SD_Flash{"sd_flash_counter >= 25 ?<br>(25回 = 4Hz)"}
        C0_SD_Flash -->|Yes| C0_SD_Write["flashSD() で SPI 書き込み実行<br>カウンタリセット"]
        C0_SD_Flash -->|No| C0_SD_End
        C0_SD_Write --> C0_SD_End
        C0_SD_Red --> C0_SD_End

        C0_SD_End["NeoPixel 消灯"] --> C0_UART_Tx["transmitLog()<br>測距データを Bico へ返信"]

        C0_UART_Tx --> C0_CheckWDT{"core1_alive == true ?<br>(Core 1 が生存しているか)"}
        
        C0_CheckWDT -->|Yes| C0_WDT_Update["watchdog_update() で WDT クリア<br>core1_alive = false へリセット"]
        C0_CheckWDT -->|No| C0_End(("処理完了"))
        C0_WDT_Update --> C0_End
    end

    subgraph Core1_Loop ["Core 1 ループ : loop1() 【実行周期: 100Hz / 10ms】"]
        C1_Start(("Core 1 ループ開始")) --> C1_CheckFlag{"core1_timer_triggered<br>== true ?"}
        C1_CheckFlag -->|No| C1_Idle["待機 (次のタイマー割り込みまで)"]
        C1_CheckFlag -->|Yes| C1_Clear["core1_timer_triggered = false"]

        C1_Clear --> C1_URM_Update["update_echo_distance()<br>前回の超音波パルス幅を距離へ変換"]

        C1_URM_Update --> C1_CheckEcho{"echo_counter >= 10 ?<br>(10回 = 10Hz)"}
        C1_CheckEcho -->|Yes| C1_URM_Trigger["trigger_echo()<br>次回の超音波パルス発射<br>カウンタリセット"]
        C1_CheckEcho -->|No| C1_TSD

        C1_URM_Trigger --> C1_TSD
        C1_TSD["read_tsd20()<br>LiDAR UART の受信とパース"]

        C1_TSD --> C1_Alive["core1_alive = true<br>生存フラグを立てる"]
        C1_Alive --> C1_End(("処理完了"))
    end

    C1_Alive -.-> C0_CheckWDT
```

## 5. データ送受信・SD保存・測距パイプラインフロー (`03_data_pipeline_flowchart.md`)

```mermaid
flowchart TD
    subgraph Board_Bico ["Bico メイン基板 (Serial1 / 460,800 bps)"]
        Tx_Bico["transmitLog()<br>54項目の全ログデータを<br>CSV文字列として送信"]
        Rx_Bico["receiveUnderLog()<br>5項目の測距データを受信"]
    end

    subgraph Core0_Rx ["Core 0 : UART ロギングパイプライン"]
        U_Rx["receiveLog()<br>Serial1.readBytes()"]
        Buf_Rx["readUART_BUF<br>受信した CSV 文字列を<br>パースせずにそのまま格納"]
        SD_Add["save_SD_BUF()<br>TORICA_SD バッファへ追加"]
        SD_Flush["flashSD() (4Hzで実行)<br>SPI 経由で SD へ書き込み"]
    end

    subgraph Core1_Sense ["Core 1 : 測距センシングパイプライン"]
        S_URM["update_echo_distance()<br>URM37 超音波エコー時間パース"]
        S_TSD["read_tsd20()<br>TSD20 UART バイナリパース"]
    end

    subgraph Globals ["グローバル共有データストレージ (parameters.h)"]
        G_URM["data_under_urm_altitude_m"]
        G_TSD["data_under_tsd20_altitude_m"]
        G_BMP["data_under_bmp_pressure_hPa 等<br>(現在は 0.0 固定)"]
    end

    subgraph Core0_Tx ["Core 0 : UART 返信パイプライン"]
        U_Tx_Fmt["transmitLog()<br>sprintf() で 5 項目を<br>CSV 文字列にフォーマット"]
        U_Tx["Serial1.print()"]
    end

    Tx_Bico -->|"54項目 CSV"| U_Rx
    U_Rx --> Buf_Rx
    Buf_Rx --> SD_Add
    SD_Add --> SD_Flush
    SD_Flush -->|"SPI"| SD_Card["物理 SD カード"]

    S_URM -->|"超音波高度 (m)"| G_URM
    S_TSD -->|"LiDAR高度 (m)"| G_TSD

    G_BMP & G_URM & G_TSD ===> U_Tx_Fmt
    U_Tx_Fmt --> U_Tx
    U_Tx -->|"5項目 CSV"| Rx_Bico
```

## 6. SDロギングタイミングと NeoPixel 状態インジケータ (`04_sd_and_sensors_flowchart.md`)

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

## 7. URM37 超音波高度センサー計測回路 (`04_sd_and_sensors_flowchart.md`)

```mermaid
flowchart LR
    subgraph Trigger ["10Hz 定期トリガー (Core 1)"]
        Trig_Start["trigger_echo()<br>10ループ (100ms) に1回"] --> Pin_Low["URTRIG ピンを LOW に落とす<br>(50μs 待機)"]
        Pin_Low --> Pin_High["URTRIG ピンを HIGH に戻す<br>↓<br>echo_data_ready = false<br>waiting_for_echo = true"]
    end

    subgraph ISR ["ハードウェア割り込み (echo_isr)"]
        Echo_Pin{"URECHO ピン<br>CHANGE 検知"}
        Echo_Pin -->|"立ち下がり (LOW)"| Rec_Start["echo_start_time = micros()"]
        Echo_Pin -->|"立ち上がり (HIGH)"| Calc_Diff["echo_low_time = micros() - start_time<br>echo_data_ready = true"]
    end

    subgraph Parser ["パルス幅 -> 高度変換 (Core 1)"]
        Check_Ready{"echo_data_ready<br>== true ?"} -->|Yes| Check_Range{"low_time が<br>200 〜 40,000μs の範囲内か ?<br>(4cm 〜 8m)"}
        
        Check_Range -->|Yes| Calc_Alt["距離(cm) = low_time / 50<br>高度(m) = 距離 / 100"]
        Check_Range -->|"No (近すぎ・遠すぎ)"| Alt_10["エラー扱い<br>高度(m) = 10.0"]

        Calc_Alt --> Save_URM["data_under_urm_altitude_m へ代入"]
        Alt_10 --> Save_URM
        
        Check_Ready -->|No| Check_Timeout{"waiting_for_echo == true かつ<br>トリガーから 60ms 以上経過 ?"}
        Check_Timeout -->|"Yes (タイムアウト)"| Alt_10_TO["エラー扱い<br>高度(m) = 10.0"] --> Save_URM
    end

    Trigger -.- ISR
    ISR -.- Parser
```

## 8. TSD20 LiDAR UART パース回路 (`04_sd_and_sensors_flowchart.md`)

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

## 9. 4層レイヤー構造・関数ヒエラルキー (`06_layer_hierarchy_flowchart.md`)

```mermaid
flowchart TD
    subgraph L3 ["【Layer 3】 抽象データロジック層"]
        L3_Struct["parameters.h<br>volatile 共有変数 (data_under_urm_altitude_m 等)"]
        L3_Buf["readUART_BUF<br>受信生 CSV 文字列バッファ"]
        L3_TO["is_takeoff()<br>LiDAR 高度の移動平均による離陸確定判定"]
    end

    subgraph L2 ["【Layer 2】 中間タスク制御＆パース層"]
        L2_C0["loop() / core0_timer_callback()<br>Core 0 100Hz ループ制御"]
        L2_Rx["receiveLog()<br>BicoからのUART文字列バッファリング"]
        L2_Tx["transmitLog()<br>測距変数の sprintf CSV フォーマット化"]
        L2_SD["save_SD_BUF()<br>文字列バッファのSD用キューへの移動"]

        L2_C1["loop1() / core1_timer_callback()<br>Core 1 100Hz ループ制御"]
        L2_URM["update_echo_distance()<br>パルス幅から距離(m)への変換と範囲チェック"]
        L2_TSD["read_tsd20()<br>バイナリパース・リトルエンディアン結合・チェックサム"]
    end

    subgraph L1 ["【Layer 1】 ハードウェアドライバ＆I/O制御層"]
        L1_UART["TORICA_UART / Serial1.readBytes(), print()<br>UART フレーム送受信"]
        L1_SD["sd.flash() / flashSD()<br>SPI 経由での SD 物理セクタ書き込み"]
        L1_URM["trigger_echo() (パルス生成) &<br>echo_isr() (CHANGE割り込みハンドラ)"]
        L1_LED["Lightup_NeoPixel() / write_intLED()<br>ピン HIGH/LOW および PWM 色制御"]
        L1_WDT["watchdog_update()<br>ハードウェア WDT カウンタクリア"]
    end

    subgraph L0 ["【Layer 0】 物理ハードウェア＆RP2040 カーネル"]
        L0_HW["RP2040 デュアルコア (Core 0 / 1)<br>& 100Hz ハードウェアタイマー"]
        L0_SPI["SPI ペリフェラル & 物理 SD カード"]
        L0_UART["Serial1 (UART) & SerialPIO (TSD20用)"]
        L0_Pins["URTRIG / URECHO (CHANGE割込) ピン<br>& NeoPixel / LED ピン"]
    end

    L2_C0 -->|"タイマー駆動"| L2_Rx
    L2_Rx -->|"ドライバ呼び出し"| L1_UART
    L1_UART --- L0_UART
    L2_Rx -->|"文字列上書き"| L3_Buf

    L2_C0 --> L2_SD
    L2_SD -->|"バッファ読み出し"| L3_Buf
    L2_SD -->|"ドライバ呼び出し"| L1_SD
    L1_SD --- L0_SPI

    L2_C0 --> L2_Tx
    L2_Tx -->|"変数読み出し"| L3_Struct
    L2_Tx -->|"ドライバ呼び出し"| L1_UART

    L2_C0 -->|"状態に応じて制御"| L1_LED
    L1_LED --- L0_Pins
    L2_C0 -->|"生存確認"| L1_WDT

    L2_C1 -->|"タイマー駆動"| L2_URM
    L2_URM -->|"パルス幅取得"| L1_URM
    L2_C1 -->|"10Hz トリガー"| L1_URM
    L1_URM --- L0_Pins
    L2_URM -->|"計算結果書き込み"| L3_Struct

    L2_C1 --> L2_TSD
    L2_TSD -->|"ドライバ呼び出し"| L1_UART
    L2_TSD -->|"計算結果書き込み"| L3_Struct

    L2_C1 -->|"計算指示"| L3_TO
```
