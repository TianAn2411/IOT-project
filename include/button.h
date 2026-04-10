#pragma once
#ifndef __BUTTON_H_
#define __BUTTON_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Define --------------------------------------------------------------------*/
#define BUTTON_PIN 0 // 0, 14

/* Variables -----------------------------------------------------------------*/
extern uint32_t key_code;
extern uint32_t key_code_before_releasing;

/* Functions -----------------------------------------------------------------*/
void button_init();
void button_process();             // Process button state
bool button_is_hold_2s_event();
bool button_is_hold_10s_event();

#endif