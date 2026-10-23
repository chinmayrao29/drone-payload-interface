#pragma once
#include <Arduino.h>

// Power control pin
#define PIN_PWR_EN      4

// LED pins
#define PIN_LED_GREEN   6
#define PIN_LED_BLUE    7
#define PIN_LED_RED     14

// INA219 I2C address
#define INA219_ADDR     0x40

void power_init();
void power_enable();
void power_disable();
float power_read_current_ma();
void led_green(bool state);
void led_blue(bool state);
void led_red(bool state);