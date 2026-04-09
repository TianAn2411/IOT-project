#include "neo_blinky.h"

void neo_blinky(void *pvParameters){
    GlobalContext* ctx = (GlobalContext*)pvParameters;

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    // Default color values
    uint8_t r = 0, g = 0, b = 0;

    while(1) {
        // Wait for humidity update semaphore.
        // NeoPixel doesn't need to blink constantly if the color is just solid.
        // If it should blink, use polling. The assignment says "color patterns ... represent different humidity levels".
        // Example: Normal -> Green, Warning -> Yellow/Orange, Critical -> Red.
        
        // Block until there's an update. On boot, maybe it takes a few seconds.
        if (xSemaphoreTake(ctx->semHumiUpdate, portMAX_DELAY) == pdTRUE) {
            SensorState state;
            if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
                state = ctx->humiState;
                xSemaphoreGive(ctx->dataMutex);
            }

            if (state == STATE_NORMAL) {
                // Green for normal
                r = 0; g = 255; b = 0;
            } else if (state == STATE_WARNING) {
                // Orange/Yellow for warning
                r = 255; g = 165; b = 0;
            } else if (state == STATE_CRITICAL) {
                // Red for critical
                r = 255; g = 0; b = 0;
            }

            // Set pattern
            for(int i=0; i<strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.Color(r, g, b));
            }
            strip.show();
        }
    }
}