#include "tcc_gpio.h"

void tcc_gpio_init(int i_pin, gpio_direction_e e_direction)
{
	int i_retval;

	if(!gpio_is_valid(i_pin))
	{
		printk("[%s:%d] pin(0x%X) error\n", __func__, __LINE__, i_pin);
		return;
	}

	i_retval = gpio_request(i_pin, NULL);

	if(i_retval != 0)
	{
		printk("[%s:%d] pin(0x%X) error(%d)\n", __func__, __LINE__, i_pin, i_retval);
		return;
	}

	switch(e_direction)
	{
	case GPIO_DIRECTION_INPUT:
		gpio_direction_input(i_pin);
		break;
	case GPIO_DIRECTION_OUTPUT:
		gpio_direction_output(i_pin, 0);
		break;
	default:
		break;
	}
}

void tcc_gpio_set(int i_pin, int i_value)
{
	if(!gpio_is_valid(i_pin))
	{
		printk("[%s:%d] pin(0x%X) error\n", __func__, __LINE__, i_pin);
		return;
	}

	if(gpio_cansleep(i_pin))
	{
		gpio_set_value_cansleep(i_pin, i_value);
	}
	else
	{
		gpio_set_value(i_pin, i_value);
	}
}

int tcc_gpio_get(int i_pin)
{
	if(!gpio_is_valid(i_pin))
	{
		printk("[%s:%d] pin(0x%X) error\n", __func__, __LINE__, i_pin);
		return -1;
	}

	if(gpio_cansleep(i_pin))
	{
		return gpio_get_value_cansleep(i_pin);
	}

	return gpio_get_value(i_pin);
}

void tcc_gpio_free(int i_pin)
{
	if(gpio_is_valid(i_pin))
	{
		gpio_free(i_pin);
	}
}
