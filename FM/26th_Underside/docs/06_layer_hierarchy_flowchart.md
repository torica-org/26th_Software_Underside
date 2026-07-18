# ソフトウェアレイヤー構造・関数ヒエラルキーと処理関係

本ドキュメントでは、`26th_Underside` (RP2040) プロジェクトにおける各関数が「物理デバイスやペリフェラルに近い低レイヤー」なのか、「タスクループや通信パースを行う中間レイヤー」なのか、「変数の保存と判定を行う高レイヤー」なのかを分類した階層（ヒエラルキー）図と、その依存関係を解説します。

---

## 1. 4層レイヤー階層（ヒエラルキー）の定義と役割

| 階層 | レイヤー名 | 役割・責務 | 該当する主なモジュール・関数・変数 |
| :---: | :--- | :--- | :--- |
| **Layer 3** | **抽象データロジック層**<br>*(Abstracted Data Layer)* | 計測された生データを保存し、他のすべてのタスクから参照・更新される変数群と、閾値に基づくシンプルな判定ロジック | `is_takeoff()`, `struct LogData`, 共有バッファ `readUART_BUF`, `data_under_urm_altitude_m` 等の `volatile` 変数 |
| **Layer 2** | **中間タスク制御＆パース層**<br>*(Task Wrapper / Parser)* | 100Hz 割り込みコールバックによるタスクループの実行、データバッファの移動、文字列フォーマット（`sprintf`）、およびチェックサムや範囲検証 | `loop()`, `loop1()`, `receiveLog()`, `transmitLog()`, `save_SD_BUF()`, `update_echo_distance()`, `read_tsd20()` |
| **Layer 1** | **ハードウェアドライバ＆I/O制御層**<br>*(Hardware Driver Layer)* | SPI / UART / GPIO ペリフェラルの初期化、SDへの物理フラッシュ、超音波パルスのトリガー、LED色制御、およびハードウェア割り込みハンドラ | `initUART()`, `initSD()`, `flashSD()`, `trigger_echo()`, `echo_isr()`, `Lightup_NeoPixel()`, `write_intLED()`, `watchdog_update()` |
| **Layer 0** | **物理ハードウェア＆RP2040 カーネルプリミティブ**<br>*(Physical Hardware / Kernel)* | デュアルコア CPU、物理ピン状態、ハードウェア割り込み機構、ハードウェアタイマー、SPI バス、UART ペリフェラル等の低レイヤーリソース | RP2040 (`Core 0` / `Core 1`), SPI (`SD_CS`, `MOSI`, `MISO`, `SCK`), GPIO/NeoPixel ピン, `Serial1`, `SerialPIO`, `CHANGE` 割り込み, `add_repeating_timer_ms` |

---

## 2. ソフトウェアレイヤーヒエラルキー＆統合呼び出し関係図

以下の図は、上段から下段に向かって**抽象度の高さ（上：高レイヤー・抽象ロジック ⇔ 下：低レイヤー・物理I/O）** を配置し、データの流れと呼び出しの方向を矢印で示しています。

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

    %% --- Core 0 の流れ ---
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

    %% --- Core 1 の流れ ---
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

---

## 3. レイヤー分離の意図と特徴（Under基板特有の設計）

Bico 基板とは異なり、Under 基板では「高度な物理計算（気圧高度や対気速度など）」が存在しません。そのため Layer 3 が非常に薄く、実質的に「ただの変数の置き場所」となっています。

一方で、**Layer 1（ハードウェアドライバ）から Layer 2（パース層）へのデータの流れが分厚い** のが特徴です。
- URM37 では、Layer 0 のピン変化が Layer 1 の割り込みハンドラ (`echo_isr`) を即座に叩き、その記録時間を Layer 2 (`update_echo_distance`) が安全なタイミングで吸い出して距離に変換します。
- TSD20 では、Layer 0/1 の UART 受信バッファに溜まったバイナリを Layer 2 (`read_tsd20`) がバイト単位で取り出し、チェックサム検証というパズルを解いて初めて Layer 3 のグローバル変数へと昇格させます。

このように、ノイズの多い物理世界（Layer 0/1）のデータを、いかに綺麗に浄化・整形してグローバル空間（Layer 3）へ送り届けるかが、Under 基板のソフトウェアにおける最大の関心事となっています。
