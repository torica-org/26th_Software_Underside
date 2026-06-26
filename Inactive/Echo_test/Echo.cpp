/*

超音波センサの動作を記述するファイル

*/

#include <Arduino.h>
#include "Echo.h"

volatile float data_under_urm_altitude_m = 0.0;

const int URECHO = 26;
const int URTRIG = 27;

void echo_init() {

  pinMode(URTRIG, OUTPUT);
  pinMode(URECHO, INPUT);
}

void read_echo() {
  float urm_altitude_m;
  //超音波センサの読み取りコードを書く
  digitalWrite(URTRIG, LOW);
  delay(1);
  //超音波を出す
  digitalWrite(URTRIG, HIGH);

  unsigned long LowLevelTime = pulseIn(URECHO, LOW);    //URM37は通常時→HIGH, 反射待ち時間→LOW
  if (LowLevelTime <= 40000) {                          //50μs*800cm
    unsigned int DistanceMeasured = LowLevelTime / 50;  // every 50us low level stands for 1cm
    urm_altitude_m = (float)DistanceMeasured / 100.0;
  } else {
    urm_altitude_m = 10.0;
  }

//ここに超音波高度を格納するようにする
data_under_urm_altitude_m = urm_altitude_m;
}