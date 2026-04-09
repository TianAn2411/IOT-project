#include "task_lcd.h"
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

    bool showPredict = false;
    uint32_t lastToggle = xTaskGetTickCount();

    while (1) {
        // Wait for semaphore or timeout after 2 seconds
        xSemaphoreTake(ctx->semLCDUpdate, pdMS_TO_TICKS(2000));
        
        if (xTaskGetTickCount() - lastToggle >= pdMS_TO_TICKS(2000)) {
            showPredict = !showPredict;
            lastToggle = xTaskGetTickCount();
        }

        float t = 0, h = 0, pMin = 0, pMax = 0;

        // Safely read from context using mutex
        if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
            t = ctx->temperature;
            h = ctx->humidity;
            pMin = ctx->predictedMinTemp;
            pMax = ctx->predictedMaxTemp;
            xSemaphoreGive(ctx->dataMutex);
        }

        lcd.setCursor(0, 1);
        lcd.print("                "); // clear line
        lcd.setCursor(0, 1);

        if (!showPredict) {
            lcd.print(t, 1); lcd.print("C "); lcd.print(h, 1); lcd.print("%");
        } else {
            lcd.print("Pre:"); lcd.print(pMax, 1); lcd.print("-"); lcd.print(pMin, 1);
        }
    }
}
