#pragma once

#include <Arduino.h>

void app_wifi_init();
void app_wifi_loop();

void app_wifi_enter_config_mode();
void app_wifi_enter_normal_mode();

bool app_wifi_begin_sta_connect(const char *ssid, const char *password);
bool app_wifi_connect_sta(const char *ssid, const char *password, uint32_t timeoutMs = 15000UL);
void app_wifi_apply_current_mode();

bool app_wifi_is_sta_connected();
bool app_wifi_is_internet_connected();
bool app_wifi_is_ap_active();

String app_wifi_get_sta_ip();
String app_wifi_get_ap_ip();
String app_wifi_get_sta_url();
String app_wifi_get_ap_url();
String app_wifi_get_connected_ssid();

void app_wifi_start_scan();
void app_wifi_poll_scan();
bool app_wifi_is_scan_in_progress();
String app_wifi_scan_networks_json();
String app_wifi_get_scan_cache_json();
void app_wifi_set_scan_config(bool showHidden,
							  bool passive,
							  uint32_t maxMsPerChannel,
							  uint8_t channel);
String app_wifi_get_scan_config_json();

uint32_t app_wifi_get_mode_started_ms();
uint32_t app_wifi_get_mode_elapsed_ms();
bool app_wifi_has_internet_check_result();
uint32_t app_wifi_get_last_internet_check_elapsed_ms();
