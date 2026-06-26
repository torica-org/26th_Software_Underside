#include <Arduino.h>  // Arduinoの基本的な関数を使えるようにする
#include "Underside_config.h"

//ピン定義

//SPI(SD)用
const int SD_CS = 28;
const int SD_MOSI = 3;
const int SD_MISO = 4;
const int SD_SCK = 2;

//超音波
const int URECHO = 26;
const int URTRIG = 27;

//i2c
const int Under_SDA = 6;
const int Under_SCL = 7;

//tsd20用
const int tsd20_RX = D2;
const int tsd20_TX = D3;


//Xiao rp2040内蔵LED用
// const int LED_R = 17;
// const int LED_G = 16;
// const int LED_B = 25;