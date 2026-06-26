/*-----------------------
このファイルの役割：Under用SD用関数
更新日時：2026-03-11 17:09

------------------------*/

#include "SD_Under.h"

//ピン配置定義ファイルを読み込む
#include "Underside_config.h"
#include "parameters.h"
#include <SPI.h>


TORICA_SD sd(true); //インスタンス化

char SD_BUF[2048]; //SD書き込み用バッファ


//SD初期化コード
void initSD(){

    SPI.setSCK(SD_SCK);
    SPI.setTX(SD_MOSI);
    SPI.setRX(SD_MISO);
    SPI.begin();
    
    if (!sd.begin(SD_CS)) {
    Serial.println("SD initialization failed!");
  } else {
    Serial.println("SD initialization done.");
  }
}



//TORICA_SD内のバッファに保存
void save_SD_BUF(char* BUF){
    memset(SD_BUF, 0, sizeof(SD_BUF)); //SD_BUFを0で初期化

    sd.add_str(BUF);
}

void flashSD(){
    sd.flash(); //SD書き込み
}


//SDが生きてるか調べる関数
bool SDisActive(){
    if (sd.SDisActive == true){
        return true;
    } else {
        return false;
    }
}