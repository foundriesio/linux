#ifndef __TCC_I2C_H__
#define __TCC_I2C_H__

#include <linux/i2c.h>

int tcc_i2c_read(unsigned char uc_addr, int i_byte, unsigned char *puc_buffer);
int tcc_i2c_write(unsigned char uc_addr, int i_byte, unsigned char *puc_buffer);

struct i2c_adapter * tcc_i2c_get_adapter(void);

int tcc_i2c_init(void);
int tcc_i2c_deinit(void);

#endif	// __TCC_I2C_H__
