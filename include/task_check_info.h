#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <ArduinoJson.h>
#include "LittleFS.h"
#include "global.h"
#include "task_wifi.h"


bool check_info_File(GlobalContext *ctx, bool check);
void Load_info_File(GlobalContext *ctx);
void Delete_info_File();
void Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT);

#endif