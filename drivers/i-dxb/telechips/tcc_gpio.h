#ifndef __TCC_GPIO_H__
#define __TCC_GPIO_H__

#include <linux/delay.h>
#include <linux/of_gpio.h>

typedef enum tag_gpio_direction_e
{
	GPIO_DIRECTION_INPUT, 
	GPIO_DIRECTION_OUTPUT
} gpio_direction_e;

void tcc_gpio_init(int i_pin, gpio_direction_e e_direction);
void tcc_gpio_set(int i_pin, int i_value);
int tcc_gpio_get(int i_pin);
void tcc_gpio_free(int i_pin);

#endif	// __TCC_GPIO_H__