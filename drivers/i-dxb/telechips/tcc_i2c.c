#include "tcc_i2c.h"

#define TCC_I2C_CH	1

struct i2c_adapter *g_p_adapter = NULL;

int tcc_i2c_read(unsigned char uc_addr, int i_byte, unsigned char *puc_buffer)
{
	int i_retval = 0;
	
	struct i2c_msg t_msg = { .addr = uc_addr, .flags = I2C_M_RD, .buf = puc_buffer, .len = i_byte };

	if(g_p_adapter == NULL)
	{
		printk("[TCC_I2C] i2c adapter is NULL!\n");
		return -1;
	}

	i_retval = i2c_transfer(g_p_adapter, &t_msg, 1);

	//printk("[%s:%d] addr : 0x%x %d %d\n", __func__, __LINE__, uc_addr, i_byte, i_retval);

	return (i_retval == 1) ? i_byte : i_retval;
}

int tcc_i2c_write(unsigned char uc_addr, int i_byte, unsigned char *puc_buffer)
{
	int i_retval;
	
	struct i2c_msg t_msg = { .addr = uc_addr, .flags = 0, .buf = puc_buffer, .len = i_byte };

	if(g_p_adapter == NULL)
	{
		printk("[TCC_I2C] i2c adapter is NULL!\n");
		return -1;
	}

	//printk("[%s:%d] addr : 0x%x %d\n", __func__, __LINE__, uc_addr, i_byte);

	i_retval = i2c_transfer(g_p_adapter, &t_msg, 1);

	return (i_retval == 1) ? i_byte : i_retval;
}

struct i2c_adapter * tcc_i2c_get_adapter()
{
	return g_p_adapter;
}

int tcc_i2c_init(void)
{
	if(g_p_adapter != NULL)
	{
		return 0;
	}

	g_p_adapter = i2c_get_adapter(TCC_I2C_CH);

	if(g_p_adapter == NULL)
	{
		printk("[TCC_I2C] i2c_get_adapter() failed!\n");
		return -1;
	}
	
	return 0;
}

int tcc_i2c_deinit(void)
{
	if(g_p_adapter == NULL)
	{
		printk("[TCC_I2C] i2c adapter is NULL!\n");
		return -1;
	}

	i2c_put_adapter(g_p_adapter);

	g_p_adapter = NULL;
	
	return 0;
}
