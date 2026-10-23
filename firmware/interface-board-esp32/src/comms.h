#pragma once
#include <Arduino.h>

// UART pins
#define PIN_UART_TX     43
#define PIN_UART_RX     44
#define PIN_MOD_TX      10
#define PIN_MOD_RX      11

// UART baud rates
#define PI_BAUD         115200
#define MODULE_BAUD     115200

void comms_init();
void comms_send_pi(const char* message);
void comms_send_module(const char* message);
String comms_read_pi();
String comms_read_module();