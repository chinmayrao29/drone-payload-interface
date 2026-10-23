#include "detection.h"
#include "power.h"
#include "comms.h"
#include <Wire.h>

SystemState currentState = STATE_IDLE;
ModuleProfile currentModule;
uint16_t maxCurrentMa = 1000;

// Heartbeat tracking
volatile unsigned long lastHeartbeatTime = 0;

// Module identification table
struct ModuleEntry {
    float vMin;
    float vMax;
    uint8_t typeId;
    const char* name;
};

ModuleEntry moduleTable[] = {
    {0.50, 0.80, MODULE_TEST,    "test_module"},      // ~1.5kΩ
    {1.00, 1.30, MODULE_THERMAL, "thermal_camera"},   // ~4.7kΩ
    {1.50, 1.80, MODULE_SPRAY,   "spray_controller"}, // ~10kΩ
    {2.00, 2.30, MODULE_LIDAR,   "lidar_scanner"},    // ~22kΩ
    {2.50, 2.70, MODULE_GRIPPER, "delivery_gripper"}, // ~47kΩ
    {2.80, 2.95, MODULE_CAMERA,  "esp32_camera"},     // ~100kΩ
    {3.00, 3.10, MODULE_RELAY,   "comms_relay"},      // ~220kΩ
    {3.15, 3.25, MODULE_TETHER,  "tethered_sensor"}   // ~470kΩ
};

#define MODULE_TABLE_SIZE 8

// Heartbeat interrupt
void IRAM_ATTR heartbeat_isr() {
    lastHeartbeatTime = millis();
}

void detection_init() {
    // ADC setup
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(PIN_CC1_ADC, INPUT);
    
    // I2C bus reset — toggle SCL manually to unstick bus
pinMode(9, OUTPUT);
for(int i = 0; i < 9; i++) {
    digitalWrite(9, HIGH);
    delayMicroseconds(5);
    digitalWrite(9, LOW);
    delayMicroseconds(5);
}
pinMode(9, INPUT);
delay(100);
    // I2C setup
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    
    // Heartbeat interrupt
    pinMode(PIN_HEARTBEAT, INPUT);
    attachInterrupt(PIN_HEARTBEAT, heartbeat_isr, CHANGE);
    lastHeartbeatTime = millis();
    
    Serial.println("Detection system initialized");
}

float read_cc1_voltage() {
    // Take 5 samples and average for noise reduction
    float total = 0;
    for (int i = 0; i < 5; i++) {
        total += analogReadMilliVolts(PIN_CC1_ADC) / 1000.0f;
        delay(5);
    }
    return total / 5.0f;
}

uint8_t identify_module(float voltage) {
    for (int i = 0; i < MODULE_TABLE_SIZE; i++) {
        if (voltage >= moduleTable[i].vMin && 
            voltage <= moduleTable[i].vMax) {
            return moduleTable[i].typeId;
        }
    }
    return MODULE_UNKNOWN;
}

bool read_eeprom(ModuleProfile* profile)
{
    // AT24C32 requires TWO address bytes before reading, unlike AT24C02
    // which only needs one. Send 0x00 0x00 to set pointer to start of memory.
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write(0x00); // high address byte
    Wire.write(0x00); // low address byte
    if (Wire.endTransmission() != 0) {
        Serial.println("EEPROM not found");
        return false;
    }
 
    // Read 32 bytes — identical to before from here
    Wire.requestFrom(EEPROM_ADDR, 32);
    uint8_t* ptr = (uint8_t*)profile;
    int bytesRead = 0;
    while (Wire.available() && bytesRead < 32) {
        ptr[bytesRead++] = Wire.read();
    }
 
    if (bytesRead < 32) {
        Serial.println("EEPROM read incomplete");
        return false;
    }
 
    return true;
}
void idle_loop() {
    static unsigned long lastPoll = 0;
    unsigned long now = millis();
    
    if (now - lastPoll > 100) {
        lastPoll = now;
        float voltage = read_cc1_voltage();
        
        
        // Voltage dropped below 3.0V means something plugged in
        if (voltage < 3.0f) {
            Serial.printf("Module detected, CC1=%.2fV\n", voltage);
            delay(50); // debounce
            currentState = STATE_DETECTION;
        }
    }
}

void detection_loop() {
    Serial.println("Starting detection sequence...");
    
    // Step 1 - read CC1 voltage
    float voltage = read_cc1_voltage();
    Serial.printf("CC1 voltage: %.2fV\n", voltage);
    
    // Step 2 - identify module type
    uint8_t typeId = identify_module(voltage);
    
    if (typeId == MODULE_UNKNOWN) {
        Serial.println("Unknown module type");
        comms_send_pi("FAULT:unknown_module");
        led_red(true);
        currentState = STATE_IDLE;
        return;
    }
    
    Serial.printf("Module type identified: %d\n", typeId);
    
    // Step 3 - read EEPROM
    ModuleProfile profile;
    if (!read_eeprom(&profile)) {
        Serial.println("EEPROM read failed");
        comms_send_pi("FAULT:eeprom_read_failed");
        led_red(true);
        currentState = STATE_IDLE;
        return;
    }
    
    // Step 4 - cross check type
    if (profile.typeId != typeId) {
        Serial.printf("Type mismatch: CC1=%d EEPROM=%d\n", 
            typeId, profile.typeId);
        comms_send_pi("FAULT:type_mismatch");
        led_red(true);
        currentState = STATE_IDLE;
        return;
    }
    
    // Step 5 - store profile
    currentModule = profile;
    maxCurrentMa = profile.maxCurrentMa;
    
    // Step 6 - enable power
    power_enable();
    lastHeartbeatTime = millis();
    
    // Step 7 - notify Pi
    char message[128];
    snprintf(message, sizeof(message),
        "MODULE_CONNECTED:type=%d:version=%d:current_limit=%d:name=%s:ready",
        profile.typeId,
        profile.fwVersion,
        profile.maxCurrentMa,
        profile.name
    );
    comms_send_pi(message);
    
    led_green(false);
    led_blue(true);
    led_red(false);
    
    currentState = STATE_RUNNING;
    Serial.println("Module connected successfully");
}

void running_loop() {
    static unsigned long lastCurrentCheck = 0;
    static unsigned long lastPlugCheck = 0;
    unsigned long now = millis();
    
    // Check current every 50ms
    if (now - lastCurrentCheck > 50) {
        lastCurrentCheck = now;
        float current = power_read_current_ma();
        
        if (current > maxCurrentMa) {
            Serial.printf("Overcurrent: %.1fmA limit=%d\n", 
                current, maxCurrentMa);
            power_disable();
            led_blue(false);
            led_red(true);
            comms_send_pi("FAULT:overcurrent");
            currentState = STATE_FAULT;
            return;
        }
    }
    
    // Check heartbeat every 100ms
    if (now - lastHeartbeatTime > 1000) {
        Serial.println("Heartbeat timeout");
        power_disable();
        led_blue(false);
        led_red(true);
        comms_send_pi("FAULT:heartbeat_timeout");
        currentState = STATE_FAULT;
        return;
    }
    
    // Check for plug-out every 200ms
    if (now - lastPlugCheck > 200) {
        lastPlugCheck = now;
        float voltage = read_cc1_voltage();
        
        if (voltage > 3.0f) {
            Serial.println("Module disconnected");
            power_disable();
            led_blue(false);
            comms_send_pi("MODULE_DISCONNECTED");
            currentState = STATE_IDLE;
            return;
        }
    }
}

void fault_loop() {
    // Flash red LED
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    if (millis() - lastBlink > 500) {
        lastBlink = millis();
        ledState = !ledState;
        led_red(ledState);
    }
    
    // Wait for module to be unplugged
    float voltage = read_cc1_voltage();
    if (voltage > 3.0f) {
        led_red(false);
        currentState = STATE_IDLE;
    }
}