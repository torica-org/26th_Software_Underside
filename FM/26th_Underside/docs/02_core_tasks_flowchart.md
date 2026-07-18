# RP2040 デュアルコア処理・タイマー割り込み＆タスクフロー

`26th_Underside` プログラムでは、RP2040 の 2 つの CPU コア（Core 0 と Core 1）に処理を完全に分離し、それぞれに対して独立した 100Hz（周期 10ms）のハードウェアタイマー割り込みを設定して定周期処理を行っています。

---

## 1. 初期化およびマルチコア起動シーケンス (`setup()` / `setup1()`)

システム起動時、Core 0 が最初に `setup()` を実行し、UART（Bico通信用）や SD カード用 SPI、測距センサーのピンアサイン設定、LED、CSV ヘッダーの書き込み初期化などを行った後、ハードウェアタイマーおよび Watchdog を有効化します。Core 1 は `pico/multicore` によって自動起動され、並列でタイマー設定を行います。

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

---

## 2. Core 0 vs Core 1 のタスクループフローチャート

タイマーコールバック関数 (`core0_timer_callback` / `core1_timer_callback`) が 10ms 毎にフラグ (`core0_timer_triggered` / `core1_timer_triggered`) を `true` にすることで、メインループ (`loop()` / `loop1()`) 内の定周期処理が起動します。

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

    %% --- 接続関係 ---
    C1_Alive -.-> C0_CheckWDT
```

---

## 3. タスク分離の意図と Watchdog 機構

### 処理ブロック（SD 遅延）の回避
SD カードへの `flashSD()` 処理（SPI 通信）は、ブロック書き込みのタイミング等により突発的に数十ミリ秒の遅延（ブロッキング）を発生させるリスクがあります。
これを測距センサーの割り込みやパルス生成 (`trigger_echo`) と同じコアで行うと、**超音波のエコーパルス幅計測が狂ったり、TSD20 の UART バッファが溢れたり** します。
Core 0 を「通信と SD 記録」、Core 1 を「測距センサー専任」に分けることで、SD 書き込みが遅延しても高度計測の精度に全く影響が出ない堅牢なアーキテクチャを実現しています。

### デュアルコアクロス Watchdog 監視機構
Bico 基板と同様に、**「Core 0 と Core 1 がお互いを監視して初めて WDT がリフレッシュされる」** 相互監視を行っています。
- Core 1 が 1 ループ実行するたびに `core1_alive = true` をセットします。
- Core 0 は自ループの最後で `core1_alive == true` を確認できた場合のみ `watchdog_update()` を実行して WDT カウンタを巻き戻し、`core1_alive = false` にリセットします。
- これにより、SD 書き込みが完全にフリーズしたり、UART 割り込みがスタックしたりした場合、**2 秒経過すると自動的にシステム全体をリセット・再起動** させます。
