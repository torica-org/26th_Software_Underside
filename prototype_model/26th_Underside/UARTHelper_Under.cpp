#include <Arduino.h>
#include "UARTHelper_Under.h"

//SerialIN -> エアデータのBicoから送られてくる
//SerialOUT -> 機体下電装からエアデータのBicoに向けて送る
#define SerialIN Serial1
#define SerialOUT Serial1


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

  while(SerialIN.available() == true){
    SerialIN.read();
  }
}



char trans_buff[256]; // 送信する文字列を保存するためのバッファ

void transmitLog () {
  sprintf(trans_buff, "%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", data_under_bmp_pressure_hPa, data_under_bmp_temperature_deg, data_under_bmp_altitude_m, data_under_urm_altitude_m, data_under_tsd20_altitude_m, takeoff);
  SerialOUT.print(trans_buff);
}


char readUART_BUF[256] = {0};
const int readUART_BUF_SIZE = 256;

void receiveLog (){
  //この関数はUARTを受信してreadUART_BUFにデータを格納するところまでを行う

  int read_length = SerialIN.available();
  if (read_length >= readUART_BUF_SIZE -1){ //バッファオーバーフローを防ぐため
    read_length = readUART_BUF_SIZE -1; //'-1'はヌル文字'\0'のため
  }
  SerialIN.readBytes(readUART_BUF, read_length); // バッファに受信したデータを長さを指定して格納
  readUART_BUF[read_length] ='\0'; //受信用バッファ配列の最後は確実にヌル文字で終わらせる

}


bool receive_available(){
  return SerialIN.available();
}