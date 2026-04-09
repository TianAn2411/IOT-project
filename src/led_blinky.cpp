#include "led_blinky.h"

void led_blinky(void *pvParameters){
    GlobalContext* ctx = (GlobalContext*)pvParameters;
    pinMode(LED_GPIO, OUTPUT);

    // Default blink rate 1000ms
    int blinkRate = 1000;

    while(1) {
        // Poll semaphore to see if temperature state changed
        if (xSemaphoreTake(ctx->semTempUpdate, 0) == pdTRUE) {
            SensorState state;
            if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
                state = ctx->tempState;
                xSemaphoreGive(ctx->dataMutex);
            }

            // Adjust blinking behavior based on temperature condition
            if (state == STATE_NORMAL) {
                blinkRate = 1000; // Normal blink
            } else if (state == STATE_WARNING) {
                blinkRate = 500;  // Fast blink
            } else if (state == STATE_CRITICAL) {
                blinkRate = 100;  // Very fast blink
            }
        }

        digitalWrite(LED_GPIO, HIGH);
        vTaskDelay(blinkRate / portTICK_PERIOD_MS);
        digitalWrite(LED_GPIO, LOW);
        vTaskDelay(blinkRate / portTICK_PERIOD_MS);
    }
}