#include "app_webserver.h"

#include "app_html.h"
#include "app_rtc.h"
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
  Serial.printf("[WEB] %s %s from %s\n", tag, request->url().c_str(),
                request->client()->remoteIP().toString().c_str());
}

void log_lock_state(const char *tag) {
  Serial.printf("[AUTH] %s locked=%s ip=%s lastSeen=%lu ms\n", tag,
                g_clientLocked ? "true" : "false",
                g_clientLocked ? g_clientIp.toString().c_str() : "0.0.0.0",
                static_cast<unsigned long>(g_clientLastSeenMs));
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
  Serial.println("[AUTH] release_client_lock");
  g_clientLocked = false;
  g_clientIp = IPAddress(0, 0, 0, 0);
  g_clientLastSeenMs = 0;
}

void reset_login_gate() {
  Serial.println("[AUTH] reset_login_gate");
  release_client_lock();
}

void touch_client_if_owner(const IPAddress &ip) {
  if (g_clientLocked && ip == g_clientIp) {
    g_clientLastSeenMs = millis();
    Serial.printf("[AUTH] touch owner ip=%s\n", ip.toString().c_str());
  }
}

void refresh_client_lock_state() {
  if (!g_clientLocked) {
    return;
  }

  if ((millis() - g_clientLastSeenMs) > kClientIdleTimeoutMs) {
    Serial.printf(
        "[AUTH] idle timeout reached: elapsed=%lu ms threshold=%lu ms\n",
        static_cast<unsigned long>(millis() - g_clientLastSeenMs),
        static_cast<unsigned long>(kClientIdleTimeoutMs));
    release_client_lock();
  }
}

bool is_authorized_ip(const IPAddress &ip) {
  refresh_client_lock_state();

  if (!g_clientLocked) {
    Serial.printf("[AUTH] deny ip=%s reason=no-lock\n", ip.toString().c_str());
    return false;
  }

  if (ip != g_clientIp) {
    Serial.printf("[AUTH] deny ip=%s owner=%s reason=ip-mismatch\n",
                  ip.toString().c_str(), g_clientIp.toString().c_str());
    return false;
  }

  g_clientLastSeenMs = millis();
  Serial.printf("[AUTH] allow ip=%s\n", ip.toString().c_str());
  return true;
}

bool ensure_authorized(AsyncWebServerRequest *request) {
  if (current_app_mode != APP_MODE_CONFIG) {
    Serial.printf("[WEB] reject %s reason=not-config-mode\n",
                  request->url().c_str());
    request->send(
        403, "application/json",
        "{\"message\":\"Config endpoints are available only in config mode\"}");
    return false;
  }

  const IPAddress ip = request->client()->remoteIP();
  if (is_authorized_ip(ip)) {
    return true;
  }

  Serial.printf("[WEB] unauthorized %s from %s\n", request->url().c_str(),
                ip.toString().c_str());
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
      Serial.println("[WEB] serving normal page");
      request->send(200, "text/html", app_html_get_normal_page());
      return;
    }

    if (is_config_unlocked()) {
      Serial.println("[WEB] serving main page");
      request->send(200, "text/html", app_html_get_main_page());
      return;
    }

    Serial.println("[WEB] serving login page");
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

    Serial.printf("[AUTH] login attempt ip=%s keyLen=%u\n",
                  ip.toString().c_str(), static_cast<unsigned>(key.length()));

    if (key != String(login_password)) {
      Serial.println("[AUTH] login failed: wrong key");
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

    Serial.printf("[AUTH] login blocked: owner=%s requester=%s\n",
                  g_clientIp.toString().c_str(), ip.toString().c_str());
    request->send(423, "application/json",
                  "{\"message\":\"Another client is active\"}");
  });

  server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    if (g_clientLocked && request->client()->remoteIP() == g_clientIp) {
      Serial.println("[AUTH] owner logout");
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
    Serial.printf("[WEB] scan cache size=%u inProgress=%s\n",
                  static_cast<unsigned>(payload.length()),
                  app_wifi_is_scan_in_progress() ? "true" : "false");
    request->send(200, "application/json", payload);
  });

  server.on("/api/scan-result", HTTP_GET, [](AsyncWebServerRequest *request) {
    log_request("GET", request);
    if (!ensure_authorized(request)) {
      return;
    }

    const String payload = app_wifi_get_scan_cache_json();
    Serial.printf("[WEB] scan result cache size=%u inProgress=%s\n",
                  static_cast<unsigned>(payload.length()),
                  app_wifi_is_scan_in_progress() ? "true" : "false");
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

    Serial.printf("[WEB] sta save ssid=%s connected=%s\n", ssid.c_str(),
                  connected ? "true" : "false");

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

    Serial.printf("[WEB] ap saved ssid=%s\n", ap_ssid);

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
    Serial.println("[WEB] login key updated");
    send_ok(request, "Login key updated");
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
              Serial.printf("[WEB] config timeout updated: %lu sec\n",
                            static_cast<unsigned long>(sec));
              send_ok(request, "Config timeout updated");
            });

  server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest *request) {
    log_request("POST", request);
    const String mode = req_param(request, "mode");
    if (mode == "config") {
      if (current_app_mode == APP_MODE_CONFIG && !ensure_authorized(request)) {
        return;
      }

      Serial.println("[WEB] switch mode -> config");
      app_wifi_enter_config_mode();
      send_ok(request, "Switched to config mode");
      return;
    }

    if (mode == "normal") {
      if (!ensure_authorized(request)) {
        return;
      }

      Serial.println("[WEB] switch mode -> normal");
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
              Serial.printf("[WEB] normal option sta_only=%u\n",
                            static_cast<unsigned>(normal_use_sta_only));

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
    Serial.printf("[WEB] ap toggle enabled=%u\n",
                  static_cast<unsigned>(normal_ap_enabled));

    if (current_app_mode == APP_MODE_NORMAL) {
      app_wifi_apply_current_mode();
    }

    send_ok(request, normal_ap_enabled ? "AP enabled in normal mode"
                                       : "AP disabled in normal mode");
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void app_webserver_loop() { refresh_client_lock_state(); }

void app_webserver_reset_login_gate() { reset_login_gate(); }
