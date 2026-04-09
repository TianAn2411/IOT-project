#ifndef __COREIOT_H__
#define __COREIOT_H__

#include <Arduino.h>
#include <WiFi.h>
#include "global.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

void setup_coreiot(GlobalContext *ctx);
void coreiot_task(void *pvParameters);
void reconnect(GlobalContext *ctx);

#endif