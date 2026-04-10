#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Define States for Temperature and Humidity
enum SensorState {
    STATE_NORMAL,
    STATE_WARNING,
    STATE_CRITICAL
};

// Application Context replacing global variables
struct GlobalContext {
    // Sensor Information
    float temperature = -1.0;
    float humidity = -1.0;
    String predictedWeather = "Unknown";

    // States
    SensorState tempState = STATE_NORMAL;
    SensorState humiState = STATE_NORMAL;
    
    // Mutex to protect shared data write/reads
    SemaphoreHandle_t dataMutex;

    // Sync Semaphores
    SemaphoreHandle_t semTempUpdate;
    SemaphoreHandle_t semHumiUpdate;
    SemaphoreHandle_t semLCDUpdate;

};

// --- Task & Module Includes ---
// (Placed here so that any file including global.h gets access to all task prototypes)
#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "tinyml.h"
#include "task_lcd.h"

#endif // __GLOBAL_H__
