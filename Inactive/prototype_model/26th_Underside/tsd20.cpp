/* RP2040用TSD20ドライバ */

#include <SerialPIO.h>
#include "tsd20.h"
#include "parameters.h"

#include "Underside_config.h"

SerialPIO tsd20_Serial(tsd20_RX, tsd20_TX, 256);

// https://akizukidenshi.com/goodsaffix/TSD20%20user%20manual.pdf

/*
UARTで送られてくるデータの構造
Byte    内容           例
1     Frame Header    0x5C（固定）
2     Distance_L      0x02
3     Distance_H      0x11
4      Checksum       0xEC

距離データはlittle-endianのため下位バイト→上位バイトの順に送られてくる．
0x02,0x11の順に来た場合，距離は0x1102 = 4354mmとなる．

チェックサムの仕組み：0x02,0x11の場合
0x02+0x11 = 0x13
ビット反転(NOT)→ ~0x13=0xFF - 0x13 = 0xEC 

*/

// Checksum計算用関数
uint8_t calcChecksum(uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  for (int i = 0; i < len; i++) sum += data[i];
  return ~sum;
}

void init_tsd20(){
    // 読み取り用Serial設定
    tsd20_Serial.begin(460800);
}



void read_tsd20() {
  if (tsd20_Serial.available() >= 4) {
    if (tsd20_Serial.read() == 0x5C) {  // Frame Header読み取り
      uint8_t dist_L = tsd20_Serial.read();
      uint8_t dist_H = tsd20_Serial.read();
      uint8_t recv_chk = tsd20_Serial.read();

      uint8_t data_for_chk[2] = {dist_L, dist_H};
      uint8_t calc_chk = calcChecksum(data_for_chk, 2);

      if (recv_chk == calc_chk) {
        uint16_t distance_mm = (dist_H << 8) | dist_L;  // Little-endianなので
        if (distance_mm == 5000) {

            data_under_tsd20_altitude_m = 0.00f; // エラーなので0.00mを返す
            #ifdef DEBUG_MODE
            Serial.println("Out of range");
            #endif DEBUG_MODE
        
        } else {

            data_under_tsd20_altitude_m = distance_mm / 1000.0f; // mmからmに変換
            #ifdef DEBUG_MODE
            Serial.print("Distance: ");
            Serial.print(distance_mm);
            Serial.println(" mm");
            #endif DEBUG_MODE
        }
      } else {
        data_under_tsd20_altitude_m = 0.00f;
        #ifdef DEBUG_MODE
        Serial.println("Checksum error");
        #endif
      }
    }
  }
}