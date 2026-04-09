#include "global.h"

GlobalContext* global_ctx = nullptr;

// Member 1 Headers
#include "app_rtc.h"
#include "app_webserver.h"
#include "app_wifi.h"
#include "memory.h"
#include "variable.h"
#include "button.h"


// Task includes have been moved to global.h

void taskwebserver_member1(void *parameter) {
  (void)parameter;
  for (;;) {
    if (button_is_hold_2s_event() && current_app_mode != APP_MODE_CONFIG) {
      Serial.println("[BTN] GPIO0 held 2s -> enter config mode");
      app_wifi_enter_config_mode();
    }

    app_wifi_loop();
    app_rtc_loop();
    app_webserver_loop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void main_task(void *pvParameters)
{
  GlobalContext* ctx = (GlobalContext*)pvParameters;
  while(1) {
    if (check_info_File(ctx, 1))
    {
      if (!app_wifi_is_sta_connected())
      {
        // CORE_IOT_stop/pause or handled within task
      }
      else
      {
        // CORE_IOT_reconnect(ctx);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}


void setup()
{
  Serial.begin(115200);

  // Allocate GlobalContext dynamically
  GlobalContext* ctx = new GlobalContext();
  global_ctx = ctx;
  
  // Initialize semaphores
  ctx->dataMutex = xSemaphoreCreateMutex();
  ctx->semTempUpdate = xSemaphoreCreateBinary();
  ctx->semHumiUpdate = xSemaphoreCreateBinary();
  ctx->semLCDUpdate = xSemaphoreCreateBinary();
  ctx->xBinarySemaphoreInternet = xSemaphoreCreateBinary();

  // Load configs (old method, kept for compatibility if needed)
  check_info_File(ctx, 0);

  // Initialize Member 1 modules
  eeprom_init();
  variable_init();
  variable_load_all();
  app_rtc_init();
  button_init();
  app_wifi_init();
  app_webserver_init();

  xTaskCreate(led_blinky, "Task LED Blink", 2048, ctx, 2, NULL);
  xTaskCreate(neo_blinky, "Task NEO Blink", 2048, ctx, 2, NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 2048, ctx, 2, NULL);
  xTaskCreate(task_lcd, "Task LCD", 2048, ctx, 2, NULL);
  xTaskCreate(taskwebserver_member1, "task webserver M1", 8192, nullptr, 2, nullptr);
  xTaskCreate( tiny_ml_task, "Tiny ML Task" , 4096  ,ctx  ,2 , NULL);
  xTaskCreate(coreiot_task, "CoreIOT Task" ,4096  ,ctx  ,2 , NULL);
  
  // Main logic
  xTaskCreate(main_task, "Main Task", 4096, ctx, 2, NULL);

  // Delete loop task
  vTaskDelete(NULL);
}

void loop()
{
  // Will never be called
}
