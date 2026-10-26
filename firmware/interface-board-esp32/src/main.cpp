#include <Arduino.h>
#include <Wire.h>
#include "detection.h"
#include "power.h"
#include "comms.h"

void setup() {
    // Debug serial
    Serial.begin(115200);
    delay(2000);
    Serial.println("Serial OK");
     // TEMPORARY LED TEST — remove after confirming LEDs work
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    pinMode(14, OUTPUT);
    digitalWrite(6, HIGH);
    delay(500);
    digitalWrite(7, HIGH);
    delay(500);
    digitalWrite(14, HIGH);
    delay(500);
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);
    digitalWrite(14, LOW);
    // END TEMPORARY TEST
    Serial.println("Interface board starting...");
    
    // Initialize all systems
    comms_init();
    Serial.println("comms_init OK");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    power_init();
    Serial.println("power_init OK");
    detection_init();
    Serial.println("detection_init OK");
    
    // Green LED on — board is powered
    led_green(true);
    
    Serial.println("Ready — waiting for module");
    currentState = STATE_IDLE;
}

void loop() {
    comms_poll();
    switch (currentState) {
        case STATE_IDLE:
            idle_loop();
            break;
        case STATE_DETECTION:
            detection_loop();
            break;
        case STATE_RUNNING:
            running_loop();
            break;
        case STATE_FAULT:
            fault_loop();
            break;
    }
}