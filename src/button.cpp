#include "button.h"
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Define --------------------------------------------------------------------*/
constexpr uint32_t kButtonPollMs = 50;
constexpr uint32_t kHold2sTicks = 2000 / kButtonPollMs;
constexpr uint32_t kHold10sTicks = 10000 / kButtonPollMs;

/* Variables -----------------------------------------------------------------*/
uint32_t key_code                  = 0;
uint32_t key_code_before_releasing = 0;
volatile bool g_hold2sEventPending = false;
volatile bool g_hold10sEventPending = false;
bool g_hold2sLatched = false;
bool g_hold10sLatched = false;

/* Task handles */
TaskHandle_t buttonTaskHandle = NULL;

/* Functions -----------------------------------------------------------------*/
void button_task(void* pvParameters)
{
  (void)pvParameters;

  while (1)
  {
    if (digitalRead(BUTTON_PIN) == LOW)
    {
      key_code_before_releasing = 0;
      if (key_code < 1000) {
        key_code++;
      }

      if (key_code >= kHold2sTicks && !g_hold2sLatched) {
        g_hold2sLatched = true;
        g_hold2sEventPending = true;
      }

      if (key_code >= kHold10sTicks && !g_hold10sLatched) {
        g_hold10sLatched = true;
        g_hold10sEventPending = true;
      }
    }
    else
    {
      key_code_before_releasing = key_code;
      key_code                  = 0;
      g_hold2sLatched = false;
      g_hold10sLatched = false;
    }

    vTaskDelay(pdMS_TO_TICKS(kButtonPollMs));
  }
}

void button_init()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  key_code                  = 0;
  key_code_before_releasing = 0;

  // Create the button task
  xTaskCreate(button_task, "Button Task", 1024, NULL, 2, &buttonTaskHandle);

  Serial.println("but: \t [init]");
}

bool button_is_hold_2s_event() {
  if (!g_hold2sEventPending) {
    return false;
  }

  g_hold2sEventPending = false;
  return true;
}

bool button_is_hold_10s_event() {
  if (!g_hold10sEventPending) {
    return false;
  }

  g_hold10sEventPending = false;
  return true;
}