#pragma once
#include <Arduino.h>
#include <Wire.h>

// ADC pins
#define PIN_CC1_ADC     1
#define PIN_CC2_ADC     2

// I2C pins
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9

// Heartbeat pin
#define PIN_HEARTBEAT   5

// EEPROM address
#define EEPROM_ADDR     0x50

// Module type IDs
#define MODULE_UNKNOWN          0xFF
#define MODULE_TEST             0x00
#define MODULE_THERMAL          0x01
#define MODULE_SPRAY            0x02
#define MODULE_LIDAR            0x03
#define MODULE_GRIPPER          0x04
#define MODULE_CAMERA           0x05
#define MODULE_RELAY            0x06
#define MODULE_TETHER           0x07
// Module profile from EEPROM
struct __attribute__((packed)) ModuleProfile {
    uint8_t typeId;
    uint8_t hwVersion;
    uint8_t fwVersion;
    uint16_t maxCurrentMa;
    uint8_t baudCode;
    uint8_t reserved[2];
    char name[24];
};

// State machine states
typedef enum {
  STATE_IDLE,
  STATE_DETECTION,
  STATE_RUNNING,
  STATE_FAULT
} SystemState;

extern SystemState currentState;
extern ModuleProfile currentModule;
extern uint16_t maxCurrentMa;

void detection_init();
float read_cc1_voltage();
uint8_t identify_module(float voltage);
bool read_eeprom(ModuleProfile* profile);
void idle_loop();
void detection_loop();
void running_loop();
void fault_loop();