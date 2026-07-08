#include <Arduino.h>
#include "UARTHelper_Under.h"
#include "SD_Under.h"


void initUART () {

  // UART初期化（<-まだ通信の開始処理はされていない）
  Serial1.setFIFOSize(1024); // バッファ(受信したデータの一時保管場所)サイズ指定(1024byte)

  // パラメータ設定とともに通信を開始
  // ICS通信の仕様に合わせ，`SERIAL_8E1`としている．
  // `8`:データビットの長さ
  // `E`:偶数パリティ(`N`:パリティなし，`O`:奇数パリティ)
  // `1`:ストップビット(データフレームの終わりを示すビット)の長さ
  // デフォルトでは`SERIAL_8N1`となっている．
  
  Serial1.begin(460800, SERIAL_8E1);
  
  Serial.begin(115200); // デバッグ用にパリティはいらないかな...ってか使えない気がする
  Serial.print("loading...\n\n");

  while(Serial1.available() == true){
    Serial1.read();
  }
}

char trans_buff[256]; // 送信する文字列を保存するためのバッファ

void transmitLog () {
  sprintf(trans_buff, "%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", data_under_bmp_pressure_hPa, data_under_bmp_temperature_deg, data_under_bmp_altitude_m, data_under_urm_altitude_m, data_under_tsd20_altitude_m, takeoff);
  Serial1.print(trans_buff);

  // 野田TF緊急措置．とりあえずログとる
  // このモードは機体下基板単体動作でSD書き込みまで行う．
  // save_SD_BUF(trans_buff);
  // flashSD();
}


char readUART_BUF[512] = {0};
const int readUART_BUF_SIZE = 512;

void receiveLog (){
  // この関数はUARTを受信してreadUART_BUFにデータを格納するところまでを行う

  int read_length = Serial1.available();
  if (read_length >= readUART_BUF_SIZE -1){ // バッファオーバーフローを防ぐため
    read_length = readUART_BUF_SIZE -1; // '-1'はヌル文字'\0'のため
  }
  Serial1.readBytes(readUART_BUF, read_length); // バッファに受信したデータを長さを指定して格納
  readUART_BUF[read_length] ='\0'; // 受信用バッファ配列の最後は確実にヌル文字で終わらせる

}


bool receive_available(){
  return Serial1.available();
}