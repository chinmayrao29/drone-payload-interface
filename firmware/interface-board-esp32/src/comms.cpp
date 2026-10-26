#include "comms.h"
#include "soc/rtc_cntl_reg.h"

// The Pi header (J3) and debug header (J4) share UART0 (GPIO43/44),
// so the Pi link uses Serial, the same port as debug output.
static String pendingPi = "";

void comms_init() {
    Serial.setTimeout(50);

    // UART to module STM32
    Serial2.begin(MODULE_BAUD, SERIAL_8N1, PIN_MOD_RX, PIN_MOD_TX);

    Serial.println("UART comms initialized");
}

// Reads incoming lines. "ENTER_BOOTLOADER" reboots into flashing mode.
void comms_poll() {
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line == "ENTER_BOOTLOADER") {
            Serial.println("Rebooting into bootloader...");
            Serial.flush();
            REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
            esp_restart();
        }
        if (line.length() > 0) {
            pendingPi = line;
        }
    }
}

void comms_send_pi(const char* message) {
    Serial.println(message);
}

void comms_send_module(const char* message) {
    Serial2.println(message);
}

String comms_read_pi() {
    comms_poll();
    String line = pendingPi;
    pendingPi = "";
    return line;
}

String comms_read_module() {
    if (Serial2.available()) {
        return Serial2.readStringUntil('\n');
    }
    return "";
}