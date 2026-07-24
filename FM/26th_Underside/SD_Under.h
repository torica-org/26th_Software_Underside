/*-----------------------
このファイルの役割：Under用SD用関数

------------------------*/

#pragma once

#include <SD.h>
#include <TORICA_SD.h>

#include "parameters.h"

// ログ1行保持用構造体 (512バイト)
struct SDLogEntry {
    char data[512];
};

void initSD();

// キュー初期化関数
void initSDQueue();

// Core0から呼び出す非同期キュー追加関数
bool save_SD_Queue(const char* BUF);

// Core1から呼び出すキュー処理・SD書き込み関数
void process_SD_Queue();

extern void save_SD_BUF(char* BUF);

extern void flashSD();

bool SDisActive();