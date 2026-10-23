#include "power.h"
#include <Wire.h>

// INA219 registers
#define INA219_REG_CONFIG    0x00
#define INA219_REG_SHUNTVOLT 0x01
#define INA219_REG_BUSVOLT   0x02
#define INA219_REG_CURRENT   0x04
#define INA219_REG_CALIB     0x05

void ina219_write(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(INA219_ADDR);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);
    Wire.write(value & 0xFF);
    Wire.endTransmission();
}

uint16_t ina219_read(uint8_t reg) {
    Wire.beginTransmission(INA219_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(INA219_ADDR, 2);
    uint16_t value = (Wire.read() << 8) | Wire.read();
    return value;
}

void power_init() {
    // Power switch pin
    pinMode(PIN_PWR_EN, OUTPUT);
    digitalWrite(PIN_PWR_EN, LOW);

    // LED pins
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    // Configure INA219
    // 32V range, 320mV shunt, 12-bit, continuous
    ina219_write(INA219_REG_CONFIG, 0x399F);
    
    // Calibration for 0.1 ohm shunt, 1A max
    // Cal = 0.04096 / (current_LSB * shunt_resistance)
    // current_LSB = 0.0001A (0.1mA), shunt = 0.1 ohm
    // Cal = 0.04096 / (0.0001 * 0.1) = 4096
    ina219_write(INA219_REG_CALIB, 4096);

    Serial.println("Power system initialized");
}

void power_enable() {
    digitalWrite(PIN_PWR_EN, HIGH);
    Serial.println("VBUS power enabled");
}

void power_disable() {
    digitalWrite(PIN_PWR_EN, LOW);
    Serial.println("VBUS power disabled");
}

float power_read_current_ma() {
    int16_t raw = (int16_t)ina219_read(INA219_REG_CURRENT);
    // Each LSB = 0.1mA with our calibration
    return raw * 0.1f;
}

void led_green(bool state) {
    digitalWrite(PIN_LED_GREEN, state ? HIGH : LOW);
}

void led_blue(bool state) {
    digitalWrite(PIN_LED_BLUE, state ? HIGH : LOW);
}

void led_red(bool state) {
    digitalWrite(PIN_LED_RED, state ? HIGH : LOW);
}