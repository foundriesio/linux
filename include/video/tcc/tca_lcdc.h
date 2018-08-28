#ifndef __TCA_LCDC_H__
#define __TCA_LCDC_H__

#include "tccfb_ioctrl.h"
#include "tcc_fb.h"

extern unsigned int DEV_LCDC_Wait_signal(char lcdc);
extern unsigned int DEV_LCDC_Wait_signal_Ext(void);
extern void tcc_lcdc_dithering_setting(struct tcc_dp_device *pdata);
extern void lcdc_initialize(struct lcd_panel *lcd_spec, struct tcc_dp_device *pdata);

#define ID_INVERT	0x01 	// Invered Data Enable(ACBIS pin)  anctive Low
#define IV_INVERT	0x02	// Invered Vertical sync  anctive Low
#define IH_INVERT	0x04	// Invered Horizontal sync	 anctive Low
#define IP_INVERT	0x08	// Invered Pixel Clock : anctive Low

#endif //__TCA_LCDC_H__

