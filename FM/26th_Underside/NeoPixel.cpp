/*
更新日時：2026-03-11 17:09
*/


#include "NeoPixel.h"
#include <Arduino.h>

#include <Adafruit_NeoPixel.h> // XIAO RP2040についているNeoPixelを虹色に光らせるために使用

#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800); //PIN_NEOPIXELは予めライブラリ側で用意されている定数

void init_NeoPixel(){
    pinMode(NEOPIXEL_POWER, OUTPUT); //NeoPixelの電源投入
    digitalWrite(NEOPIXEL_POWER, HIGH); //NEOPIXEL_POWERもあらかじめ用意されている

    pixels.begin();
}


void Lightup_NeoPixel(int color)
{
    pixels.clear(); // まずはNeoPixelを消灯
    switch(color) {
        case RED:  
            // 赤色で点灯
            pixels.setPixelColor(0, pixels.Color(255, 0, 0));
            break;
        case GREEN:
            // 緑色で点灯
            pixels.setPixelColor(0, pixels.Color(0, 255, 0));
            break;
        case BLUE:
            // 青色で点灯
            pixels.setPixelColor(0, pixels.Color(0, 0, 255));
            break;
        default:
            // デフォルトは白色
            pixels.setPixelColor(0, pixels.Color(255, 255, 255));
            break;
    }

    pixels.show(); // 実際にLEDに色を反映
}


void NeoPixel_off(){
    pixels.clear();
    pixels.show();
}
