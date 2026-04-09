#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include <WiFi.h>
#include <task_check_info.h>

void startAP(GlobalContext *ctx);
void startSTA(GlobalContext *ctx);
bool Wifi_reconnect(GlobalContext *ctx);

#endif