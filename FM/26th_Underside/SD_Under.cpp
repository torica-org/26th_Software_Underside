/*---------------------------------
このファイルの役割：Under用SD用関数
更新日時：2026-03-11 17:09
---------------------------------*/

#define private public // これによりTORICA_SD内のprivate変数内に無理やりアクセス
#include "SD_Under.h"
#undef private

// ピン配置定義ファイルを読み込む
#include "Underside_config.h"
#include "parameters.h"
#include <SPI.h>
#include <pico/util/queue.h>

TORICA_SD sd; // 引数なしでインスタンス化

char SD_BUF[2048]; //SD書き込み用バッファ

static queue_t sd_queue;
static const uint SD_QUEUE_CAPACITY = 64; // 64要素 * 512バイト = 32KB

//SD初期化コード
void initSD(){

    SPI.setSCK(SD_SCK);
    SPI.setTX(SD_MOSI);
    SPI.setRX(SD_MISO);
    SPI.begin();
    // SPI.setClock(16000000); // SPIクロックを16MHzに設定
    
    if (!sd.begin(SD_CS)) {
    Serial.println("SD initialization failed!");
  } else {
    Serial.println("SD initialization done.");
  }
}

// キュー初期化
void initSDQueue() {
    queue_init(&sd_queue, sizeof(SDLogEntry), SD_QUEUE_CAPACITY);
}

// Core0から呼び出す非同期キュー追加関数
bool save_SD_Queue(const char* BUF) {
    if (BUF == NULL || BUF[0] == '\0') return false;

    SDLogEntry entry;
    strncpy(entry.data, BUF, sizeof(entry.data) - 1);
    entry.data[sizeof(entry.data) - 1] = '\0';

    bool success = queue_try_add(&sd_queue, &entry);
    if (!success) {
        Serial.println("[Warning] SD Queue Overflow!");
    }
    return success;
}

// Core1から呼び出すキュー処理・SD書き込み関数
void process_SD_Queue() {
    if (!sd.SDisActive) {
        // SDがアクティブでない場合、3秒ごとに再初期化を試みる
        static uint32_t last_retry_time = 0;
        uint32_t now = millis();
        if (now - last_retry_time >= 3000) {
            last_retry_time = now;
            if (sd.begin(SD_CS)) {
                Serial.println("SD re-initialization done.");
            } else {
                Serial.println("SD re-initialization failed!");
            }
        }
        return;
    }

    SDLogEntry entry;
    bool had_data = false;

    // キューに溜まっている全データを順次取り出してSDバッファに追加
    while (queue_try_remove(&sd_queue, &entry)) {
        sd.add_str(entry.data);
        had_data = true;
    }

    // 250ms (4Hz) ごとにSDカードへフラッシュ書き込み
    static uint32_t last_flash_time = 0;
    uint32_t now = millis();
    if (had_data && (now - last_flash_time >= 250)) {
        flashSD();
        last_flash_time = now;
    }
}

// TORICA_SD内のバッファに保存
void save_SD_BUF(char* BUF){
    memset(SD_BUF, 0, sizeof(SD_BUF)); // SD_BUFを0で初期化
    sd.add_str(BUF);
}

void flashSD(){
    sd.flash(); //SD書き込み
}

//SDが生きてるか調べる関数
bool SDisActive(){
    return sd.SDisActive;
}


void flashHeader() {
  // この関数は`setup()`内なのでブロッキング関数（処理の流れが止まる関数）であっても構わない
  if (sd.SDisActive == true) {
    const char *str[3];

    for (int i = 0; i < 4 /* case0~3まで実行 */; i++) {

      switch (i) {
        case 0:  // 13個
          {
            str[0] = "time_ms,takeoff,urm_is_reliable,data_air_gps_hour,";                                                         // 4個
            str[1] = "data_air_gps_minute,data_air_gps_second,data_air_gps_centisecond,data_air_gps_latitude_deg,";             // 4個
            str[2] = "data_air_gps_longitude_deg,data_air_gps_altitude_m,data_air_gps_groundspeed_ms,data_air_gps_heading_deg,data_air_gps_satellites,";  // 5個
            break;
          }
        case 1:
          {                                                                                                                                  // 11個
            str[0] = "filtered_bmp_altitude_m,filtered_urm_altitude_m,filtered_airspeed_ms,data_air_bmp_pressure_hPa,";                      // 4個
            str[1] = "data_air_bmp_temperature_deg,data_air_bmp_altitude_m,data_air_sdp_differentialPressure_Pa,data_air_sdp_airspeed_ms,";  // 4個
            str[2] = "data_air_AoA_angle_deg,data_air_AoS_angle_deg,data_ics_angle,";                                                        // 3個
            break;
          }
        case 2:  // 14個
          {
            str[0] = "fslg_is_alive,data_fslg_bno_qw,data_fslg_bno_qx,data_fslg_bno_qy,data_fslg_bno_qz,";                   // 5個
            str[1] = "data_fslg_bno_roll,data_fslg_bno_pitch,data_fslg_bno_yaw,data_fslg_lsm_roll,data_fslg_lsm_pitch,";     // 5個
            str[2] = "data_fslg_lsm_yaw,data_fslg_bmp_pressure_hPa,data_fslg_bmp_temperature_deg,data_fslg_bmp_altitude_m,";  // 4個
            break;
          }
        case 3:  // 16個
          {
            str[0] = "data_fslg_bno_accx_mss,data_fslg_bno_accy_mss,data_fslg_bno_accz_mss,data_fslg_lsm_accx_mss,data_fslg_lsm_accy_mss, data_fslg_lsm_accz_mss,";    // 6個
            str[1] = "data_fslg_bno_cal_system,data_fslg_bno_cal_gyro,data_fslg_bno_cal_accel,data_fslg_bno_cal_mag,under_is_alive,";                                  // 5個
            str[2] = "data_under_bmp_pressure_hPa,data_under_bmp_temperature_deg,data_under_bmp_altitude_m,data_under_urm_altitude_m,data_under_tsd20_altitude_m\n";  // 5個
            break;
          }
        default:
          {
            Serial.println("The parameter value is out of range.");
            break;
          }
      }

      sprintf(SD_BUF, "%s%s%s", str[0], str[1], str[2]);

      sd.add_str(SD_BUF);
      sd.flash();

      delayMicroseconds(10);  // 遅延あったほうがいいと思う
    }
  }
}