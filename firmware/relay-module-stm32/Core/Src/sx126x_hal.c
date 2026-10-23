/**
 * @file    sx126x_hal.c
 *
 * @brief   STM32F103 HAL implementation for the SX126x driver (E22-900M30S module)
 *
 * Pin mapping (relay module, U6 = E22-900M30S):
 *   NSS   -> PB0  (software-controlled GPIO, SPI1 Hardware NSS = Disable)
 *   RESET -> PB1
 *   BUSY  -> PA1
 *   SPI1  -> PA5 (SCK), PA6 (MISO), PA7 (MOSI)
 *
 * This file implements the four functions required by sx126x_hal.h.
 * It does not modify sx126x.c / sx126x_regs.h, which are used as-is from
 * the official Lora-net/sx126x_driver repository.
 */

#include "sx126x_hal.h"
#include "main.h"     /* gives us hspi1 handle + CubeMX-generated pin labels */
#include <string.h>

/* -----------------------------------------------------------------------
 * Pin / peripheral handles — these match the labels CubeMX already
 * generated for LORA_NSS_Pin / LORA_RESET_Pin (see MX_GPIO_Init in
 * main.c). BUSY (PA1) is NOT yet configured in MX_GPIO_Init — add a
 * pin labeled "LORA_BUSY" in CubeMX (GPIO_MODE_INPUT, no pull) and
 * regenerate so LORA_BUSY_Pin / LORA_BUSY_GPIO_Port exist. Until then
 * this file will not compile.
 * ---------------------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;

#define SPI_TIMEOUT_MS    1000

/* Safety timeout while waiting for BUSY to go low, so a hardware fault
 * (e.g. bad solder joint on BUSY) hangs gracefully instead of forever. */
#define BUSY_WAIT_TIMEOUT_MS  5000

/* -----------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------*/

static inline void nss_low( void )
{
    HAL_GPIO_WritePin( LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET );
}

static inline void nss_high( void )
{
    HAL_GPIO_WritePin( LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET );
}

/**
 * @brief Block until BUSY (PA1, label LORA_BUSY in CubeMX) goes low, or
 *        time out.
 *
 * The SX1262 pulls BUSY high while it is internally processing the last
 * command. Starting a new SPI transaction before BUSY drops can corrupt
 * or drop that command, so every write/read must wait here first.
 *
 * @return true if BUSY went low within the timeout, false on timeout.
 */
static bool wait_on_busy( void )
{
    uint32_t start = HAL_GetTick( );

    while( HAL_GPIO_ReadPin( LORA_BUSY_GPIO_Port, LORA_BUSY_Pin ) == GPIO_PIN_SET )
    {
        if( ( HAL_GetTick( ) - start ) > BUSY_WAIT_TIMEOUT_MS )
        {
            return false;
        }
    }
    return true;
}

/* -----------------------------------------------------------------------
 * Public HAL functions required by sx126x_hal.h
 * ---------------------------------------------------------------------*/

sx126x_hal_status_t sx126x_hal_write( const void* context, const uint8_t* command, const uint16_t command_length,
                                       const uint8_t* data, const uint16_t data_length )
{
    ( void ) context; /* unused on this single-radio board */

    if( !wait_on_busy( ) )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    nss_low( );

    HAL_StatusTypeDef status = HAL_OK;

    if( command_length > 0 )
    {
        status = HAL_SPI_Transmit( &hspi1, ( uint8_t* ) command, command_length, SPI_TIMEOUT_MS );
    }

    if( ( status == HAL_OK ) && ( data_length > 0 ) )
    {
        status = HAL_SPI_Transmit( &hspi1, ( uint8_t* ) data, data_length, SPI_TIMEOUT_MS );
    }

    nss_high( );

    return ( status == HAL_OK ) ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_read( const void* context, const uint8_t* command, const uint16_t command_length,
                                      uint8_t* data, const uint16_t data_length )
{
    ( void ) context;

    if( !wait_on_busy( ) )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    nss_low( );

    HAL_StatusTypeDef status = HAL_OK;

    if( command_length > 0 )
    {
        status = HAL_SPI_Transmit( &hspi1, ( uint8_t* ) command, command_length, SPI_TIMEOUT_MS );
    }

    if( ( status == HAL_OK ) && ( data_length > 0 ) )
    {
        /* Clock out NOP bytes while clocking in the response, per the
         * SX126x datasheet read sequence. */
        uint8_t nop = SX126X_NOP;
        for( uint16_t i = 0; i < data_length; i++ )
        {
            status = HAL_SPI_TransmitReceive( &hspi1, &nop, &data[i], 1, SPI_TIMEOUT_MS );
            if( status != HAL_OK )
            {
                break;
            }
        }
    }

    nss_high( );

    return ( status == HAL_OK ) ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_reset( const void* context )
{
    ( void ) context;

    /* RST is active low on the E22-900M30S (per EBYTE datasheet). */
    HAL_GPIO_WritePin( LORA_RESET_GPIO_Port, LORA_RESET_Pin, GPIO_PIN_RESET );
    HAL_Delay( 5 );   /* hold reset low briefly */
    HAL_GPIO_WritePin( LORA_RESET_GPIO_Port, LORA_RESET_Pin, GPIO_PIN_SET );
    HAL_Delay( 10 );  /* allow chip to boot before issuing commands */

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup( const void* context )
{
    ( void ) context;

    /* A brief NSS low/high pulse wakes the radio from sleep mode. */
    nss_low( );
    HAL_Delay( 1 );
    nss_high( );

    if( !wait_on_busy( ) )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    return SX126X_HAL_STATUS_OK;
}

/* --- EOF ------------------------------------------------------------------ */
