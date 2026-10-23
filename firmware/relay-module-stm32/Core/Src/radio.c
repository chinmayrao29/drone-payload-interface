/**
 * @file    radio.c
 *
 * @brief   Implementation of the LoRa_* wrapper around sx126x_driver.
 *
 * All sx126x_* calls used here are taken directly from the vendor
 * sx126x.h (Lora-net/sx126x_driver). Nothing in this file guesses at
 * a signature — every call matches the header exactly.
 */

#include "radio.h"
#include "sx126x.h"
#include "sx126x_hal.h"   /* for sx126x_hal_status_t, not called directly here */
#include "main.h"         /* PA_TXEN_Pin/_GPIO_Port, plus RXEN labels (see note below) */
#include <string.h>

/* -----------------------------------------------------------------------
 * RXEN handling
 *
 * NOTE: as of the last GPIO_Init you shared, only PA_TXEN_Pin exists.
 * RXEN (PA0) needs the same treatment BUSY got: add a GPIO_Output pin in
 * CubeMX labeled e.g. "LORA_RXEN", regenerate, and the
 * LORA_RXEN_Pin / LORA_RXEN_GPIO_Port defines below will resolve. Until
 * then this file will not compile — that's intentional, not a bug, so the
 * missing pin config gets caught at compile time instead of silently
 * misbehaving on hardware.
 * ---------------------------------------------------------------------*/

/* The "context" argument every sx126x_* call takes is a void* the vendor
 * driver passes straight through to your sx126x_hal_* functions. This
 * board only ever has one radio, so we don't need it to carry any real
 * data — NULL is fine, and our sx126x_hal.c already ignores it. */
#define RADIO_CONTEXT  ( NULL )

static uint8_t s_rx_buf_internal[LORA_MAX_PKT_SIZE];

/* -----------------------------------------------------------------------
 * Internal helpers — RF switch control
 * ---------------------------------------------------------------------*/

static void rf_switch_idle( void )
{
    HAL_GPIO_WritePin( PA_TXEN_GPIO_Port, PA_TXEN_Pin, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( LORA_RXEN_GPIO_Port, LORA_RXEN_Pin, GPIO_PIN_RESET );
}

static void rf_switch_rx( void )
{
    HAL_GPIO_WritePin( PA_TXEN_GPIO_Port, PA_TXEN_Pin, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( LORA_RXEN_GPIO_Port, LORA_RXEN_Pin, GPIO_PIN_SET );
}

static void rf_switch_tx( void )
{
    HAL_GPIO_WritePin( LORA_RXEN_GPIO_Port, LORA_RXEN_Pin, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( PA_TXEN_GPIO_Port, PA_TXEN_Pin, GPIO_PIN_SET );
}

/* -----------------------------------------------------------------------
 * LoRa_Init
 * ---------------------------------------------------------------------*/

uint8_t LoRa_Init( void )
{
    rf_switch_idle( );

    /* Hardware reset + wake, per sx126x.h "Miscellaneous" functions */
    if( sx126x_reset( RADIO_CONTEXT ) != SX126X_STATUS_OK )
    {
        return 0;
    }
    if( sx126x_wakeup( RADIO_CONTEXT ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* Standby on RC oscillator before configuring anything else —
     * required state per sx126x_cal()'s documented precondition and
     * general init order used throughout the vendor driver. */
    if( sx126x_set_standby( RADIO_CONTEXT, SX126X_STANDBY_CFG_RC ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* This board uses the E22's own LDO/regulator arrangement; LDO is
     * the SX126x default and matches what the old Ra-02 setup assumed
     * (no DC/DC inductor on this design), so we set it explicitly
     * rather than relying on chip defaults after reset. */
    if( sx126x_set_reg_mode( RADIO_CONTEXT, SX126X_REG_MODE_LDO ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* Workaround recommended by Semtech: optimizes PA clamping
     * threshold, must be called after reset/cold-start wakeup. */
    sx126x_cfg_tx_clamp( RADIO_CONTEXT );

    /* Run full calibration once at startup. */
    if( sx126x_cal( RADIO_CONTEXT, SX126X_CAL_ALL ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* Packet type must be set before frequency / modulation / packet
     * params, per the header's documented call order. */
    if( sx126x_set_pkt_type( RADIO_CONTEXT, SX126X_PKT_TYPE_LORA ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* 868 MHz, matching your original design brief frequency. */
    if( sx126x_set_rf_freq( RADIO_CONTEXT, 868000000UL ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* SF7 / BW125 / CR4-5, same parameters as the original Ra-02 setup,
     * LDRO off (not needed at SF7/BW125 — LDRO is only recommended for
     * long symbol durations at higher SF / lower BW). */
    sx126x_mod_params_lora_t mod_params = {
        .sf   = SX126X_LORA_SF7,
        .bw   = SX126X_LORA_BW_125,
        .cr   = SX126X_LORA_CR_4_5,
        .ldro = 0,
    };
    if( sx126x_set_lora_mod_params( RADIO_CONTEXT, &mod_params ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* Explicit header, variable length, CRC on — matches old config. */
    sx126x_pkt_params_lora_t pkt_params = {
        .preamble_len_in_symb = 8,
        .header_type          = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes     = LORA_MAX_PKT_SIZE,
        .crc_is_on            = true,
        .invert_iq_is_on      = false,
    };
    if( sx126x_set_lora_pkt_params( RADIO_CONTEXT, &pkt_params ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* Sync word 0x12 — private network value, matches original config. */
    if( sx126x_set_lora_sync_word( RADIO_CONTEXT, 0x12 ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* TX power: E22-900M30S datasheet rates it up to 30dBm (1W). Start
     * at a conservative 22dBm for bring-up / bench testing — raise once
     * basic link is confirmed working, and only after confirming your
     * antenna/SWR situation, since 30dBm into a bad match stresses the
     * PA. Ramp time 200us is a safe, commonly-used default. */
    if( sx126x_set_tx_params( RADIO_CONTEXT, 22, SX126X_RAMP_200_US ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* Tx/Rx buffer base addresses — single-buffer use, both at 0. */
    if( sx126x_set_buffer_base_address( RADIO_CONTEXT, 0x00, 0x00 ) != SX126X_STATUS_OK )
    {
        return 0;
    }

    /* IMPORTANT: this board manually drives RXEN/TXEN via GPIO (see
     * rf_switch_* helpers above), so DIO2-as-RF-switch-control stays
     * disabled — letting the chip also try to drive the switch via DIO2
     * would fight with our manual control. */
    sx126x_set_dio2_as_rf_sw_ctrl( RADIO_CONTEXT, false );

    /* Route RxDone, TxDone, Timeout and CRC error onto DIO1 (the pin
     * wired to STM32 PB2 / EXTI2). header_valid/crc_error are useful for
     * debugging dropped packets but the relay logic only needs to know
     * about RxDone/TxDone/Timeout to drive its state machine. */
    sx126x_irq_mask_t irq_mask = SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_DONE |
                                  SX126X_IRQ_TIMEOUT | SX126X_IRQ_CRC_ERROR;
    if( sx126x_set_dio_irq_params( RADIO_CONTEXT, irq_mask, irq_mask, SX126X_IRQ_NONE, SX126X_IRQ_NONE ) !=
        SX126X_STATUS_OK )
    {
        return 0;
    }
    sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );

    return 1;
}

/* -----------------------------------------------------------------------
 * LoRa_StartReceive
 * ---------------------------------------------------------------------*/

void LoRa_StartReceive( void )
{
    sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );
    rf_switch_rx( );
    sx126x_set_rx( RADIO_CONTEXT, SX126X_RX_CONTINUOUS );
}

/* -----------------------------------------------------------------------
 * LoRa_Send — blocking transmit
 * ---------------------------------------------------------------------*/

void LoRa_Send( uint8_t* data, uint8_t length )
{
    if( length > LORA_MAX_PKT_SIZE )
    {
        length = LORA_MAX_PKT_SIZE;
    }

    /* Payload length can vary per-packet even with header_type EXPLICIT,
     * so update pld_len_in_bytes for this specific send. */
    sx126x_pkt_params_lora_t pkt_params = {
        .preamble_len_in_symb = 8,
        .header_type          = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes     = length,
        .crc_is_on            = true,
        .invert_iq_is_on      = false,
    };
    sx126x_set_lora_pkt_params( RADIO_CONTEXT, &pkt_params );

    sx126x_write_buffer( RADIO_CONTEXT, 0x00, data, length );

    sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );
    rf_switch_tx( );

    /* 5 second TX timeout as a safety net against a hung transmission;
     * the main loop's lora_rx_flag / IRQ handling will normally see
     * TxDone well before this and move on. */
    sx126x_set_tx( RADIO_CONTEXT, 5000 );

    /* Block here waiting for TxDone or Timeout on DIO1. This mirrors the
     * old driver's blocking LoRa_Send() behavior so main.c's call sites
     * don't need restructuring. BUSY-pin waiting inside sx126x_hal.c
     * already protects each individual SPI transaction; this loop is
     * purely waiting for the *radio operation* (the actual over-the-air
     * transmission) to finish, which is a separate, longer timescale. */
    sx126x_irq_mask_t irq = SX126X_IRQ_NONE;
    uint32_t           start = HAL_GetTick( );
    while( ( irq & ( SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT ) ) == 0 )
    {
        sx126x_get_irq_status( RADIO_CONTEXT, &irq );
        if( ( HAL_GetTick( ) - start ) > 5500 )
        {
            break; /* belt-and-braces in case the chip's own timeout
                      somehow doesn't fire an IRQ we catch */
        }
    }
    sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );

    rf_switch_idle( );
}

/* -----------------------------------------------------------------------
 * LoRa_Receive — called once DIO1 has signaled RxDone
 * ---------------------------------------------------------------------*/

bool LoRa_Receive( uint8_t* data, uint8_t* length )
{
    sx126x_irq_mask_t irq = SX126X_IRQ_NONE;
    sx126x_get_irq_status( RADIO_CONTEXT, &irq );

    if( ( irq & SX126X_IRQ_RX_DONE ) == 0 )
    {
        sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );
        return false;
    }

    if( irq & SX126X_IRQ_CRC_ERROR )
    {
        sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );
        return false;
    }

    sx126x_rx_buffer_status_t rx_status;
    if( sx126x_get_rx_buffer_status( RADIO_CONTEXT, &rx_status ) != SX126X_STATUS_OK )
    {
        sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );
        return false;
    }

    uint8_t len = rx_status.pld_len_in_bytes;
    if( len > LORA_MAX_PKT_SIZE )
    {
        len = LORA_MAX_PKT_SIZE;
    }

    sx126x_read_buffer( RADIO_CONTEXT, rx_status.buffer_start_pointer, s_rx_buf_internal, len );
    memcpy( data, s_rx_buf_internal, len );
    *length = len;

    /* Per sx126x.h: call this after any reception sequence. */
    sx126x_handle_rx_done( RADIO_CONTEXT );

    sx126x_clear_irq_status( RADIO_CONTEXT, SX126X_IRQ_ALL );

    return true;
}

/* -----------------------------------------------------------------------
 * LoRa_GetRSSI
 * ---------------------------------------------------------------------*/

int16_t LoRa_GetRSSI( void )
{
    sx126x_pkt_status_lora_t pkt_status;
    if( sx126x_get_lora_pkt_status( RADIO_CONTEXT, &pkt_status ) != SX126X_STATUS_OK )
    {
        return 0;
    }
    return ( int16_t ) pkt_status.rssi_pkt_in_dbm;
}
