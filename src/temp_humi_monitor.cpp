#include "temp_humi_monitor.h"

DHT20 dht20;

void temp_humi_monitor(void *pvParameters){
    GlobalContext *ctx = (GlobalContext *)pvParameters;

    Wire.begin(11, 12);
    dht20.begin();

    while (1){
        dht20.read();
        float t = dht20.getTemperature();
        float h = dht20.getHumidity();

        if (isnan(t) || isnan(h)) {
            Serial.println("Failed to read from DHT sensor!");
            t = -1;
            h = -1;
        } else {
            Serial.print("Humidity: ");
            Serial.print(h);
            Serial.print("%  Temperature: ");
            Serial.print(t);
            Serial.println("°C");
        }

        // Use Mutex to protect writing to shared sensor data
        if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
            SensorState oldTempState = ctx->tempState;
            SensorState oldHumiState = ctx->humiState;

            ctx->temperature = t;
            ctx->humidity = h;

            // Define temperature conditions
            if (t < 30) {
                ctx->tempState = STATE_NORMAL;
            } else if (t < 35) {
                ctx->tempState = STATE_WARNING;
            } else {
                ctx->tempState = STATE_CRITICAL;
            }

            // Define humidity conditions
            if (h < 60) {
                ctx->humiState = STATE_NORMAL;
            } else if (h < 80) {
                ctx->humiState = STATE_WARNING;
            } else {
                ctx->humiState = STATE_CRITICAL;
            }

            xSemaphoreGive(ctx->dataMutex); // Release Mutex

            // Give Semaphores based on conditions (Notifying other tasks)
            if (oldTempState != ctx->tempState) {
                xSemaphoreGive(ctx->semTempUpdate);
            }
            if (oldHumiState != ctx->humiState) {
                xSemaphoreGive(ctx->semHumiUpdate);
            }

            // Notify LCD regardless of change if you want it refreshed, or only when changed
            // We'll update the LCD every read, or only on state change. Let's update it every read.
            xSemaphoreGive(ctx->semLCDUpdate);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}