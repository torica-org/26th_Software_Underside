#pragma once

extern volatile float data_under_urm_altitude_m;

extern const int URCHO;
extern const int URTRIG;

void echo_init();

void read_echo();

