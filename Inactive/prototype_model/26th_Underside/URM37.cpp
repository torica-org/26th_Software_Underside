/*
超音波センサの動作を記述するファイル (ノイズ対策・ノンブロッキング版)
*/

#include <Arduino.h>
#include "URM37.h"

#include "parameters.h"
#include "Underside_config.h"

volatile unsigned long echo_start_time = 0;
volatile unsigned long echo_low_time = 0;
volatile bool echo_data_ready = false;

unsigned long echo_trigger_time = 0;
bool waiting_for_echo = false;

// 前方宣言（関数を切り替えるため）
void echo_falling_isr();
void echo_rising_isr();

// ─── ① 立ち下がり（通常HIGH -> 計測開始LOW）をキャッチ ───
void echo_falling_isr() {
  echo_start_time = micros();
  // 開始を捉えたら、次は立ち上がり（計測終了のRISING）を待つように切り替える
  attachInterrupt(digitalPinToInterrupt(URECHO), echo_rising_isr, RISING);
}

// ─── ② 立ち上がり（計測終了LOW -> 通常HIGH）をキャッチ ───
void echo_rising_isr() {
  if (echo_start_time != 0) {
    echo_low_time = micros() - echo_start_time;
    echo_data_ready = true;
  }
  // 終了を捉えたら、次はまた立ち下がり（FALLING）を待つ
  attachInterrupt(digitalPinToInterrupt(URECHO), echo_falling_isr, FALLING);
}

void init_echo() {
  pinMode(URTRIG, OUTPUT);
  digitalWrite(URTRIG, HIGH);

  pinMode(URECHO, INPUT_PULLUP);
  
  // 最初は立ち下がり（計測開始）を待つ
  attachInterrupt(digitalPinToInterrupt(URECHO), echo_falling_isr, FALLING);
}

void trigger_echo() {
  echo_data_ready = false;
  echo_start_time = 0;
  
  waiting_for_echo = true;
  echo_trigger_time = millis();

  // トリガーを引く前に、確実にFALLINGから始まるようにリセット
  attachInterrupt(digitalPinToInterrupt(URECHO), echo_falling_isr, FALLING);

  // URM37のトリガーパルス生成
  digitalWrite(URTRIG, LOW);
  delayMicroseconds(10);
  digitalWrite(URTRIG, HIGH);
}

void update_echo_distance() {
  if (echo_data_ready) {
    echo_data_ready = false;
    waiting_for_echo = false;

    float urm_altitude_m;
    unsigned long low_time = echo_low_time;

    if (low_time <= 40000) {                          // 50μs * 800cm
      unsigned int DistanceMeasured = low_time / 50;  // 50μsあたり1cm
      urm_altitude_m = (float)DistanceMeasured / 100.0;
    } else {
      urm_altitude_m = 10.0;
    }

    data_under_urm_altitude_m = urm_altitude_m;
  }
  else if (waiting_for_echo && (millis() - echo_trigger_time > 60)) {
    waiting_for_echo = false;
    data_under_urm_altitude_m = 10.0;
  }
}