/*

更新日時：2026-03-11 17:09
DPS310用コード．25代機体下基板テスト用コード．


*/


#include "DPS310.h"
#include <Arduino.h>

#include <Wire.h> 
#include <Adafruit_DPS310.h>

#include "parameters.h"

Adafruit_DPS310 dps;
sensors_event_t temp_event, pressure_event;

void init_DPS310() {
    while(dps.begin_I2C() == false) {
        Serial.println("DPS310 not found.");
        delay(100);
    }
    Serial.println("DPS310 OK");
    dps.configurePressure(DPS310_32HZ, DPS310_16SAMPLES); // 気圧取得時の設定
    dps.configureTemperature(DPS310_32HZ, DPS310_2SAMPLES); // 気温取得時の設定

}

void read_dps() {
    dps.getEvents(&temp_event, &pressure_event);

    data_under_bmp_pressure_hPa = pressure_event.pressure;
    data_under_bmp_temperature_deg = temp_event.temperature;
}

bool DPS310_is_OK() {
    if ( (dps.temperatureAvailable() && dps.pressureAvailable() ) == true) {
        return true;
    } else {
        return false;
    }
}