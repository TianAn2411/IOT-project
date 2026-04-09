#include "task_wifi.h"

void startAP(GlobalContext *ctx)
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ctx->ssid, ctx->password);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA(GlobalContext *ctx)
{
    if (ctx->WIFI_SSID.isEmpty())
    {
        return; // Changed from vTaskDelete(NULL) to prevent deleting main_task
    }

    WiFi.mode(WIFI_STA);

    if (ctx->WIFI_PASS.isEmpty())
    {
        WiFi.begin(ctx->WIFI_SSID.c_str());
    }
    else
    {
        WiFi.begin(ctx->WIFI_SSID.c_str(), ctx->WIFI_PASS.c_str());
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    // Give internet semaphore upon successful connect
    xSemaphoreGive(ctx->xBinarySemaphoreInternet);
}

bool Wifi_reconnect(GlobalContext *ctx)
{
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
        return true;
    }
    startSTA(ctx);
    return false;
}
