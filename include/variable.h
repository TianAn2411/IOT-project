#pragma once

#include <Arduino.h>

// Default STA settings
#define WIFI_SSID_DEFAULT "P628"
#define WIFI_PASSWORD_DEFAULT "628628628"

// Default AP settings
#define AP_SSID_DEFAULT "AP-ESP32"
#define AP_PASSWORD_DEFAULT "12345678"

// Login key for config mode
#define LOGIN_PASSWORD_DEFAULT "admin123"

// Main mode and timeout defaults
#define CONFIG_TIMEOUT_MS_DEFAULT 300000UL
#define SENSOR_UPDATE_INTERVAL_MS_DEFAULT 5000UL
#define NORMAL_USE_STA_ONLY_DEFAULT 0U
#define NORMAL_AP_ENABLED_DEFAULT 1U

enum AppMode : uint8_t {
	APP_MODE_CONFIG = 0,
	APP_MODE_NORMAL = 1,
};

enum NormalNetMode : uint8_t {
	NORMAL_NET_AP_STA = 0,
	NORMAL_NET_STA_ONLY = 1,
};

extern char sta_ssid[33];
extern char sta_password[65];

extern char ap_ssid[33];
extern char ap_password[65];

extern char login_password[65];

extern uint32_t config_timeout_ms;
extern uint8_t normal_use_sta_only;
extern uint8_t normal_ap_enabled;

extern AppMode current_app_mode;

void variable_init(void);
void variable_load_all(void);

void variable_save_sta(void);
void variable_save_ap(void);
void variable_save_login_key(void);
void variable_save_config_timeout(void);
void variable_save_normal_options(void);
void variable_save_all(void);

void variable_set_defaults(void);
