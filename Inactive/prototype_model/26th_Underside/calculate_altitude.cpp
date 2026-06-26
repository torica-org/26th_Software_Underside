#include "calculate_altitude.h"
#include <TORICA_MoveAve.h> // TSD20移動平均用


// 基本的にフィルタリングはエアデータで行うので機体下では行わない．
// TSD20の値は離陸判定で使うため，移動平均をとる．
TORICA_MoveAve<5> filtered_tsd20_altitude_m(0);   



void calculate_bmp_altitude() {
    
    // 気圧高度計算
    if (data_under_bmp_pressure_hPa == 0.0f && data_under_bmp_temperature_deg == 0.0f) { // 気圧が0.0hPaのときはセンサー値が取得できていないとみなす
        data_under_bmp_altitude_m = 0.0f;
    } else {
        data_under_bmp_altitude_m = (powf(1013.25 / data_under_bmp_pressure_hPa, 1 / 5.257) - 1) * (data_under_bmp_temperature_deg + 273.15) / 0.0065;
    }
}

bool is_takeoff(){
    filtered_tsd20_altitude_m.add(data_under_tsd20_altitude_m);
    if (filtered_tsd20_altitude_m.get() > 3.0f) { // TSD20のフィルタリング済み高度が3.0m以上なら離陸と判定
        return true;
    } else {
        return false;
    }
}