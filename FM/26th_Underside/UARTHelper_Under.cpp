#include <Arduino.h>
#include "UARTHelper_Under.h"
#include "SD_Under.h"


void initUART () {

  // UART初期化（<-まだ通信の開始処理はされていない）
  Serial1.setFIFOSize(2048); // バッファ(受信したデータの一時保管場所)サイズ指定(1024byte)

  // パラメータ設定とともに通信を開始
  // ICS通信の仕様に合わせ，`SERIAL_8E1`としている．
  // `8`:データビットの長さ
  // `E`:偶数パリティ(`N`:パリティなし，`O`:奇数パリティ)
  // `1`:ストップビット(データフレームの終わりを示すビット)の長さ
  // デフォルトでは`SERIAL_8N1`となっている．
  
  Serial1.begin(460800, SERIAL_8E1);
  
  Serial.begin(115200); // デバッグ用にパリティはいらないかな...ってか使えない気がする
  Serial.print("loading...\n\n");

  while(Serial1.available() > 0){
    Serial1.read();
  }
}

char trans_buff[256]; // 送信する文字列を保存するためのバッファ

void transmitLog () {
  memset(trans_buff, 0, sizeof(trans_buff)); // 配列を0で初期化
  sprintf(trans_buff, "%.2f,%.2f,%.2f,%.2f,%.2f\n", data_under_bmp_pressure_hPa, data_under_bmp_temperature_deg, data_under_bmp_altitude_m, data_under_urm_altitude_m, data_under_tsd20_altitude_m);
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
  int actual_read = Serial1.readBytes(readUART_BUF, read_length); // バッファに受信したデータを長さを指定して格納
  readUART_BUF[actual_read] ='\0'; // 受信用バッファ配列の最後は確実にヌル文字で終わらせる
}



// もしこれを使う場合でもBico側のUARTをいじる必要はない．
// Underに関するものは受信してもデータを破棄している．
// void receiveLog () {
//   static unsigned long int last_Bico_time_ms = 0;
//   int readnum_Bico = Bico_UART.readUART();
//   const int Bico_data_num = ; //正常な場合のデータ受信数

//   if (readnum_Bico == Bico_data_num){
//     last_Bico_time_ms = millis();

//     // 受信データを格納

//     // 1回目の受信　計13個
//     time_ms = Bico_UART.UART_data[0];
//     takeoff = Bico_UART.UART_data[1];
//     urm_is_reliable = Bico_UART.UART_data[2];
//     data_air_gps_hour = Bico_UART.UART_data[3];
//     data_air_gps_minute = Bico_UART.UART_data[4];
//     data_air_gps_second = Bico_UART.UART_data[5];
//     data_air_gps_centisecond = Bico_UART.UART_data[6];
//     data_air_gps_latitude_deg = Bico_UART.UART_data[7];
//     data_air_gps_longitude_deg = Bico_UART.UART_data[8];
//     data_air_gps_altitude_m = Bico_UART.UART_data[9];
//     data_air_gps_ground_speed_ms = Bico_UART.UART_data[10];
//     data_air_gps_heading_deg = Bico_UART.UART_data[11];
//     data_air_gps_satellites = Bico_UART.UART_data[12];

//     // 2回目の受信　計11個
//     filtered_bmp_altitude_m = Bico_UART.UART_data[13];
//     filtered_urm_altitude_m = Bico_UART.UART_data[14];
//     filtered_airspeed_ms = Bico_UART.UART_data[15];
//     data_air_bmp_pressure_hPa = Bico_UART.UART_data[16];
//     data_air_bmp_temperature_deg = Bico_UART.UART_data[17];
//     data_air_bmp_altitude_m = Bico_UART.UART_data[18];
//     data_air_sdp_differentialPressure_Pa = Bico_UART.UART_data[19];
//     data_air_sdp_airspeed_ms = Bico_UART.UART_data[20];
//     data_air_AoA_angle_deg = Bico_UART.UART_data[21];
//     data_air_AoS_angle_deg = Bico_UART.UART_data[22];
//     data_ics_angle = Bico_UART.UART_data[23];

//     // 3回目の受信
//     fslg_is_alive = Bico_UART.UART_data[24];
//     data_fslg_bno_qw = Bico_UART.UART_data[25];
//     data_fslg_bno_qx = Bico_UART.UART_data[26];
//     data_fslg_bno_qy = Bico_UART.UART_data[27];
//     data_fslg_bno_qz = Bico_UART.UART_data[28];
//     data_fslg_bno_roll = Bico_UART.UART_data[29];
//     data_fslg_bno_pitch = Bico_UART.UART_data[30];
//     data_fslg_bno_yaw = Bico_UART.UART_data[31];
//     data_fslg_lsm_roll = Bico_UART.UART_data[32];
//     data_fslg_lsm_pitch = Bico_UART.UART_data[33];
//     data_fslg_lsm_yaw = Bico_UART.UART_data[34];
//     data_fslg_bmp_pressure_hPa = Bico_UART.UART_data[35];
//     data_fslg_bmp_temperature_deg = Bico_UART.UART_data[36];
//     data_fslg_bmp_altitude_m = Bico_UART.UART_data[37];

//     // 4回目の受信
//     data_fslg_bno_accx_mss = Bico_UART.UART_data[38];
//     data_fslg_bno_accy_mss = Bico_UART.UART_data[39];
//     data_fslg_bno_accz_mss = Bico_UART.UART_data[40];
//     data_fslg_lsm_accx_mss = Bico_UART.UART_data[41];
//     data_fslg_lsm_accy_mss = Bico_UART.UART_data[42];
//     data_fslg_lsm_accz_mss = Bico_UART.UART_data[43];
//     data_fslg_bno_cal_system = Bico_UART.UART_data[44];
//     data_fslg_bno_cal_gyro = Bico_UART.UART_data[45];
//     data_fslg_bno_cal_accel = Bico_UART.UART_data[46];
//     data_fslg_bno_cal_mag = Bico_UART.UART_data[47];
//     under_is_alive = Bico_UART.UART_data[48];

//     /* Underで測定したデータがUnder -> Bico -> Unde という経路でデータがやってくる．データを格納するグローバル変数を書き換えられてしまうので，コメントアウトしておく */

//     // data_under_bmp_pressure_hPa = Bico_UART.UART_data[49];
//     // data_under_bmp_temperature_deg = Bico_UART.UART_data[50];
//     // data_under_bmp_altitude_m = Bico_UART.UART_data[51];
//     // data_under_urm_altitude_m = Bico_UART.UART_data[52];
//     // data_under_tsd20_altitude_m = Bico_UART.UART_data[53];


//   }
// }




bool receive_available(){
  return Serial1.available();
}