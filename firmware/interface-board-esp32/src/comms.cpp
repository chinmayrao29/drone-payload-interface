#include "comms.h"

void comms_init() {
    // UART to Raspberry Pi
    Serial1.begin(PI_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
    
    // UART to module STM32
    Serial2.begin(MODULE_BAUD, SERIAL_8N1, PIN_MOD_RX, PIN_MOD_TX);
    
    Serial.println("UART comms initialized");
}

void comms_send_pi(const char* message) {
    Serial1.println(message);
    Serial.printf("To Pi: %s\n", message);
}

void comms_send_module(const char* message) {
    Serial2.println(message);
}

String comms_read_pi() {
    if (Serial1.available()) {
        return Serial1.readStringUntil('\n');
    }
    return "";
}

String comms_read_module() {
    if (Serial2.available()) {
        return Serial2.readStringUntil('\n');
    }
    return "";
}