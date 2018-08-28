
#ifndef __TCC_HDMI_V_2_0_CEC_ACCESS_H__
#define __TCC_HDMI_V_2_0_CEC_ACCESS_H__

#ifndef BIT
#define BIT(n)          (1 << n)
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern int cec_dev_write(struct cec_device *cec_dev, uint32_t offset, uint32_t data);
extern int cec_dev_read(struct cec_device *cec_dev, uint32_t offset);
extern void cec_dev_write_mask(struct cec_device *cec_dev, u32 addr, u8 mask, u8 data);
extern u32 cec_dev_read_mask(struct cec_device *cec_dev, u32 addr, u8 mask);

extern int cec_dev_pmu_write(struct cec_device *cec_dev, uint32_t offset, uint32_t data);
extern int cec_dev_pmu_read(struct cec_device *cec_dev, uint32_t offset);
extern void cec_dev_pmu_write_mask(struct cec_device *cec_dev, u32 addr, u8 mask, u8 data);
extern u32 cec_dev_pmu_read_mask(struct cec_device *cec_dev, u32 addr, u8 mask);
#endif				/* __HDMI_ACCESS_H__ */

