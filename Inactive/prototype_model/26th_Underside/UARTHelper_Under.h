#pragma once

#include "parameters.h"


extern char readUART_BUF[512];


// 関数のプロトタイプ宣言
void initUART();
void transmitLog();
void receiveLog();
bool receive_available();