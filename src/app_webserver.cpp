#include "app_webserver.h"

#include "app_html.h"
#include "app_rtc.h"
#include "task_output.h"
#include "app_wifi.h"
#include "variable.h"
#include "global.h"

extern GlobalContext* global_ctx;

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <cstring>

namespace {

AsyncWebServer server(80);

bool g_clientLocked = false;
IPAddress g_clientIp;
uint32_t g_clientLastSeenMs = 0;

constexpr uint32_t kClientIdleTimeoutMs = 300000UL;

float g_dummyTemp = 26.0f;
float g_dummyHumidity = 56.0f;
uint32_t g_dummyCounter = 0;
String g_sensorRtcIso = "";

void log_request(const char *tag, AsyncWebServerRequest *request) {
  (void)tag;
  (void)request;
}

void log_lock_state(const char *tag) {
  (void)tag;
}

String json_escape(const String &in) {
  String out;
  out.reserve(in.length() + 8);

  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }

  return out;
}

String req_param(AsyncWebServerRequest *request, const char *name) {
  if (!request->hasParam(name, true)) {
    return String();
  }

  return request->getParam(name, true)->value();
}

void copy_text(char *dest, size_t size, const String &src) {
  if (size == 0) {
    return;
  }

  strncpy(dest, src.c_str(), size - 1);
  dest[size - 1] = '\0';
}

void release_client_lock() {
  g_clientLocked = false;
  g_clientIp = IPAddress(0, 0, 0, 0);
  g_clientLastSeenMs = 0;
}

void reset_login_gate() {
  release_client_lock();
}

void touch_client_if_owner(const IPAddress &ip) {
  if (g_clientLocked && ip == g_clientIp) {
    g_clientLastSeenMs = millis();
  }
}

void refresh_client_lock_state() {
  if (!g_clientLocked) {
    return;
  }

  if ((millis() - g_clientLastSeenMs) > kClientIdleTimeoutMs) {
    release_client_lock();
  }
}

bool is_authorized_ip(const IPAddress &ip) {
  refresh_client_lock_state();

  if (!g_clientLocked) {
    return false;
  }

  if (ip != g_clientIp) {
    return false;
  }

  g_clientLastSeenMs = millis();
  return true;
}

bool ensure_authorized(AsyncWebServerRequest *request) {
  if (current_app_mode != APP_MODE_CONFIG) {
    request->send(
        403, "application/json",
        "{\"message\":\"Config endpoints are available only in config mode\"}");
    return false;
  }

  const IPAddress ip = request->client()->remoteIP();
  if (is_authorized_ip(ip)) {
    return true;
  }

  request->send(401, "application/json",
                "{\"message\":\"Unauthorized. Login first\"}");
  return false;
}

String current_mode_text() {
  return current_app_mode == APP_MODE_CONFIG ? "config" : "normal";
}

bool is_config_unlocked() {
  return current_app_mode == APP_MODE_CONFIG && g_clientLocked;
}

String build_status_json() {
  const bool staConnected = app_wifi_is_sta_connected();
  const bool hasInternetCheck = app_wifi_has_internet_check_result();
  const bool internetConnected = app_wifi_is_internet_connected();
  const bool apActive = app_wifi_is_ap_active();

  String json = "{";
  json += "\"mode\":\"" + current_mode_text() + "\",";
  json += "\"sta_connected\":" + String(staConnected ? "true" : "false") + ",";
    json += "\"internet_checked\":" + String(hasInternetCheck ? "true" : "false") + ",";
    json += "\"internet_connected\":" +
      String((hasInternetCheck && internetConnected) ? "true" : "false") + ",";
    json += "\"internet_last_check_sec_ago\":" +
      String(hasInternetCheck ? (app_wifi_get_last_internet_check_elapsed_ms() / 1000UL)
            : 0) +
      ",";
  json += "\"ap_active\":" + String(apActive ? "true" : "false") + ",";
  json += "\"sta_ssid\":\"" + json_escape(sta_ssid) + "\",";
  json += "\"sta_connected_ssid\":\"" +
          json_escape(app_wifi_get_connected_ssid()) + "\",";
  json += "\"ap_ssid\":\"" + json_escape(ap_ssid) + "\",";
  json += "\"sta_ip\":\"" + json_escape(app_wifi_get_sta_ip()) + "\",";
  json += "\"ap_ip\":\"" + json_escape(app_wifi_get_ap_ip()) + "\",";
  json += "\"sta_url\":\"" + json_escape(app_wifi_get_sta_url()) + "\",";
  json += "\"ap_url\":\"" + json_escape(app_wifi_get_ap_url()) + "\",";
  json += "\"config_timeout_sec\":" + String(config_timeout_ms / 1000UL) + ",";
    json += "\"mqtt_server\":\"" + json_escape(mqtt_server) + "\",";
    json += "\"mqtt_port\":" + String(mqtt_port) + ",";
    json += "\"mqtt_user\":\"" + json_escape(mqtt_user) + "\",";
    json += "\"mqtt_password\":\"" + json_escape(mqtt_password) + "\",";
    json += "\"mqtt_publish_interval_sec\":" +
      String(mqtt_publish_interval_ms / 1000UL) + ",";
  json += "\"led1_enabled\":" + String(task_output_get_led1_enabled() ? "true" : "false") + ",";
  json += "\"pwm1_enabled\":" + String(task_output_get_pwm1_enabled() ? "true" : "false") + ",";
  json += "\"pwm1_duty_percent\":" + String(task_output_get_pwm1_duty_percent()) + ",";
  json += "\"normal_use_sta_only\":" +
          String(normal_use_sta_only ? "true" : "false") + ",";
  json +=
      "\"normal_ap_enabled\":" + String(normal_ap_enabled ? "true" : "false") +
      ",";
  json += "\"client_locked\":" + String(g_clientLocked ? "true" : "false");
  json += "}";

  return json;
}

void send_ok(AsyncWebServerRequest *request, const String &message) {
  request->send(200, "application/json",
                "{\"message\":\"" + json_escape(message) + "\"}");
}

bool is_valid_ap_password(const String &password) {
  return password.length() >= 8 && password.length() <= 63;
}

bool is_valid_mqtt_port(uint16_t port) {
  return port > 0;
}

bool is_valid_publish_interval_sec(uint32_t sec) {
  return sec >= 10UL && sec <= 3600UL;
}

bool is_valid_pwm1_duty_percent(uint32_t dutyPercent) {
  return dutyPercent <= 100UL;
}

} // namespace

void app_webserver_update_dummy_sensor() {
  g_dummyCounter++;
  g_sensorRtcIso = app_rtc_now_iso8601();
}

float app_webserver_get_dummy_temp() {
    if (global_ctx) {
        return global_ctx->temperature;
    }
    return 0.0f; 
}

float app_webserver_get_dummy_humidity() { 
    if (global_ctx) {
        return global_ctx->humidity;
    }
    return 0.0f; 
}

uint32_t app_webserver_get_dummy_counter() { return g_dummyCounter; }

void app_webserver_init() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    if (current_app_mode == APP_MODE_NORMAL) {
      request->send(200, "text/html", app_html_get_normal_page());
      return;
    }

    if (is_config_unlocked()) {
      request->send(200, "text/html", app_html_get_main_page());
      return;
    }

    request->send(200, "text/html", app_html_get_login_page());
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    refresh_client_lock_state();
    touch_client_if_owner(request->client()->remoteIP());
    log_lock_state("status");
    request->send(200, "application/json", build_status_json());
  });

  server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    touch_client_if_owner(request->client()->remoteIP());
    log_lock_state("sensor");

    float t = 0.0f;
    float h = 0.0f;
    
    // Đọc dữ liệu thật từ biến dùng chung của Member 2
    if (global_ctx && xSemaphoreTake(global_ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
      t = global_ctx->temperature;
      h = global_ctx->humidity;
      xSemaphoreGive(global_ctx->dataMutex);
    }

    String json = "{";
    const String rtcIso = app_rtc_now_iso8601();
    json += "\"temp_c\":" + String(t, 1) + ",";
    json += "\"humidity\":" + String(h, 1) + ",";
    json += "\"counter\":" + String(g_dummyCounter) + ",";
    json += "\"rtc\":\"" + json_escape(rtcIso) + "\",";
    json += "\"rtc_ready\":" + String(app_rtc_is_ready() ? "true" : "false") + ",";
    json += "\"ntp_synced\":" + String(app_rtc_is_ntp_synced() ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (current_app_mode != APP_MODE_CONFIG) {
      request->send(403, "application/json",
                    "{\"message\":\"Login is available only in config mode\"}");
      return;
    }

    const String key = req_param(request, "key");
    const IPAddress ip = request->client()->remoteIP();

    refresh_client_lock_state();

    if (key != String(login_password)) {
      request->send(401, "application/json",
                    "{\"message\":\"Wrong login key\"}");
      return;
    }

    if (!g_clientLocked) {
      g_clientLocked = true;
      g_clientIp = ip;
      g_clientLastSeenMs = millis();
      log_lock_state("login-success-new");
      send_ok(request, "Login success. You are the active client");
      return;
    }

    if (ip == g_clientIp) {
      g_clientLastSeenMs = millis();
      log_lock_state("login-refresh-owner");
      send_ok(request, "Session refreshed");
      return;
    }

    request->send(423, "application/json",
                  "{\"message\":\"Another client is active\"}");
  });

  server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (g_clientLocked && request->client()->remoteIP() == g_clientIp) {
      release_client_lock();
    }
    send_ok(request, "Logged out");
  });

  server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    if (!ensure_authorized(request)) {
      return;
    }
    log_lock_state("ping");
    send_ok(request, "alive");
  });

  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    if (!ensure_authorized(request)) {
      return;
    }

    app_wifi_start_scan();
    const String payload = app_wifi_get_scan_cache_json();
    request->send(200, "application/json", payload);
  });

  server.on("/api/scan-result", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    if (!ensure_authorized(request)) {
      return;
    }

    const String payload = app_wifi_get_scan_cache_json();
    request->send(200, "application/json", payload);
  });

  server.on("/api/sta", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (!ensure_authorized(request)) {
      return;
    }

    const String ssid = req_param(request, "ssid");
    const String password = req_param(request, "password");

    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64) {
      request->send(400, "application/json",
                    "{\"message\":\"Invalid STA credentials\"}");
      return;
    }

    const bool connected = app_wifi_connect_sta(ssid.c_str(), password.c_str());
    variable_save_sta();

    if (connected) {
      send_ok(request, "STA connected. Web URL: " + app_wifi_get_sta_url());
    } else {
      request->send(200, "application/json",
                    "{\"message\":\"Saved STA, connect timeout\"}");
    }
  });

  server.on("/api/ap", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (!ensure_authorized(request)) {
      return;
    }

    const String ssid = req_param(request, "ssid");
    const String password = req_param(request, "password");

    if (ssid.length() == 0 || ssid.length() > 32 ||
        !is_valid_ap_password(password)) {
      request->send(400, "application/json",
                    "{\"message\":\"AP password must be 8..63 chars\"}");
      return;
    }

    copy_text(ap_ssid, sizeof(ap_ssid), ssid);
    copy_text(ap_password, sizeof(ap_password), password);
    variable_save_ap();
    app_wifi_apply_current_mode();

    send_ok(request, "AP saved. URL: " + app_wifi_get_ap_url());
  });

  server.on("/api/login-key", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (!ensure_authorized(request)) {
      return;
    }

    const String newKey = req_param(request, "new_key");
    if (newKey.length() < 4 || newKey.length() > 64) {
      request->send(400, "application/json",
                    "{\"message\":\"new_key length: 4..64\"}");
      return;
    }

    copy_text(login_password, sizeof(login_password), newKey);
    variable_save_login_key();
    send_ok(request, "Login key updated");
  });

  server.on("/api/mqtt-config", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              log_request("POST", request);
              if (!ensure_authorized(request)) {
                return;
              }

              const String serverText = req_param(request, "server");
              const String portText = req_param(request, "port");
              const String userText = req_param(request, "user");
              const String passwordText = req_param(request, "password");
              const String intervalSecText =
                  req_param(request, "publish_interval_sec");

              if (serverText.length() == 0 || serverText.length() > 64 ||
                  userText.length() > 64 || passwordText.length() > 64) {
                request->send(400, "application/json",
                              "{\"message\":\"Invalid MQTT text fields\"}");
                return;
              }

              const uint16_t port = static_cast<uint16_t>(portText.toInt());
              const uint32_t intervalSec =
                  static_cast<uint32_t>(intervalSecText.toInt());

              if (!is_valid_mqtt_port(port)) {
                request->send(400, "application/json",
                              "{\"message\":\"port must be 1..65535\"}");
                return;
              }

              if (!is_valid_publish_interval_sec(intervalSec)) {
                request->send(400, "application/json",
                              "{\"message\":\"publish_interval_sec must be 10..3600\"}");
                return;
              }

              copy_text(mqtt_server, sizeof(mqtt_server), serverText);
              mqtt_port = port;
              copy_text(mqtt_user, sizeof(mqtt_user), userText);
              if (passwordText.length() > 0) {
                copy_text(mqtt_password, sizeof(mqtt_password), passwordText);
              }
              mqtt_publish_interval_ms = intervalSec * 1000UL;

              variable_save_mqtt();
              variable_save_mqtt_publish_interval();

              send_ok(request, "MQTT config updated");
            });

  server.on("/api/config-timeout", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              log_request("POST", request);
              if (!ensure_authorized(request)) {
                return;
              }

              const String timeoutSec = req_param(request, "timeout_sec");
              const uint32_t sec = timeoutSec.toInt();
              if (sec < 10 || sec > 3600) {
                request->send(400, "application/json",
                              "{\"message\":\"timeout_sec must be 10..3600\"}");
                return;
              }

              config_timeout_ms = sec * 1000UL;
              variable_save_config_timeout();
              send_ok(request, "Config timeout updated");
            });

  server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    const String mode = req_param(request, "mode");
    if (mode == "config") {
      if (current_app_mode == APP_MODE_CONFIG && !ensure_authorized(request)) {
        return;
      }

      app_wifi_enter_config_mode();
      send_ok(request, "Switched to config mode");
      return;
    }

    if (mode == "normal") {
      // Allow falling back to normal mode even if config session expired.
      app_wifi_enter_normal_mode();
      send_ok(request, "Switched to normal mode");
      return;
    }

    request->send(400, "application/json", "{\"message\":\"Invalid mode\"}");
  });

  server.on("/api/normal-options", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              log_request("POST", request);
              if (!ensure_authorized(request)) {
                return;
              }

              const String modeText = req_param(request, "normal_use_sta_only");
              normal_use_sta_only = modeText == "1" ? 1U : 0U;
              variable_save_normal_options();

              if (current_app_mode == APP_MODE_NORMAL) {
                app_wifi_apply_current_mode();
              }

              send_ok(request, "Normal options updated");
            });

  server.on("/api/ap-toggle", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (current_app_mode == APP_MODE_CONFIG) {
      if (!ensure_authorized(request)) {
        return;
      }
    }

    const String enabled = req_param(request, "enabled");
    normal_ap_enabled = enabled == "1" ? 1U : 0U;
    variable_save_normal_options();

    if (current_app_mode == APP_MODE_NORMAL) {
      app_wifi_apply_current_mode();
    }

    send_ok(request, normal_ap_enabled ? "AP enabled in normal mode"
                                       : "AP disabled in normal mode");
  });

  server.on("/api/output-led1", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (current_app_mode != APP_MODE_NORMAL) {
      request->send(403, "application/json",
                    "{\"message\":\"Output control is available only in normal mode\"}");
      return;
    }

    const String enabledText = req_param(request, "enabled");
    const bool enabled = (enabledText == "1" || enabledText == "true");
    task_output_set_led1_enabled(enabled);
    send_ok(request, enabled ? "LED1 enabled" : "LED1 disabled");
  });

  server.on("/api/output-pwm1", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (current_app_mode != APP_MODE_NORMAL) {
      request->send(403, "application/json",
                    "{\"message\":\"Output control is available only in normal mode\"}");
      return;
    }

    const String enabledText = req_param(request, "enabled");
    const String dutyText = req_param(request, "duty_percent");

    const bool enabled = (enabledText == "1" || enabledText == "true");
    const uint32_t dutyPercent = static_cast<uint32_t>(dutyText.toInt());

    if (!is_valid_pwm1_duty_percent(dutyPercent)) {
      request->send(400, "application/json",
                    "{\"message\":\"duty_percent must be 0..100\"}");
      return;
    }

    task_output_set_pwm1_config(enabled, static_cast<uint8_t>(dutyPercent));
    send_ok(request, enabled ? "PWM1 enabled" : "PWM1 disabled");
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void app_webserver_loop() { refresh_client_lock_state(); }

void app_webserver_reset_login_gate() { reset_login_gate(); }
