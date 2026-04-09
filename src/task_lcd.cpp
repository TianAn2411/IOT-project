#include "task_lcd.h"
#include "app_rtc.h"
#include <Wire.h>

// 0x21 is often decimal 33. I2C address for some LCD backpacks.
LiquidCrystal_I2C lcd(39, 16, 2); 

void task_lcd(void *pvParameters) {
    GlobalContext *ctx = (GlobalContext *)pvParameters;
    Wire.begin(11, 12);
    lcd.begin();
    lcd.noBacklight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Group An Tai");

    int displayState = 0;
    uint32_t lastToggle = xTaskGetTickCount();

    while (1) {
        // Wait for semaphore or timeout after 1 second
        xSemaphoreTake(ctx->semLCDUpdate, pdMS_TO_TICKS(1000));
        
        if (xTaskGetTickCount() - lastToggle >= pdMS_TO_TICKS(3000)) {
            displayState = (displayState + 1) % 3;
            lastToggle = xTaskGetTickCount();
        }

        float t = 0, h = 0;
        String pWeather = "Unknown";

        // Safely read from context using mutex
        if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
            t = ctx->temperature;
            h = ctx->humidity;
            pWeather = ctx->predictedWeather;
            xSemaphoreGive(ctx->dataMutex);
        }

        lcd.setCursor(0, 1);
        lcd.print("                "); // clear line
        lcd.setCursor(0, 1);

        if (displayState == 0) {
            lcd.print(t, 1); lcd.print("C "); lcd.print(h, 1); lcd.print("%");
        } else if (displayState == 1) {
            lcd.print("Pre:"); lcd.print(pWeather);
        } else {
            AppRtcDateTime dt = {};
            if (app_rtc_get_datetime(dt)) {
                char timeBuf[16];
                snprintf(timeBuf, sizeof(timeBuf), "Time: %02d:%02d:%02d", dt.hour, dt.minute, dt.second);
                lcd.print(timeBuf);
            } else {
                lcd.print("Time: Syncing...");
            }
        }
    }
}
