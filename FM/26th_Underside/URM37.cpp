/*
超音波センサの動作を記述するファイル (ノイズ対策・ノンブロッキング版・バグ修正済)
*/

#include <Arduino.h>
#include "URM37.h"

#include "parameters.h"
#include "Underside_config.h"

// 割り込みで使用する変数は volatile
volatile unsigned long echo_start_time = 0;
volatile unsigned long echo_low_time = 0;
volatile bool echo_data_ready = false;

unsigned long echo_trigger_time = 0;
bool waiting_for_echo = false;

// ─── CHANGE割り込みハンドラ ───
void echo_isr() {
  int state = digitalRead(URECHO);
  if (state == LOW) {
    // 立ち下がり（計測開始）
    echo_start_time = micros();
  } else {
    // 立ち上がり（計測終了）
    if (echo_start_time != 0) {
      echo_low_time = micros() - echo_start_time;
      echo_data_ready = true;
      echo_start_time = 0; // チャタリングによる多重トリガー防止のため即リセット
    }
  }
}

void init_echo() {
  pinMode(URTRIG, OUTPUT);
  digitalWrite(URTRIG, HIGH); // URM37は基本HIGH

  pinMode(URECHO, INPUT_PULLUP);
  
  // 立ち上がり・立ち下がりの両エッジをCHANGE割り込みで監視
  attachInterrupt(digitalPinToInterrupt(URECHO), echo_isr, CHANGE);
}

void trigger_echo() {
  // トリガー前に一時的に割り込みを禁止し，状態変数を完全にクリーンにする
  noInterrupts();
  echo_data_ready = false;
  echo_start_time = 0;
  echo_low_time = 0;
  interrupts();
  
  waiting_for_echo = true;
  echo_trigger_time = millis();

  // URM37のトリガーパルス生成
  // 仕様に則り，確実に動作させるためにLOW時間を50μsに修正
  digitalWrite(URTRIG, LOW);
  delayMicroseconds(50);
  digitalWrite(URTRIG, HIGH);
}

void update_echo_distance() {
  bool data_ready = false;
  unsigned long low_time = 0;

  // 割り込みデータを安全に取り出す
  noInterrupts();
  if (echo_data_ready) {
    data_ready = true;
    low_time = echo_low_time;
    echo_data_ready = false; // フラグを消費
  }
  interrupts();

  if (data_ready) {
    waiting_for_echo = false;
    float urm_altitude_m;

    // 4cm以上8m以下のみを対象とする
    if (low_time >= 200 && low_time <= 40000) {                          // 200μs(4cm) 〜 40000μs(800cm)
      unsigned int DistanceMeasured = low_time / 50;  // 50μsあたり1cm
      urm_altitude_m = (float)DistanceMeasured / 100.0;
    } else {
      // 範囲外は10.0mにする
      urm_altitude_m = 10.0;
    }

    data_under_urm_altitude_m = urm_altitude_m;
  }
  // 60ms経過でのタイムアウト処理
  else if (waiting_for_echo && (millis() - echo_trigger_time > 60)) {
    // 割り込みをクリア
    noInterrupts();
    waiting_for_echo = false;
    echo_data_ready = false;
    echo_start_time = 0;
    echo_low_time = 0;
    interrupts();

    data_under_urm_altitude_m = 10.0;
  }
}