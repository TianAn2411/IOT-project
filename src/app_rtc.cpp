#include "app_rtc.h"

#include "app_wifi.h"

#include <ctime>
#include <cstring>
#include <WiFi.h>

namespace {

time_t g_baseEpoch = 0;
uint32_t g_baseMillis = 0;
bool g_rtcReady = false;
bool g_ntpConfigured = false;
bool g_ntpSynced = false;
uint32_t g_lastNtpAttemptMs = 0;
uint32_t g_lastNtpPendingLogMs = 0;

constexpr char kTzInfo[] = "ICT-7";          // GMT+7 (VN)
constexpr uint32_t kNtpRetryMs = 1000UL;      // poll every second while waiting first sync
constexpr uint32_t kNtpResyncMs = 21600000UL; // resync every 6 hours
constexpr uint32_t kNtpPendingLogMs = 10000UL; // reduce log spam while pending

constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";
constexpr char kNtpServer3[] = "time.google.com";

int month_to_index(const char *mon) {
  static const char *kMonths[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  for (int i = 0; i < 12; ++i) {
    if (strncmp(mon, kMonths[i], 3) == 0) {
      return i;
    }
  }

  return -1;
}

time_t parse_build_epoch() {
  // __DATE__ format: "Mmm dd yyyy", __TIME__ format: "hh:mm:ss"
  char mon[4] = {0};
  int day = 1;
  int year = 1970;
  int hh = 0;
  int mm = 0;
  int ss = 0;

  if (sscanf(__DATE__, "%3s %d %d", mon, &day, &year) != 3) {
    return 0;
  }

  if (sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss) != 3) {
    return 0;
  }

  const int month = month_to_index(mon);
  if (month < 0) {
    return 0;
  }

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month;
  t.tm_mday = day;
  t.tm_hour = hh;
  t.tm_min = mm;
  t.tm_sec = ss;
  t.tm_isdst = -1;
  return mktime(&t);
}

String format_iso8601(time_t epoch) {
  struct tm t = {};
  localtime_r(&epoch, &t);

  char buf[24];
  snprintf(buf,
           sizeof(buf),
           "%04d-%02d-%02dT%02d:%02d:%02d",
           t.tm_year + 1900,
           t.tm_mon + 1,
           t.tm_mday,
           t.tm_hour,
           t.tm_min,
           t.tm_sec);
  return String(buf);
}

time_t software_now_epoch() {
  const uint32_t elapsedSec = (millis() - g_baseMillis) / 1000UL;
  return g_baseEpoch + static_cast<time_t>(elapsedSec);
}

bool is_valid_epoch(time_t epoch) {
  // Rough sanity threshold: 2023-01-01 UTC
  return epoch >= 1672531200;
}

}  // namespace

void app_rtc_init() {
  setenv("TZ", kTzInfo, 1);
  tzset();

  g_baseEpoch = parse_build_epoch();
  g_baseMillis = millis();
  g_rtcReady = true;
  Serial.printf("[RTC] software clock ready: %s\n", app_rtc_now_iso8601().c_str());
}

void app_rtc_loop() {
  if (!app_wifi_is_sta_connected()) {
    return;
  }

  if (app_wifi_has_internet_check_result() && !app_wifi_is_internet_connected()) {
    return;
  }

  const uint32_t nowMs = millis();
  if (!g_ntpConfigured) {
    configTzTime(kTzInfo, kNtpServer1, kNtpServer2, kNtpServer3);
    g_ntpConfigured = true;
    g_lastNtpAttemptMs = nowMs;
    Serial.printf("[RTC] NTP configured, TZ=%s\n", kTzInfo);
    return;
  }

  const uint32_t intervalMs = g_ntpSynced ? kNtpResyncMs : kNtpRetryMs;
  if ((nowMs - g_lastNtpAttemptMs) < intervalMs) {
    return;
  }

  g_lastNtpAttemptMs = nowMs;

  const time_t ntpNow = time(nullptr);
  if (!is_valid_epoch(ntpNow)) {
    if ((nowMs - g_lastNtpPendingLogMs) >= kNtpPendingLogMs) {
      g_lastNtpPendingLogMs = nowMs;
      Serial.printf("[RTC] NTP sync pending... staIp=%s gateway=%s dns1=%s\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.gatewayIP().toString().c_str(),
                    WiFi.dnsIP(0).toString().c_str());
    }
    return;
  }

  g_baseEpoch = ntpNow;
  g_baseMillis = millis();

  if (!g_ntpSynced) {
    Serial.printf("[RTC] NTP synced: %s\n", app_rtc_now_iso8601().c_str());
  } else {
    Serial.println("[RTC] NTP resynced");
  }

  g_ntpSynced = true;
}

bool app_rtc_is_ready() {
  return g_rtcReady;
}

bool app_rtc_is_ntp_synced() {
  return g_ntpSynced;
}

bool app_rtc_get_datetime(AppRtcDateTime &out) {
  if (!g_rtcReady) {
    return false;
  }

  const time_t now = software_now_epoch();
  struct tm t = {};
  localtime_r(&now, &t);

  out.year = t.tm_year + 1900;
  out.month = t.tm_mon + 1;
  out.day = t.tm_mday;
  out.hour = t.tm_hour;
  out.minute = t.tm_min;
  out.second = t.tm_sec;
  return true;
}

float app_rtc_get_hour_float() {
  AppRtcDateTime dt = {};
  if (!app_rtc_get_datetime(dt)) {
    return 0.0f;
  }

  return static_cast<float>(dt.hour) +
         static_cast<float>(dt.minute) / 60.0f +
         static_cast<float>(dt.second) / 3600.0f;
}

String app_rtc_now_iso8601() {
  if (!g_rtcReady) {
    return String();
  }

  const time_t now = software_now_epoch();
  return format_iso8601(now);
}

uint32_t app_rtc_now_timestamp() {
  if (!g_rtcReady) {
    return 0;
  }

  const time_t now = software_now_epoch();
  if (now < 0) {
    return 0;
  }

  return static_cast<uint32_t>(now);
}
