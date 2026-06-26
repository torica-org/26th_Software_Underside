/*
更新日時：2026-03-11 17:09
*/


#pragma once

void init_NeoPixel();


// 色の定数定義（Lightup_NeoPixel()用）
#define RED 0
#define GREEN 1
#define BLUE 2

void Lightup_NeoPixel(int color);

void NeoPixel_off();