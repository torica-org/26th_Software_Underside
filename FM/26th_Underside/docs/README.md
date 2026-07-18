# 26th_Underside (機体下電装 / RP2040) ソフトウェア仕様書

本 `docs` ディレクトリには、鳥人間コンテスト26代機体の機体下電装（Under基板）を担う **`26th_Underside` (デュアルコア RP2040 / Raspberry Pi Pico)** のファイル構成・マルチコア同期・SDロギング・測距センサ制御仕様を網羅した仕様書と、draw.io 貼り付け可能な Mermaid 図解を収録しています。

---

## 1. ドキュメント構成（目次）

本ドキュメントは以下の6部構成（＋draw.io用コード集の全7ファイル）となっています。各Markdownファイル内で詳細なMermaidチャートと技術解説を記述しています。

| ファイル名 | タイトル | 概要 |
| :--- | :--- | :--- |
| **[01_file_relationships.md](01_file_relationships.md)** | **ファイル構成とインクルード依存グラフ** | 各 `.ino`, `.h`, `.cpp` ファイルの役割、インクルード依存関係、およびグローバル変数の共有設計 |
| **[02_core_tasks_flowchart.md](02_core_tasks_flowchart.md)** | **RP2040 デュアルコア処理＆同期フロー** | Core 0（SD書き込み＆UART送受信）と Core 1（高度センサー計測）の 100Hz タイマー割り込みと Watchdog 監視機構 |
| **[03_data_pipeline_flowchart.md](03_data_pipeline_flowchart.md)** | **データ受信・SD保存・送信パイプライン** | Bico からの受信生ログの SD ダイレクト保存と、自基板の URM/TSD20 測距データの Bico への返信フロー |
| **[04_sd_and_sensors_flowchart.md](04_sd_and_sensors_flowchart.md)** | **SDロギング・LED表示・高度計測仕様** | SD カードのフラッシュタイミングと NeoPixel 状態表示、および URM37 (超音波) / TSD20 (LiDAR) の処理回路図 |
| **[06_layer_hierarchy_flowchart.md](06_layer_hierarchy_flowchart.md)** | **4層レイヤー構造＆関数ヒエラルキー** | 全関数を「物理I/O・ハードウェア・タスク制御・抽象ロジック」の4階層へ分類し、処理の依存方向を明示した設計書 |
| **[05_drawio_mermaid_snippets.md](05_drawio_mermaid_snippets.md)** | **draw.io用貼り付けコード＆インポート手順** | draw.io (app.diagrams.net) の Mermaid インポート機能に完全対応した、純粋な図解コード集（全7図解） |

---

## 2. システム全体の概要（全体アーキテクチャ）

`26th_Underside` は、メインコントローラーである Bico 基板と対になる「機体下部のサブシステム」です。
主な役割は以下の 2 点に集約されます。
1. **メインログの SD バックアップ保存**: Bico から 460,800 bps で送られてくる全 54 項目のフライトログ CSV 文字列を受信し、パースすることなくそのまま SD カード（`TORICA_SD`）へ記録します。
2. **対地高度の計測**: 超音波センサー (URM37) と LiDAR (TSD20) を用いて地面までの距離をリアルタイム計測し、Bico へ返信します。

RP2040 のマルチコア機能を生かし、**「Core 0：SD カード書き込みと UART 通信タスク」** と **「Core 1：測距センサーのハードウェア割り込み・シリアルパース専用タスク」** を分離することで、SD カードの書き込みブロック（遅延）が測距タイミングや UART 受信の取りこぼしに影響を与えない堅牢な設計となっています。

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

    %% --- 接続関係 ---
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

---

## 3. コア別タスクと割り込み処理のまとめ

| コア | エントリ関数 | 動作周期 | タスクの責務・概要 |
| :---: | :--- | :---: | :--- |
| **Core 0** | `setup()`<br>`loop()` | **100Hz (10ms)**<br>`core0_timer` | 1. **UART 受信と SD ロギング**: Bico からの 54 項目テレメトリを `receiveLog()` で受け取り、パースせずにそのまま `save_SD_BUF()` で SD バッファへ直行させ、4Hz で `flashSD()` を実行。<br>2. **UART 送信**: 共有変数にある URM/TSD20 の高度データを 5 項目カンマ区切りで Bico へ送信。<br>3. **ステータス表示・監視**: SD のアクティブ状態に応じて NeoPixel を点灯（緑/赤）させ、Core 1 が生存していれば Watchdog を更新。 |
| **Core 1** | `setup1()`<br>`loop1()` | **100Hz (10ms)**<br>`core1_timer` | 1. **URM37 制御**: `trigger_echo()` を 10Hz で発火し、ハードウェア割り込み (`echo_isr`) で得たパルス時間を元に超音波高度を計算 (`update_echo_distance`)。<br>2. **TSD20 LiDAR パース**: 460.8kbps の専用 UART から送られるバイナリフレームを受信し、チェックサムを検証して LiDAR 高度を算出 (`read_tsd20`)。<br>3. **生存アピール**: 毎ループ `core1_alive = true` を立てる。 |
