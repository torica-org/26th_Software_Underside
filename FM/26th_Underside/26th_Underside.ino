/* read_dps(), init_dps()をread_bmp(), init_bmp()に書き換えること */


#include "parameters.h"
#include "Underside_config.h"
#include "UARTHelper_Under.h"
// #include "DPS310.h" //25代基板でテストする用
#include "BMP3xx.h" // 本番はこれを使う
#include "calculate_altitude.h"

#include "LED.h"
#include "NeoPixel.h"
#include "SD_Under.h"
#include "URM37.h"
#include "tsd20.h"

#include <Wire.h>

// WatchDog用
#include "hardware/watchdog.h"
volatile bool core1_alive; // core1の生存確認用フラグ


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

  // init_DPS310(); // 25代基板でテストする用
  // BMP3XX_init(); // 本番はこれを使う

  init_NeoPixel();

  init_echo();

  init_tsd20();

  initSD();

  init_intLED();

  
  Serial.println("Setup1 Done.");

  // for (int i = 0; i<=3; i++){
  //   write_intLED(1);
  //   delay(500);
  //   write_intLED(0);
  //   delay(500);
  // }
  write_intLED(0);

  // ハードウェアタイマー起動
  add_repeating_timer_ms(-10, core0_timer_callback, NULL, &core0_timer);

  watchdog_enable(2000, 1); // WatchDogを有効化．
  // 2000ms(=2s)経っても反応がない場合，システムが暴走したとみなして強制再起動

  Serial.println("Setup Done.");
}

void setup1() {
  // ハードウェアタイマーの設定はコアごとに
  add_repeating_timer_ms(-10, core1_timer_callback, NULL, &core1_timer);
}


void loop() {
  if (core0_timer_triggered == true /* 100Hz用フラグが立っていたら */) {
    core0_timer_triggered = false; // フラグを戻す

    //UART受信
    while (receive_available() == true) {
      Lightup_NeoPixel(RED);
      receiveLog();

      save_SD_BUF(readUART_BUF);

      NeoPixel_off();
    }


    //気圧・温度取得 for DPS310
    // if (DPS310_is_OK() == true) {
    //   Lightup_NeoPixel(BLUE);
    //   read_dps();
    //   calculate_bmp_altitude(); // 気圧高度の計算．DPS310の値を使うからDPS310の値取得後に計算
    //   NeoPixel_off();
    // }

    // 気圧・温度高度取得 for BMP3XX
    /*----
    Lightup_NeoPixel(BLUE);
    read_bmp_under();
    calculate_bmp_altitude(); // 気圧高度の計算．BMPの値を使うからBMPの値取得後に計算
    NeoPixel_off();
    ---*/


    // SD書き込み
    if(SDisActive() == true){
      Lightup_NeoPixel(GREEN);
      flashSD();
      NeoPixel_off();
    } else {
      Lightup_NeoPixel(RED);
      // Serial.println("SD is not Active.");
      NeoPixel_off();
    }
    

    //UART送信
    transmitLog();

    //for debug
    // Serial.print("URM altitude: ");
    // Serial.println(data_under_urm_altitude_m);
    // Serial.print("Pressure: ");
    // Serial.print(data_under_bmp_pressure_hPa);
    // Serial.print("  Temperature: ");
    // Serial.println(data_under_bmp_temperature_deg);


    //WatchDogでCore1の生存確認
    if (core1_alive == true) {
      watchdog_update(); //core1の生存を確認出来たらWatchDogTimerをアップデート

      core1_alive = false; //core1の生存フラグをfalseに戻す
    }
  }
}


void loop1() {
  if (core1_timer_triggered == true) {
    core1_timer_triggered = false; // タイマーのフラグを下す

    write_intLED(HIGH);
    
    // 100Hzで毎回受信チェック
    update_echo_distance();

    static int echo_counter = 0;
    echo_counter++;
    if (echo_counter >= 10) { // 100Hzで10回ごと（＝10Hz）
      echo_counter = 0;
      trigger_echo();

      
      // Serial.print("URM altitude: ");
      // Serial.println(data_under_urm_altitude_m);
    }

    read_tsd20();

    // TSD20の値をもとに離陸判定
    if (takeoff == false) {
        takeoff = is_takeoff();
    }

    write_intLED(LOW);

    core1_alive = true; 
  }
}
