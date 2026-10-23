/**
 * @file    eeprom_programmer_at24c32.ino
 *
 * @brief   One-time EEPROM programmer for the Aelstra comms relay module.
 *          For AT24C32 on DS3231 RTC module, using ESP32-S3 with SDA=GPIO8, SCL=GPIO9.
 *
 * Wiring:
 *   DS3231 module VCC -> 3.3V
 *   DS3231 module GND -> GND
 *   DS3231 module SDA -> GPIO8
 *   DS3231 module SCL -> GPIO9
 *
 * ModuleProfile memory layout (matches detection.h exactly):
 *   Byte  0      typeId        = 0x06 (comms relay)
 *   Byte  1      hwVersion     = 1
 *   Byte  2      fwVersion     = 1
 *   Byte  3-4    maxCurrentMa  = 800 (0x0320, little-endian: 0x20 at byte 3, 0x03 at byte 4)
 *   Byte  5      baudCode      = 1 (115200)
 *   Byte  6-7    reserved      = 0x00 0x00
 *   Bytes 8-31   name          = "comms_relay\0" + padding zeros
 */

#include <Arduino.h>
#include <Wire.h>

#define EEPROM_ADDR  0x57   // AT24C32 on DS3231 module with A0/A1/A2 = GND
#define EEPROM_SIZE  32

#define SDA_PIN      8
#define SCL_PIN      9

const uint8_t MODULE_DATA[EEPROM_SIZE] = {
    0x06,                    // Byte  0: typeId = MODULE_RELAY
    0x01,                    // Byte  1: hwVersion = 1
    0x01,                    // Byte  2: fwVersion = 1
    0x20,                    // Byte  3: maxCurrentMa low byte  (800 = 0x0320)
    0x03,                    // Byte  4: maxCurrentMa high byte
    0x01,                    // Byte  5: baudCode = 1 (115200)
    0x00,                    // Byte  6: reserved[0]
    0x00,                    // Byte  7: reserved[1]
    'c','o','m','m',         // Bytes 8-11
    's','_','r','e',         // Bytes 12-15
    'l','a','y',0x00,        // Bytes 16-19: "comms_relay\0"
    0x00,0x00,0x00,0x00,     // Bytes 20-23: padding
    0x00,0x00,0x00,0x00,     // Bytes 24-27: padding
    0x00,0x00,0x00,0x00      // Bytes 28-31: padding
};

bool eeprom_write_byte( uint16_t mem_addr, uint8_t data )
{
    Wire.beginTransmission( EEPROM_ADDR );
    Wire.write( (uint8_t)(mem_addr >> 8) );    // high address byte
    Wire.write( (uint8_t)(mem_addr & 0xFF) );  // low address byte
    Wire.write( data );
    uint8_t result = Wire.endTransmission();
    delay( 5 );  // AT24C32 write cycle time
    return ( result == 0 );
}

uint8_t eeprom_read_byte( uint16_t mem_addr )
{
    Wire.beginTransmission( EEPROM_ADDR );
    Wire.write( (uint8_t)(mem_addr >> 8) );
    Wire.write( (uint8_t)(mem_addr & 0xFF) );
    Wire.endTransmission();
    Wire.requestFrom( EEPROM_ADDR, 1 );
    return Wire.available() ? Wire.read() : 0xFF;
}

bool program_eeprom()
{
    Serial.println( "Writing EEPROM..." );

    for ( uint8_t i = 0; i < EEPROM_SIZE; i++ )
    {
        if ( !eeprom_write_byte( i, MODULE_DATA[i] ) )
        {
            Serial.print( "WRITE FAILED at byte " );
            Serial.println( i );
            return false;
        }
    }

    Serial.println( "Write complete. Verifying..." );

    bool ok = true;
    for ( uint8_t i = 0; i < EEPROM_SIZE; i++ )
    {
        uint8_t readback = eeprom_read_byte( i );
        if ( readback != MODULE_DATA[i] )
        {
            Serial.print( "VERIFY FAILED at byte " );
            Serial.print( i );
            Serial.print( ": wrote 0x" );
            Serial.print( MODULE_DATA[i], HEX );
            Serial.print( " but read 0x" );
            Serial.println( readback, HEX );
            ok = false;
        }
    }
    return ok;
}

void dump_eeprom()
{
    Serial.println( "\n--- EEPROM contents ---" );

    uint8_t buf[EEPROM_SIZE];

    Wire.beginTransmission( EEPROM_ADDR );
    Wire.write( 0x00 );
    Wire.write( 0x00 );
    Wire.endTransmission();
    Wire.requestFrom( EEPROM_ADDR, EEPROM_SIZE );

    for ( uint8_t i = 0; i < EEPROM_SIZE; i++ )
    {
        buf[i] = Wire.available() ? Wire.read() : 0xFF;
    }

    Serial.print( "typeId:       0x" ); Serial.println( buf[0], HEX );
    Serial.print( "hwVersion:    " );   Serial.println( buf[1] );
    Serial.print( "fwVersion:    " );   Serial.println( buf[2] );

    uint16_t maxCurrent = ( (uint16_t)buf[4] << 8 ) | buf[3];
    Serial.print( "maxCurrentMa: " );   Serial.println( maxCurrent );

    Serial.print( "baudCode:     " );   Serial.println( buf[5] );
    Serial.print( "name:         " );   Serial.println( (char*)&buf[8] );

    Serial.println( "\nRaw bytes:" );
    for ( uint8_t i = 0; i < EEPROM_SIZE; i++ )
    {
        if ( i % 8 == 0 )
        {
            Serial.print( "\n  [" );
            if ( i < 10 ) Serial.print( " " );
            Serial.print( i );
            Serial.print( "] " );
        }
        if ( buf[i] < 0x10 ) Serial.print( "0" );
        Serial.print( buf[i], HEX );
        Serial.print( " " );
    }
    Serial.println( "\n-----------------------" );
}

void setup()
{
    Serial.begin( 115200 );
    delay( 1000 );

    Serial.println( "\n==============================" );
    Serial.println( "EEPROM Programmer" );
    Serial.println( "Relay Module — Type ID 0x06" );
    Serial.println( "AT24C32 on DS3231 module" );
    Serial.println( "==============================" );

    Wire.begin( SDA_PIN, SCL_PIN );  // GPIO8=SDA, GPIO9=SCL

    Wire.beginTransmission( EEPROM_ADDR );
    uint8_t error = Wire.endTransmission();
    if ( error != 0 )
    {
        Serial.println( "ERROR: AT24C32 not found at 0x57." );
        Serial.println( "Check SDA/SCL/VCC/GND wiring. Halting." );
        while ( 1 );
    }
    Serial.println( "AT24C32 found at 0x57." );

    if ( program_eeprom() )
    {
        Serial.println( "\nSUCCESS: All 32 bytes written and verified." );
        dump_eeprom();
        Serial.println( "\nDone. Reflash your main firmware now." );
    }
    else
    {
        Serial.println( "\nFAILED: Check connections and try again." );
    }
}

void loop()
{
    // One-shot — nothing to do here
}
