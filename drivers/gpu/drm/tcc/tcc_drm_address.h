#ifndef __TCC_DRM_ADDRESS_H__
#define __TCC_DRM_ADDRESS_H__


#define RDMA_MAX_NUM 4


enum {
        TCC_DRM_DT_VERSION_OLD = 0,
        TCC_DRM_DT_VERSION_1_0,
};

struct tcc_hw_block{
        volatile void __iomem* virt_addr;
        unsigned int irq_num;
        unsigned int blk_num;
};

struct tcc_hw_device {
        //unsigned int DispNum;
        struct clk *vioc_clock;
        struct clk *ddc_clock;
        struct tcc_hw_block display_device;
        struct tcc_hw_block wmixer;
        struct tcc_hw_block wdma;
        struct tcc_hw_block rdma[RDMA_MAX_NUM];
	int rdma_plane_type[RDMA_MAX_NUM];
};


extern int tcc_drm_address_dt_parse(struct platform_device *pdev, 
                                        struct tcc_hw_device *hw_data);

#endif