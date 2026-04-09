#pragma once

#include <Arduino.h>

struct AppRtcDateTime {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
};

void app_rtc_init();
void app_rtc_loop();
bool app_rtc_is_ready();
bool app_rtc_is_ntp_synced();
String app_rtc_now_iso8601();
bool app_rtc_get_datetime(AppRtcDateTime &out);
float app_rtc_get_hour_float();
