#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_error.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "led.h"
#include "debug.h"

int main(void)
{
	uint32_t err_code;
	
	/* Initialise debugging port */
  debug_init();
  debug_printf("Debug port opened... Initialising...\r\n");
	
	/* Initialise GPIO subsystem */
	err_code = nrf_drv_gpiote_init();
	if (err_code == NRF_SUCCESS)
		debug_printf("GPIOTE initialised!\r\n");
	APP_ERROR_CHECK(err_code);

	led_init();
	debug_printf("Visual feedback should be active!\r\n");
	
	led_rgb_set(LED_GREEN, false);
	led_rgb_set(LED_BLUE, false);
	
	debug_printf("Ready.. \r\n");
	
  while (true)
  {
		led_rgb_set(LED_RED, true);
		
    nrf_delay_ms(500);
		
		led_rgb_set(LED_RED, false);
		
		nrf_delay_ms(500);
		debug_printf(".");
	}
}
