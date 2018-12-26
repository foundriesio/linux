#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <linux/i-dxb/silabs/si2166.h>

#include "tcc_power_silabs.h"

typedef enum tag_gpio_direction_e
{
	GPIO_DIRECTION_INPUT, 
	GPIO_DIRECTION_OUTPUT
} gpio_direction_e;

typedef struct tag_fe_gpio_t
{
	int gpio_rst;
} fe_gpio_t;

typedef struct tag_silabs_power_t
{
	int i_n_fe_gpio;

	fe_gpio_t *p_fe_gpio;
} silabs_power_t;

static silabs_power_t *gp_instance = NULL;

static void sfn_gpio_init(int i_pin, gpio_direction_e e_direction)
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

static void sfn_gpio_set(int i_pin, int i_value)
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

static int sfn_gpio_get(int i_pin)
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

static void sfn_gpio_free(int i_pin)
{
	if(gpio_is_valid(i_pin))
	{
		gpio_free(i_pin);
	}
}

int tcc_power_silabs_on(void)
{
	int i;
	
	silabs_power_t *p_instance = gp_instance;

	if(p_instance == NULL)
	{
		printk("[%s:%d] instance is NULL!\n", __func__, __LINE__);
		return -1;
	}

	for(i = 0; i < p_instance->i_n_fe_gpio; i++)
	{
		fe_gpio_t *p_fe_gpio = &p_instance->p_fe_gpio[i];

		sfn_gpio_set(p_fe_gpio->gpio_rst, 0);

		msleep(20);

		sfn_gpio_set(p_fe_gpio->gpio_rst, 0);

		msleep(20);

		sfn_gpio_set(p_fe_gpio->gpio_rst, 1);
	}	
	
	return 0;
}

int tcc_power_silabs_off(void)
{
	int i;
	
	silabs_power_t *p_instance = gp_instance;

	if(p_instance == NULL)
	{
		printk("[%s:%d] instance is NULL!\n", __func__, __LINE__);
		return -1;
	}

	for(i = 0; i < p_instance->i_n_fe_gpio; i++)
	{
		fe_gpio_t *p_fe_gpio = &p_instance->p_fe_gpio[i];

		sfn_gpio_set(p_fe_gpio->gpio_rst, 0);
	}
	
	return 0;
}

int tcc_power_silabs_init(silabs_fe_type_e e_type)
{
	int i;

	char str_name[20];

	struct device_node *p_dnode;
	
	silabs_power_t *p_instance = NULL;

	if(e_type >= SILABS_FE_TYPE_MAX)
	{
		printk("[%s:%d] fe type error! - %d\n", __func__, __LINE__, e_type);
		goto error;
	}
	
	p_instance = (silabs_power_t *)kcalloc(1, sizeof(silabs_power_t), GFP_KERNEL);

	if(p_instance == NULL)
	{
		printk("[%s:%d] kcalloc() failed!\n", __func__, __LINE__);
		goto error;
	}

	p_instance->i_n_fe_gpio = (int)(e_type + 1);

	p_instance->p_fe_gpio = (fe_gpio_t *)kcalloc(p_instance->i_n_fe_gpio, sizeof(fe_gpio_t), GFP_KERNEL);

	if(p_instance->p_fe_gpio == NULL)
	{
		printk("[%s:%d] kcalloc() failed!\n", __func__, __LINE__);
		goto error;
	}

	p_dnode = of_find_compatible_node(NULL, NULL, "telechips,si_power");

	if(p_dnode == NULL)
	{
		printk("[%s:%d] of_find_compatible_node() failed!\n", __func__, __LINE__);
		goto error;
	}
	
	for(i = 0; i < p_instance->i_n_fe_gpio; i++)
	{
		fe_gpio_t *p_fe_gpio = &p_instance->p_fe_gpio[i];

		sprintf(str_name, "si%d-gpios", i);

		p_fe_gpio->gpio_rst = of_get_named_gpio(p_dnode, str_name, 1);

		printk("[%s:%d] gpi rst : [%d][0x%x]\n", __func__, __LINE__, i, p_fe_gpio->gpio_rst);

		sfn_gpio_init(p_fe_gpio->gpio_rst, GPIO_DIRECTION_OUTPUT);
	}

	gp_instance = p_instance;

	return 0;

error:

	if(p_instance->p_fe_gpio != NULL)
	{
		kfree(p_instance->p_fe_gpio);
	}

	if(p_instance != NULL)
	{
		kfree(p_instance);
	}

	return -1;
}

int tcc_power_silabs_deinit(void)
{
	int i;
	
	silabs_power_t *p_instance = gp_instance;

	if(p_instance == NULL)
	{
		printk("[%s:%d] instance is NULL!\n", __func__, __LINE__);
		return -1;
	}

	for(i = 0; i < p_instance->i_n_fe_gpio; i++)
	{
		fe_gpio_t *p_fe_gpio = &p_instance->p_fe_gpio[i];

		sfn_gpio_free(p_fe_gpio->gpio_rst);
	}

	kfree(p_instance->p_fe_gpio);

	kfree(p_instance);

	gp_instance = NULL;

	return 0;
}