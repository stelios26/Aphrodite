#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "led.h"

int main(void)
{
	uint32_t err_code;
	
	/* Initialise GPIO subsystem */
	err_code = nrf_drv_gpiote_init();
	APP_ERROR_CHECK(err_code);

	led_init();

	led_rgb_set(LED_GREEN, false);
	led_rgb_set(LED_BLUE, false);
	
  while (true)
  {
		led_rgb_set(LED_RED, true);
		
    nrf_delay_ms(500);
		
		led_rgb_set(LED_RED, false);
		
		nrf_delay_ms(500);
	}
}
