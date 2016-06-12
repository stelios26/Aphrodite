/*  CCH Beacon
 *  Copyright (c) 2016 Stelios Ioakim.
 *  All Rights Reserved.
 *
 *  Developed by s.ioakim@engino.net
 */

/**
 *  @file
 *  @brief       LED configuration and control
 *  @addtogroup  CCH-Beacon
 *
 *  The beacon has an RGB LED on board which is directly driven by
 *  GPIO pins.
 *  @{
 */

#include <stdint.h>
#include <stdbool.h>

#include "app_error.h"
//#include "app_timer.h"
//#include "bsp.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_drv_gpiote.h"

//#include "debug.h"
#include "led.h"


#define LED_PIN_RED     2      	/**< RGB LED Red pin */
#define LED_PIN_GREEN   30      /**< RGB LED Green pin */
#define LED_PIN_BLUE    3       /**< RGB LED Blue pin */

/** RGB LED pin map */
static uint32_t led_rgb_pin[3] = { LED_PIN_RED, LED_PIN_GREEN, LED_PIN_BLUE };

/**
 *  @brief Set the state of the RGB LED
 *
 *  @param led LED element ID
 *  @param state LED element state True = lit
 */
void led_rgb_set(uint32_t led, bool state)
{
    if(state)
    {
        nrf_drv_gpiote_out_clear(led_rgb_pin[led]);
    }
    else
    {
        nrf_drv_gpiote_out_set(led_rgb_pin[led]);
    }
}


/**
 *  @brief Initialise LED control pins
 */
void led_init()
{
    nrf_drv_gpiote_out_config_t config = GPIOTE_CONFIG_OUT_SIMPLE(false);
    uint32_t                    pin;
    uint32_t                    err_code;

		
    /* Initialise RGB LED */
    for(pin = LED_RED; pin <= LED_BLUE; pin++)
    {
        /* Set pin mode */
        err_code = nrf_drv_gpiote_out_init(led_rgb_pin[pin], &config);
        APP_ERROR_CHECK(err_code);

        /* Configure high drive */
        NRF_GPIO->PIN_CNF[led_rgb_pin[pin]] =
                (GPIO_PIN_CNF_DRIVE_H0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
    }
		
		nrf_gpio_pin_dir_set(23, NRF_GPIO_PIN_DIR_INPUT);
		nrf_gpio_cfg_input(23, NRF_GPIO_PIN_NOPULL);
}
/** @} */
