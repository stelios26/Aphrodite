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
#include "nrf_adc.h"
//#include "app_timer.h"
//#include "bsp.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_drv_gpiote.h"

//#include "debug.h"
#include "led.h"

#ifndef NRF_APP_PRIORITY_HIGH
#define NRF_APP_PRIORITY_HIGH 1
#endif

#define LED_PIN_RED     23      	/**< RGB LED Red pin */
#define LED_PIN_GREEN   30      /**< RGB LED Green pin */
#define LED_PIN_BLUE    3       /**< RGB LED Blue pin */

extern int32_t adc_sample;

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
		
		nrf_gpio_pin_dir_set(2, NRF_GPIO_PIN_DIR_INPUT);
		nrf_gpio_cfg_input(2, NRF_GPIO_PIN_PULLUP);
}

//ADC initialization
void adc_init(void)
{	
	  const nrf_adc_config_t nrf_adc_config = NRF_ADC_CONFIG_DEFAULT;

    // Initialize and configure ADC
    nrf_adc_configure( (nrf_adc_config_t *)&nrf_adc_config);
    nrf_adc_input_select(NRF_ADC_CONFIG_INPUT_2);
    nrf_adc_int_enable(ADC_INTENSET_END_Enabled << ADC_INTENSET_END_Pos);
    NVIC_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_HIGH);
    NVIC_EnableIRQ(ADC_IRQn);
}

/* Interrupt handler for ADC data ready event */
void ADC_IRQHandler(void)
{
    nrf_adc_conversion_event_clean();

    adc_sample = nrf_adc_result_get();
}	
/** @} */
