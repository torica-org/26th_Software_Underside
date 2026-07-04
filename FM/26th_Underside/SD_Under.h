/*-----------------------
このファイルの役割：Under用SD用関数

------------------------*/

#pragma once

#include <SD.h>
#include <TORICA_SD.h>

#include "parameters.h"

void initSD();

extern void save_SD_BUF(char* BUF);

extern void flashSD();

bool SDisActive();