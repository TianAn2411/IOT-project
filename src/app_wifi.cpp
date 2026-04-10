#include "app_wifi.h"

#include "app_webserver.h"

#include "variable.h"

#include <ArduinoHttpClient.h>
#include <WiFi.h>
#include <cstring>

namespace {

uint32_t g_modeStartedMs = 0;
bool g_apActive = false;
bool g_scanInProgress = false;
bool g_scanRequestPending = false;
String g_scanCacheJson = "{\"networks\":[],\"count\":0,\"message\":\"Scan not started\"}";
bool g_internetConnected = false;
bool g_hasInternetCheckResult = false;
uint32_t g_lastInternetCheckMs = 0;

constexpr uint32_t kInternetCheckIntervalMs = 120000UL;  // every 2 minutes
constexpr uint32_t kInternetCheckTimeoutMs = 5000UL;
constexpr char kInternetCheckHost[] = "www.google.com";
constexpr uint16_t kInternetCheckPort = 80;
constexpr char kInternetCheckPath[] = "/generate_204";

void copy_text(char *dest, size_t size, const char *src) {
  if (size == 0) {
    return;
  }
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
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

String build_scan_json_from_results(int count) {
  String json = "{\"networks\":[";

  int added = 0;
  if (count > 0) {
    for (int i = 0; i < count; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) {
        continue;
      }

      if (added > 0) {
        json += ",";
      }

      json += "{\"ssid\":\"";
      json += json_escape(ssid);
      json += "\",\"rssi\":";
      json += String(WiFi.RSSI(i));
      json += ",\"enc\":";
      json += String((int)WiFi.encryptionType(i));
      json += "}";
      added++;
    }
  }

  json += "],\"count\":";
  json += String(added);
  json += ",\"message\":";
  json += '"';
  json += (added > 0) ? "OK" : "Khong tim thay mang WiFi";
  json += '"';
  json += "}";
  return json;
}

void log_scan_state(const char *tag) {
  Serial.printf(
      "[SCAN] %s mode=%u sta=%s ap=%s pending=%s running=%s status=%d apIp=%s staIp=%s\n",
      tag,
      static_cast<unsigned>(current_app_mode),
      WiFi.isConnected() ? "true" : "false",
      g_apActive ? "true" : "false",
      g_scanRequestPending ? "true" : "false",
      g_scanInProgress ? "true" : "false",
      static_cast<int>(WiFi.status()),
      WiFi.softAPIP().toString().c_str(),
      WiFi.localIP().toString().c_str());
}

void start_ap() {
  WiFi.softAPdisconnect(true);
  g_apActive = WiFi.softAP(ap_ssid, ap_password);
}

void stop_ap() {
  WiFi.softAPdisconnect(true);
  g_apActive = false;
}

void start_sta_async() {
  if (strlen(sta_ssid) == 0) {
    return;
  }

  WiFi.begin(sta_ssid, sta_password);
}

void update_scan_cache_from_driver() {
  if (!g_scanInProgress) {
    return;
  }

  const int scanState = WiFi.scanComplete();
  if (scanState == WIFI_SCAN_RUNNING) {
    return;
  }

  if (scanState >= 0) {
    g_scanCacheJson = build_scan_json_from_results(scanState);
  } else {
    g_scanCacheJson = "{\"networks\":[],\"count\":0,\"message\":\"Scan failed\"}";
  }

  WiFi.scanDelete();
  g_scanInProgress = false;
}

void run_scan_now() {
  g_scanRequestPending = false;
  g_scanInProgress = true;
  g_scanCacheJson = "{\"networks\":[],\"count\":0,\"message\":\"Scan in progress\"}";

  log_scan_state("start");
  const uint32_t startMs = millis();
  WiFi.scanDelete();
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  
  const int count = WiFi.scanNetworks(false, true);
  const uint32_t elapsedMs = millis() - startMs;

  if (count >= 0) {
    g_scanCacheJson = build_scan_json_from_results(count);
    Serial.printf("[SCAN] done count=%d elapsed=%lu ms\n",
                  count,
                  static_cast<unsigned long>(elapsedMs));
  } else {
    g_scanCacheJson = "{\"networks\":[],\"count\":0,\"message\":\"Scan failed\"}";
    Serial.printf("[SCAN] failed state=%d elapsed=%lu ms\n",
                  count,
                  static_cast<unsigned long>(elapsedMs));
  }

  WiFi.scanDelete();
  g_scanInProgress = false;
  log_scan_state("end");
}

void apply_config_mode() {
  WiFi.mode(WIFI_MODE_APSTA);
  start_ap();
  start_sta_async();
  g_modeStartedMs = millis();
}

void apply_normal_mode() {
  if (normal_use_sta_only) {
    WiFi.mode(WIFI_MODE_STA);
    stop_ap();
    start_sta_async();
  } else {
    WiFi.mode(WIFI_MODE_APSTA);
    if (normal_ap_enabled) {
      start_ap();
    } else {
      stop_ap();
    }
    start_sta_async();
  }

  g_modeStartedMs = millis();
}

bool check_internet_connection() {
  WiFiClient wifiClient;
  HttpClient httpClient(wifiClient, kInternetCheckHost, kInternetCheckPort);
  httpClient.setHttpResponseTimeout(kInternetCheckTimeoutMs);
  httpClient.get(kInternetCheckPath);
  const int code = httpClient.responseStatusCode();
  if (code == 204) {
    Serial.println("[NET] Internet OK");
    httpClient.stop();
    return true;
  }

  if (code > 0) {
    Serial.printf("[NET] HTTP code=%d -> Internet NOT OK\n", code);
  } else {
    Serial.printf("[NET] HTTP request failed: code=%d\n", code);
  }

  httpClient.stop();
  return false;
}

void update_internet_status() {
  if (WiFi.status() != WL_CONNECTED) {
    if (g_internetConnected || g_hasInternetCheckResult) {
      Serial.println("[NET] Internet check reset: STA disconnected");
    }
    g_internetConnected = false;
    g_hasInternetCheckResult = false;
    g_lastInternetCheckMs = 0;
    return;
  }

  const uint32_t nowMs = millis();
  if (g_lastInternetCheckMs != 0 &&
      (nowMs - g_lastInternetCheckMs) < kInternetCheckIntervalMs) {
    return;
  }

  g_lastInternetCheckMs = nowMs;

  const bool ok = check_internet_connection();

  if (!g_hasInternetCheckResult || g_internetConnected != ok) {
    Serial.printf("[NET] Internet %s (ip=%s)\n",
                  ok ? "reachable" : "unreachable",
                  WiFi.localIP().toString().c_str());
  }

  g_internetConnected = ok;
  g_hasInternetCheckResult = true;
}

}  // namespace

void app_wifi_init() {
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  current_app_mode = APP_MODE_CONFIG;
  app_wifi_apply_current_mode();
}

void app_wifi_loop() {
  if (g_scanRequestPending && !g_scanInProgress) {
    run_scan_now();
  }

  update_scan_cache_from_driver();

  if (current_app_mode == APP_MODE_CONFIG) {
    const uint32_t elapsed = millis() - g_modeStartedMs;
    if (elapsed >= config_timeout_ms) {
      app_wifi_enter_normal_mode();
    }
  }

  update_internet_status();
}

void app_wifi_enter_config_mode() {
  current_app_mode = APP_MODE_CONFIG;
  app_webserver_reset_login_gate();
  apply_config_mode();
}

void app_wifi_enter_normal_mode() {
  current_app_mode = APP_MODE_NORMAL;
  app_webserver_reset_login_gate();
  apply_normal_mode();
}

void app_wifi_apply_current_mode() {
  if (current_app_mode == APP_MODE_CONFIG) {
    apply_config_mode();
  } else {
    apply_normal_mode();
  }
}

bool app_wifi_connect_sta(const char *ssid, const char *password, uint32_t timeoutMs) {
  if (ssid == nullptr || strlen(ssid) == 0) {
    return false;
  }

  copy_text(sta_ssid, sizeof(sta_ssid), ssid);
  copy_text(sta_password, sizeof(sta_password), password == nullptr ? "" : password);

  WiFi.begin(sta_ssid, sta_password);

  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  return WiFi.status() == WL_CONNECTED;
}

bool app_wifi_is_sta_connected() {
  return WiFi.status() == WL_CONNECTED;
}

bool app_wifi_is_internet_connected() {
  return g_internetConnected;
}

bool app_wifi_is_ap_active() {
  return g_apActive;
}

String app_wifi_get_sta_ip() {
  if (!app_wifi_is_sta_connected()) {
    return String();
  }

  return WiFi.localIP().toString();
}

String app_wifi_get_ap_ip() {
  if (!g_apActive) {
    return String();
  }

  return WiFi.softAPIP().toString();
}

String app_wifi_get_sta_url() {
  String ip = app_wifi_get_sta_ip();
  if (ip.length() == 0) {
    return String();
  }

  return String("http://") + ip + "/";
}

String app_wifi_get_ap_url() {
  String ip = app_wifi_get_ap_ip();
  if (ip.length() == 0) {
    return String();
  }

  return String("http://") + ip + "/";
}

String app_wifi_get_connected_ssid() {
  if (!app_wifi_is_sta_connected()) {
    return String();
  }

  return WiFi.SSID();
}

void app_wifi_start_scan() {
  if (g_scanInProgress || g_scanRequestPending) {
    return;
  }

  g_scanRequestPending = true;
  g_scanCacheJson = "{\"networks\":[],\"count\":0,\"message\":\"Scan queued\"}";
  log_scan_state("queued");
}

void app_wifi_poll_scan() {
  if (g_scanRequestPending && !g_scanInProgress) {
    run_scan_now();
    return;
  }

  update_scan_cache_from_driver();
}

bool app_wifi_is_scan_in_progress() {
  return g_scanInProgress;
}

String app_wifi_scan_networks_json() {
  return g_scanCacheJson;
}

String app_wifi_get_scan_cache_json() {
  return g_scanCacheJson;
}

uint32_t app_wifi_get_mode_started_ms() {
  return g_modeStartedMs;
}

uint32_t app_wifi_get_mode_elapsed_ms() {
  return millis() - g_modeStartedMs;
}

bool app_wifi_has_internet_check_result() {
  return g_hasInternetCheckResult;
}

uint32_t app_wifi_get_last_internet_check_elapsed_ms() {
  if (!g_hasInternetCheckResult || g_lastInternetCheckMs == 0) {
    return 0;
  }

  return millis() - g_lastInternetCheckMs;
}
