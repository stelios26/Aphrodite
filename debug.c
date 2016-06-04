/*  CCH Beacon
 *  Copyright (c) 2016 Stelios Ioakim.
 *  All Rights Reserved.
 *
 *  Developed by s.ioakim@engino.net
 */

/**
 *  @file
 *  @brief       Serial debugging
 *  @addtogroup  CCH-beacon
 *
 *  @{
 */

#if defined(DEBUG_UART)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "app_util_platform.h"
#include "nrf_drv_uart.h"

/**
 *  @brief Output debug string
 *
 *  @param fmt Formatting strong
 */
void debug_printf(char *fmt, ...)
{
    char buffer[256];
    uint32_t size;

    va_list args;
    va_start(args, fmt);
    size = vsnprintf (buffer, 256, fmt, args);
    va_end(args);

    if(size > 0) nrf_drv_uart_tx((uint8_t *)buffer, size);
}

/**
 *  @brief Initialise serial port
 */
void debug_init()
{
    /* Initialise debugging port */
    nrf_drv_uart_config_t uartconfig = NRF_DRV_UART_DEFAULT_CONFIG;
    nrf_drv_uart_init(&uartconfig, NULL);
}

#else
void debug_printf(char *fmt, ...) {}
void debug_init() {}
#endif
/** @} */
