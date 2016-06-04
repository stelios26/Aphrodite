#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"

int main(void)
{
	nrf_gpio_cfg_output(2);
	nrf_gpio_cfg_output(3);
	nrf_gpio_cfg_output(30);

  while (true)
  {
		nrf_gpio_pin_set(2);
		nrf_gpio_pin_set(3);
		nrf_gpio_pin_set(30);
		
    nrf_delay_ms(500);
		
		nrf_gpio_pin_clear(2);
		nrf_gpio_pin_clear(3);
		nrf_gpio_pin_clear(30);
		
		nrf_delay_ms(500);
	}
}
