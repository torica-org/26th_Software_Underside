#include "parameters.h"
#include "Underside_config.h"
#include "UARTHelper_Under.h"
#include "BMP3xx.h"
#include "calculate_altitude.h"

#include "LED.h"
#include "NeoPixel.h"
#include "SD_Under.h"
#include "URM37.h"
#include "tsd20.h"

#include <Wire.h>

// WatchDog用
#include "hardware/watchdog.h"
volatile bool core1_alive;  // core1の生存確認用フラグ

// ハードウェアタイマー設定
#include "pico/stdlib.h"
struct repeating_timer core0_timer;
struct repeating_timer core1_timer;

// 100Hz実行用フラグ
volatile bool core0_timer_triggered = false;
volatile bool core1_timer_triggered = false;

// 100Hzごとにフラグを立てる
/* 100Hzごとにフラグを立て，loop()内でフラグが立っているかどうか判別．立っていたら10ms経過後ということなので処理実行． */
bool core0_timer_callback(struct repeating_timer *t) {
  core0_timer_triggered = true;
  return true;
}

bool core1_timer_callback(struct repeating_timer *t) {
  core1_timer_triggered = true;
  return true;
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(460800);  //DEBUG用シリアル

  initUART();

  Wire.setSDA(Under_SDA);
  Wire.setSCL(Under_SCL);
  Wire.begin();
  Wire.setClock(400000);

  init_NeoPixel();

  BMP3XX_init();
  init_intLED();

  init_echo();
  init_tsd20();
  initSD();
  initSDQueue(); // SD用キューの初期化

  // ハードウェアタイマー起動
  add_repeating_timer_ms(-10, core0_timer_callback, NULL, &core0_timer);

  watchdog_enable(2000, 1);  // WatchDogを有効化．
  // 2000ms(=2s)経っても反応がない場合，システムが暴走したとみなして強制再起動

  // セットアップ終了を示す合図
  write_intLED(0);
  Serial.println("Setup Done.");
}

void setup1() {
  // ハードウェアタイマーの設定はコアごとに
  add_repeating_timer_ms(-10, core1_timer_callback, NULL, &core1_timer);
}


void loop() {
  if (core0_timer_triggered == true /* 100Hz用フラグが立っていたら */) {
    core0_timer_triggered = false;  // フラグを戻す

    // UART受信
    while (receive_available() == true) {
      // Lightup_NeoPixel(RED);
      write_intLED(HIGH);
      receiveLog();

      save_SD_Queue(readUART_BUF); // キューへ非同期保存 (Core0はブロックしない)

      // NeoPixel_off();
      write_intLED(LOW);
    }


    // 気圧・温度高度取得 for BMP3XX
    // Lightup_NeoPixel(BLUE);
    read_bmp_under();
    calculate_bmp_altitude(); // 気圧高度の計算．BMPの値を使うからBMPの値取得後に計算
    // NeoPixel_off();

    // SDステータス表示 (書き込み処理自体はCore1に移壊)
    if (SDisActive() == true) {
      Lightup_NeoPixel(GREEN);
      NeoPixel_off();
    } else {
      Lightup_NeoPixel(RED);
      // Serial.println("SD is not Active.");
      NeoPixel_off();
    }


    // UART送信
    transmitLog();

    // WatchDogでCore1の生存確認
    if (core1_alive == true) {
      watchdog_update();  // core1の生存を確認出来たらWatchDogTimerをアップデート

      core1_alive = false;  // core1の生存フラグをfalseに戻す
    }
  }
}


void loop1() {
  if (core1_timer_triggered == true) {
    core1_timer_triggered = false;  // タイマーのフラグを下す

    // 100Hzで毎回受信チェック
    update_echo_distance();

    static int echo_counter = 0;
    echo_counter++;
    if (echo_counter >= 10) {  // 100Hzで10回ごと（＝10Hz）
      echo_counter = 0;
      trigger_echo();
    }

    read_tsd20();

    // Core1でSDキューからデータを取り出し、SDカードに定期書き込み
    process_SD_Queue();

    core1_alive = true;
  }
}