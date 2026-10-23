/**
 * @file    radio.h
 *
 * @brief   Thin wrapper around the vendor sx126x_driver, exposing the same
 *          simple LoRa_* interface your main.c already calls (replaces the
 *          old lora.h written for the Ra-02/SX1276).
 *
 * Hardware: EBYTE E22-900M30S (SX1262 + integrated PA/LNA), 868MHz.
 */

#ifndef RADIO_H
#define RADIO_H

#include <stdint.h>
#include <stdbool.h>

#define LORA_MAX_PKT_SIZE  64

/**
 * @brief Full radio + RF-switch init: hardware reset, SX1262 config for
 *        868MHz / SF7 / BW125 / CR4-5 / CRC on / sync word 0x12, and sets
 *        RXEN/TXEN pins to a known (RX-off, TX-off) idle state.
 *
 * @return 1 on success, 0 on failure (mirrors old LoRa_Init() return style)
 */
uint8_t LoRa_Init( void );

/**
 * @brief Put the radio into continuous RX mode. Also drives RXEN high /
 *        TXEN low so the E22's RF switch is actually pointed at the LNA.
 */
void LoRa_StartReceive( void );

/**
 * @brief Blocking send of a packet. Drives TXEN high / RXEN low for the
 *        duration of the transmission, then restores RX state is left to
 *        the caller (call LoRa_StartReceive() again afterwards).
 *
 * @param [in] data    Pointer to payload bytes
 * @param [in] length  Payload length, must be <= LORA_MAX_PKT_SIZE
 */
void LoRa_Send( uint8_t* data, uint8_t length );

/**
 * @brief Call this from the DIO1 EXTI ISR context (or right after it sets
 *        a flag) to actually pull a received packet out of the chip once
 *        RxDone has fired.
 *
 * @param [out] data    Buffer to receive the payload into (caller-owned,
 *                       must be at least LORA_MAX_PKT_SIZE bytes)
 * @param [out] length  Filled with the number of bytes actually received
 *
 * @return true if a valid packet was read out, false otherwise (e.g. CRC
 *         error or no RxDone flag actually set)
 */
bool LoRa_Receive( uint8_t* data, uint8_t* length );

/**
 * @brief RSSI of the last received packet, in dBm.
 */
int16_t LoRa_GetRSSI( void );

#endif  // RADIO_H
