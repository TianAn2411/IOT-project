#pragma once

#include <Arduino.h>

void app_webserver_init();
void app_webserver_loop();

void app_webserver_reset_login_gate();

void app_webserver_update_dummy_sensor();
float app_webserver_get_dummy_temp();
float app_webserver_get_dummy_humidity();
uint32_t app_webserver_get_dummy_counter();
