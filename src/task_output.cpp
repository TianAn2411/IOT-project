#include "task_output.h"

#include "variable.h"

namespace {

constexpr uint32_t kLoopDelayMs = 50;
constexpr uint32_t kBlinkNormalMs = 1000;
constexpr uint32_t kBlinkWarningMs = 400;
constexpr uint32_t kBlinkCriticalMs = 120;

constexpr uint32_t kPwmFreqHz = 5000;
constexpr uint8_t kPwmResolutionBits = 8;
constexpr uint8_t kPwm1Channel = 0;
constexpr uint8_t kPwm2Channel = 1;

portMUX_TYPE g_outputMux = portMUX_INITIALIZER_UNLOCKED;

bool g_led1Enabled = false;
bool g_pwm1Enabled = false;
uint8_t g_pwm1DutyPercent = 50;

bool g_pwm2Enabled = false;
uint8_t g_pwm2DutyPercent = 50;

bool g_pwm2Manual = false;

uint8_t state_to_pwm(SensorState state) {
  if (state == STATE_CRITICAL) {
    return 255;
  }
  if (state == STATE_WARNING) {
    return 160;
  }
  return 80;
}

uint32_t state_to_blink_ms(SensorState state) {
  if (state == STATE_CRITICAL) {
    return kBlinkCriticalMs;
  }
  if (state == STATE_WARNING) {
    return kBlinkWarningMs;
  }
  return kBlinkNormalMs;
}

uint8_t clamp_pwm_duty_percent(uint8_t dutyPercent) {
  if (dutyPercent > 100U) {
    return 100U;
  }
  return dutyPercent;
}

uint8_t duty_percent_to_raw(uint8_t dutyPercent) {
  return static_cast<uint8_t>((255UL * dutyPercent) / 100UL);
}

}  // namespace

void task_output_set_led1_enabled(bool enabled) {
  taskENTER_CRITICAL(&g_outputMux);
  g_led1Enabled = enabled;
  taskEXIT_CRITICAL(&g_outputMux);
}

bool task_output_get_led1_enabled() {
  taskENTER_CRITICAL(&g_outputMux);
  const bool enabled = g_led1Enabled;
  taskEXIT_CRITICAL(&g_outputMux);
  return enabled;
}

void task_output_set_pwm1_config(bool enabled, uint8_t dutyPercent) {
  taskENTER_CRITICAL(&g_outputMux);
  g_pwm1Enabled = enabled;
  g_pwm1DutyPercent = clamp_pwm_duty_percent(dutyPercent);
  taskEXIT_CRITICAL(&g_outputMux);
}

bool task_output_get_pwm1_enabled() {
  taskENTER_CRITICAL(&g_outputMux);
  const bool enabled = g_pwm1Enabled;
  taskEXIT_CRITICAL(&g_outputMux);
  return enabled;
}

uint8_t task_output_get_pwm1_duty_percent() {
  taskENTER_CRITICAL(&g_outputMux);
  const uint8_t duty = g_pwm1DutyPercent;
  taskEXIT_CRITICAL(&g_outputMux);
  return duty;
}

void task_output_set_pwm2_config(bool enabled, uint8_t dutyPercent) {
  taskENTER_CRITICAL(&g_outputMux);
  g_pwm2Manual = true;
  g_pwm2Enabled = enabled;
  g_pwm2DutyPercent = clamp_pwm_duty_percent(dutyPercent);
  taskEXIT_CRITICAL(&g_outputMux);
}

bool task_output_get_pwm2_enabled() {
  taskENTER_CRITICAL(&g_outputMux);
  const bool enabled = g_pwm2Enabled;
  taskEXIT_CRITICAL(&g_outputMux);
  return enabled;
}

uint8_t task_output_get_pwm2_duty_percent() {
  taskENTER_CRITICAL(&g_outputMux);
  const uint8_t duty = g_pwm2DutyPercent;
  taskEXIT_CRITICAL(&g_outputMux);
  return duty;
}

void task_output(void *pvParameters) {
  GlobalContext *ctx = static_cast<GlobalContext *>(pvParameters);

  pinMode(OUTPUT_LED1_GPIO, OUTPUT);
  digitalWrite(OUTPUT_LED1_GPIO, LOW);

  ledcSetup(kPwm1Channel, kPwmFreqHz, kPwmResolutionBits);
  ledcSetup(kPwm2Channel, kPwmFreqHz, kPwmResolutionBits);
  ledcAttachPin(OUTPUT_PWM1_GPIO, kPwm1Channel);
  ledcAttachPin(OUTPUT_PWM2_GPIO, kPwm2Channel);
  ledcWrite(kPwm1Channel, 0);
  ledcWrite(kPwm2Channel, 0);

  SensorState humiState = STATE_NORMAL;

  uint32_t blink2IntervalMs = state_to_blink_ms(humiState);
  uint8_t pwm1Duty = duty_percent_to_raw(g_pwm1DutyPercent);
  uint8_t pwm2Duty = state_to_pwm(humiState);

  bool led1Applied = false;
  bool pwm1EnabledApplied = false;
  bool pwm2EnabledApplied = false;

  bool led1On = false;
  ledcWrite(kPwm1Channel, pwm1Duty);
  ledcWrite(kPwm2Channel, pwm2Duty);

  for (;;) {
    if (xSemaphoreTake(ctx->semHumiUpdate, 0) == pdTRUE &&
        xSemaphoreTake(ctx->dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      humiState = ctx->humiState;
      xSemaphoreGive(ctx->dataMutex);

      blink2IntervalMs = state_to_blink_ms(humiState);

      const uint8_t nextPwm2Duty = state_to_pwm(humiState);
      if (nextPwm2Duty != pwm2Duty) {
        pwm2Duty = nextPwm2Duty;
        ledcWrite(kPwm2Channel, pwm2Duty);
      }
    }

    bool led1Enabled = false;
    bool pwm1Enabled = false;
    uint8_t pwm1DutyPercent = 0;
    bool pwm2Enabled = false;
    uint8_t pwm2DutyPercent = 0;
    bool pwm2Manual = false;
    taskENTER_CRITICAL(&g_outputMux);
    led1Enabled = g_led1Enabled;
    pwm1Enabled = g_pwm1Enabled;
    pwm1DutyPercent = g_pwm1DutyPercent;
    pwm2Enabled = g_pwm2Enabled;
    pwm2DutyPercent = g_pwm2DutyPercent;
    pwm2Manual = g_pwm2Manual;
    taskEXIT_CRITICAL(&g_outputMux);

    if (current_app_mode == APP_MODE_CONFIG) {
      led1Enabled = false;
      pwm1Enabled = false;
    }

    const uint8_t nextPwm1Duty = pwm1Enabled ? duty_percent_to_raw(pwm1DutyPercent) : 0;
    if (nextPwm1Duty != pwm1Duty || pwm1Enabled != pwm1EnabledApplied) {
      pwm1Duty = nextPwm1Duty;
      pwm1EnabledApplied = pwm1Enabled;
      ledcWrite(kPwm1Channel, pwm1Duty);
    }

    if (led1Enabled != led1Applied) {
      led1Applied = led1Enabled;
      led1On = led1Enabled;
      digitalWrite(OUTPUT_LED1_GPIO, led1On ? HIGH : LOW);
    }

    if (pwm2Manual) {
      const uint8_t nextPwm2Duty = pwm2Enabled ? duty_percent_to_raw(pwm2DutyPercent) : 0;
      if (nextPwm2Duty != pwm2Duty || pwm2Enabled != pwm2EnabledApplied) {
        pwm2Duty = nextPwm2Duty;
        pwm2EnabledApplied = pwm2Enabled;
        ledcWrite(kPwm2Channel, pwm2Duty);
      }
    }

    (void)blink2IntervalMs;

    vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
  }
}
