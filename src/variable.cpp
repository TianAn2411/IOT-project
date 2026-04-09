#include "variable.h"

#include "memory.h"

#include <cstring>

namespace {

constexpr uint32_t kParamMagic = 0xA53036C1UL;

uint16_t addr_magic = 0;
uint16_t addr_sta_ssid = 0;
uint16_t addr_sta_password = 0;
uint16_t addr_ap_ssid = 0;
uint16_t addr_ap_password = 0;
uint16_t addr_login_password = 0;
uint16_t addr_config_timeout_ms = 0;
uint16_t addr_normal_use_sta_only = 0;
uint16_t addr_normal_ap_enabled = 0;

void assign_address(uint16_t &cursor, uint16_t &dest, uint16_t size) {
  dest = cursor;
  cursor = cursor + size;
}

bool is_safe_string(const char *s, size_t maxLen) {
  if (s[0] == '\0') {
    return false;
  }

  for (size_t i = 0; i < maxLen; ++i) {
    if (s[i] == '\0') {
      return true;
    }
  }
  return false;
}

void safe_copy(char *dest, size_t destSize, const char *src) {
  if (destSize == 0) {
    return;
  }

  strncpy(dest, src, destSize - 1);
  dest[destSize - 1] = '\0';
}

void clamp_values() {
  if (config_timeout_ms < 10000UL) {
    config_timeout_ms = 10000UL;
  }

  if (normal_use_sta_only > 1U) {
    normal_use_sta_only = NORMAL_USE_STA_ONLY_DEFAULT;
  }

  if (normal_ap_enabled > 1U) {
    normal_ap_enabled = NORMAL_AP_ENABLED_DEFAULT;
  }
}

}  // namespace

char sta_ssid[33] = WIFI_SSID_DEFAULT;
char sta_password[65] = WIFI_PASSWORD_DEFAULT;

char ap_ssid[33] = AP_SSID_DEFAULT;
char ap_password[65] = AP_PASSWORD_DEFAULT;

char login_password[65] = LOGIN_PASSWORD_DEFAULT;

uint32_t config_timeout_ms = CONFIG_TIMEOUT_MS_DEFAULT;
uint8_t normal_use_sta_only = NORMAL_USE_STA_ONLY_DEFAULT;
uint8_t normal_ap_enabled = NORMAL_AP_ENABLED_DEFAULT;

AppMode current_app_mode = APP_MODE_CONFIG;

void variable_set_defaults(void) {
  safe_copy(sta_ssid, sizeof(sta_ssid), WIFI_SSID_DEFAULT);
  safe_copy(sta_password, sizeof(sta_password), WIFI_PASSWORD_DEFAULT);
  safe_copy(ap_ssid, sizeof(ap_ssid), AP_SSID_DEFAULT);
  safe_copy(ap_password, sizeof(ap_password), AP_PASSWORD_DEFAULT);
  safe_copy(login_password, sizeof(login_password), LOGIN_PASSWORD_DEFAULT);
  config_timeout_ms = CONFIG_TIMEOUT_MS_DEFAULT;
  normal_use_sta_only = NORMAL_USE_STA_ONLY_DEFAULT;
  normal_ap_enabled = NORMAL_AP_ENABLED_DEFAULT;
}

void variable_init(void) {
  uint16_t cursor = 0;

  assign_address(cursor, addr_magic, sizeof(uint32_t));
  assign_address(cursor, addr_sta_ssid, sizeof(sta_ssid));
  assign_address(cursor, addr_sta_password, sizeof(sta_password));
  assign_address(cursor, addr_ap_ssid, sizeof(ap_ssid));
  assign_address(cursor, addr_ap_password, sizeof(ap_password));
  assign_address(cursor, addr_login_password, sizeof(login_password));
  assign_address(cursor, addr_config_timeout_ms, sizeof(uint32_t));
  assign_address(cursor, addr_normal_use_sta_only, sizeof(uint8_t));
  assign_address(cursor, addr_normal_ap_enabled, sizeof(uint8_t));
}

void variable_save_sta(void) {
  EepromWriteString(addr_sta_ssid, sta_ssid, sizeof(sta_ssid));
  EepromWriteString(addr_sta_password, sta_password, sizeof(sta_password));
}

void variable_save_ap(void) {
  EepromWriteString(addr_ap_ssid, ap_ssid, sizeof(ap_ssid));
  EepromWriteString(addr_ap_password, ap_password, sizeof(ap_password));
}

void variable_save_login_key(void) {
  EepromWriteString(addr_login_password, login_password, sizeof(login_password));
}

void variable_save_config_timeout(void) {
  EepromWrite32b(addr_config_timeout_ms, config_timeout_ms);
}

void variable_save_normal_options(void) {
  EepromWrite8b(addr_normal_use_sta_only, normal_use_sta_only);
  EepromWrite8b(addr_normal_ap_enabled, normal_ap_enabled);
}

void variable_save_all(void) {
  EepromWrite32b(addr_magic, kParamMagic);
  variable_save_sta();
  variable_save_ap();
  variable_save_login_key();
  variable_save_config_timeout();
  variable_save_normal_options();
}

void variable_load_all(void) {
  const uint32_t magic = EepromRead32b(addr_magic);
  if (magic != kParamMagic) {
    variable_set_defaults();
    variable_save_all();
    return;
  }

  EepromReadString(addr_sta_ssid, sta_ssid, sizeof(sta_ssid));
  EepromReadString(addr_sta_password, sta_password, sizeof(sta_password));
  EepromReadString(addr_ap_ssid, ap_ssid, sizeof(ap_ssid));
  EepromReadString(addr_ap_password, ap_password, sizeof(ap_password));
  EepromReadString(addr_login_password, login_password, sizeof(login_password));

  config_timeout_ms = EepromRead32b(addr_config_timeout_ms);
  normal_use_sta_only = EepromRead8b(addr_normal_use_sta_only);
  normal_ap_enabled = EepromRead8b(addr_normal_ap_enabled);

  if (!is_safe_string(sta_ssid, sizeof(sta_ssid))) {
    safe_copy(sta_ssid, sizeof(sta_ssid), WIFI_SSID_DEFAULT);
  }

  if (!is_safe_string(sta_password, sizeof(sta_password))) {
    safe_copy(sta_password, sizeof(sta_password), WIFI_PASSWORD_DEFAULT);
  }

  if (!is_safe_string(ap_ssid, sizeof(ap_ssid))) {
    safe_copy(ap_ssid, sizeof(ap_ssid), AP_SSID_DEFAULT);
  }

  if (!is_safe_string(ap_password, sizeof(ap_password))) {
    safe_copy(ap_password, sizeof(ap_password), AP_PASSWORD_DEFAULT);
  }

  if (!is_safe_string(login_password, sizeof(login_password))) {
    safe_copy(login_password, sizeof(login_password), LOGIN_PASSWORD_DEFAULT);
  }

  clamp_values();
}
