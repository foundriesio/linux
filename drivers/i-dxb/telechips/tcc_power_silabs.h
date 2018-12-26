#ifndef __TCC_POWER_SILABS_H__
#define __TCC_POWER_SILABS_H__

int tcc_power_silabs_on(void);
int tcc_power_silabs_off(void);

int tcc_power_silabs_init(silabs_fe_type_e e_type);
int tcc_power_silabs_deinit(void);

#endif	// __TCC_POWER_SILABS_H__