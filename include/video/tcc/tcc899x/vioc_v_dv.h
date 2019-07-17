#ifndef __VIOC_DISP_DV_H__
#define	__VIOC_DISP_DV_H__

#include "vioc_config.h"
#include "vioc_disp.h"
#include "vioc_rdma.h"
#include <video/tcc/tcc_edr_v1.h>
#include <video/tcc/tccfb_ioctrl.h>

#define DV_MAIN_DONE 	0x1
#define DV_SUB_DONE  	0x2
#define DV_DUAL_MODE  	0x4
#define DV_ALL_DONE 	(DV_MAIN_DONE|DV_SUB_DONE|DV_DUAL_MODE)

typedef enum{
	DV_PATH_DIRECT 			= 0x1,
	DV_PATH_VIN_WDMA		= 0x2,
	DV_PATH_DIRECT_VIN_WDMA	= 0x3, // 0x1 | 0x2,  DV_PATH_DIRECT with VIN and WDMA0 (new sequence with DISP0)
	DV_PATH_VIN_DISP		= 0x4,
	DV_PATH_VIN_ALL 		= 0x6, // 0x2 | 0x4,  VIN + DISP0 and WDMA0
	DV_PATH_DUAL 			= 0x7, // 0x1 | 0x6,  DV_PATH_DIRECT with VIN + DISP0 and WDMA0
}DV_PATH;

typedef enum{
	DV_STD = 0,
	DV_LL,
	DV_LL_RGB
}DV_MODE;

typedef enum{
	DV_OFF = 0,
	DV_STANDBY,
	DV_READY,
	DV_RUN,
}DV_STAGE;

typedef enum{
	DOVI = 0,
	HDR10,
	SDR,
	DOVI_LL
}OUT_TYPE;

//0 rgb, 1 ycc444, 2 ycc422, 3 ycc420
typedef enum{
	DV_OUT_FMT_RGB = 0,
	DV_OUT_FMT_YUV444,
	DV_OUT_FMT_YUV422,
	DV_OUT_FMT_YUV420
}OUT_FORMAT;

typedef enum{
	VEDR = 0,
	VPANEL,
	VPANEL_LUT,
	VDV_CFG,
	VEDR_MAX
}VEDR_TYPE;

typedef enum{
	EDR_OSD1 = 0,
	EDR_OSD3,
	EDR_BL,
	EDR_EL,
	EDR_MAX
}DV_DISP_TYPE;

typedef enum{
	ATTR_SDR = 0,
	ATTR_DV_WITHOUT_BC,
	ATTR_DV_WITH_BC,
}VIDEO_ATTR;

extern unsigned int Hactive;
extern unsigned int Vactive;

extern void VIOC_V_DV_SetInterruptEnable(volatile void __iomem *reg, unsigned int nInterrupt, unsigned int en);
extern void VIOC_V_DV_GetInterruptPending(volatile void __iomem *reg, unsigned int *pPending);
extern void VIOC_V_DV_GetInterruptStatus(volatile void __iomem *reg, unsigned int *pStatus);
extern void VIOC_V_DV_ClearInterrupt(volatile void __iomem *reg, unsigned int nInterrupt);
extern int VIOC_V_DV_Is_EdrRDMA(volatile void __iomem *pRDMA);
extern void VIOC_V_DV_SetSize(volatile void __iomem *pDISP, volatile void __iomem *pRDMA, unsigned int sx, unsigned int sy, unsigned int width, unsigned int height);
extern void VIOC_V_DV_SetPosition(volatile void __iomem *pDISP, volatile void __iomem *pRDMA, unsigned int sx,unsigned int sy);
extern void VIOC_V_DV_SetPXDW(volatile void __iomem *pDISP, volatile void __iomem *pRDMA, unsigned int pixel_fmt);
extern void VIOC_V_DV_SetBGColor(volatile void __iomem *pDISP, volatile void __iomem *pRDMA, unsigned int R_y, unsigned int G_u, unsigned int B_v, unsigned int alpha);
extern void VIOC_V_DV_Turnon(volatile void __iomem *pDISP, volatile void __iomem *pRDMA);
extern void VIOC_V_DV_Turnoff(volatile void __iomem *pDISP, volatile void __iomem *pRDMA);
extern void VIOC_V_DV_All_Turnoff(void);
extern void VIOC_V_DV_Power(char on);
extern void VIOC_V_DV_SWReset(unsigned int force, unsigned int bReset);
extern void VIOC_V_DV_Base_Configure(int sx, int sy, int w, int h);
extern volatile void __iomem * VIOC_DNG_GetAddress(void);
extern volatile void __iomem* VIOC_DV_GetAddress(DV_DISP_TYPE type);
extern void VIOC_DV_DUMP(DV_DISP_TYPE type, unsigned int size);
extern volatile void __iomem*  VIOC_DV_VEDR_GetAddress(VEDR_TYPE type);
extern void VIOC_DV_VEDR_DUMP(VEDR_TYPE type, unsigned int size);

// extern tcc_vdv_interface.c
extern void voic_v_dv_set_hdmi_timming(struct lcdc_timimg_parms_t *mode, int bHDMI_Out, unsigned int hdmi_khz);
extern unsigned int vioc_v_dv_get_lcd0_clk_khz(void);
extern void vioc_v_dv_el_bypass(void);
extern char vioc_v_dv_get_sc(void);
extern void vioc_v_dv_swreset(unsigned int edr, unsigned panel, unsigned int crtc);
extern void vioc_v_dv_block_off(void);
extern void voic_v_dv_osd_ctrl(DV_DISP_TYPE type, unsigned int on);
extern int vioc_v_dv_prog(unsigned int meta_PhyAddr, unsigned int reg_PhyAddr, unsigned int video_attribute, unsigned int frmcnt);
extern void vioc_v_dv_set_mode(DV_MODE mode, unsigned char* vsvdb, unsigned int sz_vsvdb);
extern DV_MODE vioc_v_dv_get_mode(void);
extern int vioc_v_dv_is_rgb_tunneling(void);
extern unsigned int vioc_v_dv_get_vsvdb(unsigned char* vsvdb);
extern void vioc_v_dv_set_stage(DV_STAGE stage);
extern DV_STAGE vioc_v_dv_get_stage(void);
extern DV_PATH vioc_get_path_type(void);
extern void vioc_set_out_type(OUT_TYPE type);
extern OUT_TYPE vioc_get_out_type(void);
extern void vioc_v_dv_set_output_color_format(unsigned int pxdw, unsigned int swap);
extern OUT_FORMAT vioc_v_dv_get_output_color_format(void);
extern VIDEO_ATTR vioc_get_video_attribute(void);
extern void vioc_v_dv_reset(void);
extern char vioc_v_dv_check_hdmi_out(void);
#endif

