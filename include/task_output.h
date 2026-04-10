#ifndef __TASK_OUTPUT_H__
#define __TASK_OUTPUT_H__

#include <Arduino.h>
#include "global.h"

#define OUTPUT_LED1_GPIO 38
#define OUTPUT_PWM1_GPIO 47
#define OUTPUT_PWM2_GPIO 5

void task_output(void *pvParameters);

void task_output_set_led1_enabled(bool enabled);
bool task_output_get_led1_enabled();

void task_output_set_pwm1_config(bool enabled, uint8_t dutyPercent);
bool task_output_get_pwm1_enabled();
uint8_t task_output_get_pwm1_duty_percent();

void task_output_set_pwm2_config(bool enabled, uint8_t dutyPercent);
bool task_output_get_pwm2_enabled();
uint8_t task_output_get_pwm2_duty_percent();

#endif
