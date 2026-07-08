#include "Echo.h"

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(9600);

  //超音波センサを初期化
  echo_init();

}

void loop() {
  // put your main code here, to run repeatedly:
  
  read_echo();

  //シリアルで超音波高度を吐き出す
  Serial.print("Altitude: ");
  Serial.print(data_under_urm_altitude_m);
  Serial.println(" m");
  delay(100);

}
