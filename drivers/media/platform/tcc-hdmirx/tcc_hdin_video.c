
/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
 
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_gpu_align.h>

#include <soc/tcc/pmap.h>

#include "tcc_hdin_main.h"
#include "tcc_hdin_ctrl.h"
#include "tcc_hdin_video.h"

static int debug = 0;

#define LOG_MODULE_NAME "HDMI"

#define logl(level, fmt, ...) printk(level "[%s][%s] %s - " pr_fmt(fmt), #level + 5, LOG_MODULE_NAME, __FUNCTION__, ##__VA_ARGS__)
#define log(fmt, ...) logl(KERN_INFO, fmt, ##__VA_ARGS__)
#define loge(fmt, ...) logl(KERN_ERR, fmt, ##__VA_ARGS__)
#define logw(fmt, ...) logl(KERN_WARNING, fmt, ##__VA_ARGS__)
#define logd(fmt, ...) if (debug) logl(KERN_DEBUG, fmt, ##__VA_ARGS__)

#define HDIN_USE_SCALER

static void hdin_data_init(void *priv)
{
    struct tcc_hdin_device *dev = (struct tcc_hdin_device *)priv;
    struct TCC_HDIN *data = &dev->data;
    struct device_node *hdin_np = dev->np;
    struct device_node *module_np;
    
    //Default Setting
    data->hdin_cfg.pp_num			= 0; 
    data->hdin_cfg.fmt				= M420_EVEN;
    data->hdin_cfg.main_set.target_x = 0;
    data->hdin_cfg.main_set.target_y = 0;
    
    //vin parameter
    module_np = of_find_node_by_name(hdin_np, MODULE_NODE);
    if(module_np) {
        of_property_read_u32_index(module_np,"polarity_hsync", 0, &dev->vioc.polarity_hsync);
        of_property_read_u32_index(module_np,"polarity_vsync", 0, &dev->vioc.polarity_vsync);
        of_property_read_u32_index(module_np,"field_bfield_low", 0, &dev->vioc.field_bfield_low);
        of_property_read_u32_index(module_np,"polarity_data_en", 0, &dev->vioc.polarity_data_en);
        of_property_read_u32_index(module_np,"gen_field_en", 0, &dev->vioc.gen_field_en);
        of_property_read_u32_index(module_np,"polarity_pclk", 0, &dev->vioc.polarity_pclk);
        of_property_read_u32_index(module_np,"conv_en", 0, &dev->vioc.conv_en);
        of_property_read_u32_index(module_np,"hsde_connect_en", 0, &dev->vioc.hsde_connect_en);
        of_property_read_u32_index(module_np,"vs_mask", 0, &dev->vioc.vs_mask);
        of_property_read_u32_index(module_np,"input_fmt", 0, &dev->vioc.input_fmt);
        of_property_read_u32_index(module_np,"data_order", 0, &dev->vioc.data_order);
        of_property_read_u32_index(module_np,"intpl_en", 0, &dev->vioc.intpl_en);
        of_property_read_u32_index(module_np,"wdma_swap_order", 0, &dev->vioc.wdma_swap_order);
    }
}

int hdin_vioc_vin_main(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    uint width, height;
    
    width = dev->input_width;
    height = dev->input_height;
    
    log("VideoIn Resolution : %dx%d\n",width,height);
    
    VIOC_CONFIG_WMIXPath(dev->vioc.wmixer.index, OFF); 		// VIN01 means LUT of VIN0 component
    VIOC_WMIX_SetUpdate(dev->vioc.wmixer.address);
    VIOC_VIN_SetSyncPolarity(dev->vioc.vin.address,
                             dev->vioc.polarity_hsync,
                             dev->vioc.polarity_vsync,
                             dev->vioc.field_bfield_low,
                             dev->vioc.polarity_data_en,
                             dev->vioc.gen_field_en,
                             dev->vioc.polarity_pclk);
    VIOC_VIN_SetCtrl(dev->vioc.vin.address,
                     dev->vioc.conv_en,
                     dev->vioc.hsde_connect_en,
                     dev->vioc.vs_mask,
                     dev->vioc.input_fmt,
                     dev->vioc.data_order);
    VIOC_VIN_SetInterlaceMode(dev->vioc.vin.address,
                              dev->hdin_interlaced_mode,
                              dev->vioc.intpl_en);
    VIOC_VIN_SetImageSize(dev->vioc.vin.address, width, height);
    VIOC_VIN_SetImageOffset(dev->vioc.vin.address, 0, 0, 0);
    VIOC_VIN_SetImageCropSize(dev->vioc.vin.address, width, height);
    VIOC_VIN_SetImageCropOffset(dev->vioc.vin.address, 0, 0);
    VIOC_VIN_SetY2RMode(dev->vioc.vin.address, 2);
    VIOC_VIN_SetY2REnable(dev->vioc.vin.address, OFF);
    VIOC_VIN_SetEnable(dev->vioc.vin.address, ON);
    return 0;
}

#ifdef HDIN_USE_SCALER
int hdin_vioc_scaler_set(struct tcc_hdin_device *vdev)
{
    unsigned int dw, dh; // destination size in SCALER
    struct tcc_hdin_device *dev = 	vdev;
    struct TCC_HDIN *data = &dev->data;
    VIOC_PlugInOutCheck plug_in_status;
    
    dw = data->hdin_cfg.main_set.target_x;
    dh = data->hdin_cfg.main_set.target_y;

    VIOC_CONFIG_Device_PlugState(dev->vioc.scaler.index, &plug_in_status);
    if(plug_in_status.enable && plug_in_status.connect_statue == VIOC_PATH_CONNECTED) {
        VIOC_CONFIG_PlugOut(dev->vioc.scaler.index);
    }

    VIOC_CONFIG_PlugIn(dev->vioc.scaler.index, dev->vioc.vin.index);	// PlugIn position in SCALER
    VIOC_SC_SetBypass(dev->vioc.scaler.address, OFF);
    if(dev->hdin_interlaced_mode) {
        VIOC_SC_SetDstSize(dev->vioc.scaler.address, dw, (dh+6));  // set destination size in scaler
        VIOC_SC_SetOutPosition(dev->vioc.scaler.address, 0, 3);    // set output position in scaler
    } else {
        VIOC_SC_SetDstSize(dev->vioc.scaler.address, dw + 12, dh + 2);  // set destination size in scaler
        VIOC_SC_SetOutPosition(dev->vioc.scaler.address, 6, 1);
    }
    VIOC_SC_SetOutSize(dev->vioc.scaler.address, dw, dh);		// set output size in scaer
    VIOC_SC_SetUpdate(dev->vioc.scaler.address);					// Scaler update

    logd("VIOC SC%d(%dx%d)\n", get_vioc_index(dev->vioc.scaler.index), dw, dh);
    return 0;
}
#endif

static void hdin_set_buf_addr_in_wdma(struct tcc_hdin_device *vdev,unsigned int frame_num)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    VIOC_WDMA_SetImageBase(dev->vioc.wdma.address,
                           (unsigned int)data->hdin_cfg.preview_buf[frame_num].p_Y,
                           (unsigned int)data->hdin_cfg.preview_buf[frame_num].p_Cb,
                           (unsigned int)data->hdin_cfg.preview_buf[frame_num].p_Cb);
}

int hdin_vioc_vin_wdma_set(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    
    uint dw, dh; 	// destination image size
    uint fmt; 		// image format
    
    dw = data->hdin_cfg.main_set.target_x;
    dh = data->hdin_cfg.main_set.target_y;
    fmt = VIOC_IMG_FMT_YUV420IL1;
    logd("WDMA size[%dx%d], format[%d]. \n", dw, dh, fmt);
    VIOC_WDMA_SetImageFormat(dev->vioc.wdma.address, fmt);
    VIOC_WDMA_SetImageSize(dev->vioc.wdma.address, dw, dh);
    VIOC_WDMA_SetImageOffset(dev->vioc.wdma.address, fmt, dw);
    VIOC_WDMA_SetImageEnable(dev->vioc.wdma.address, ON);	// operating start in 1 frame
    VIOC_WDMA_ClearEOFF(dev->vioc.wdma.address); // clear EOFF bit
    VIOC_WDMA_SetIreqMask(dev->vioc.wdma.address, VIOC_WDMA_IREQ_EOFF_MASK, 0x0);	
    return 0;
}

int hdin_vioc_viqe_main(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    unsigned int	viqe_width	= 0;
    unsigned int	viqe_height	= 0;
    unsigned int	format		= VIOC_VIQE_FMT_YUV422;
    unsigned int	bypass_deintl	= VIOC_VIQE_DEINTL_MODE_3D;
    unsigned int	offset			= dev->input_width*dev->input_height* 2 * 2;
    unsigned int	deintl_base0	= dev->viqe_area;
    unsigned int	deintl_base1	= deintl_base0 + offset;
    unsigned int	deintl_base2	= deintl_base1 + offset;
    unsigned int	deintl_base3	= deintl_base2 + offset;
    unsigned int	cdf_lut_en	= OFF;
    unsigned int	his_en		= OFF;
    unsigned int	gamut_en	= OFF;
    unsigned int	d3d_en		= OFF;
    unsigned int	deintl_en	= ON;

    VIOC_VIQE_IgnoreDecError(dev->vioc.viqe.address,
                             ON,
                             ON,
                             ON);
                             
    VIOC_VIQE_SetControlRegister(dev->vioc.viqe.address,
                                 viqe_width,
                                 viqe_height,
                                 format);
    VIOC_VIQE_SetDeintlRegister(dev->vioc.viqe.address,
                                format,
                                OFF,
                                viqe_width,
                                viqe_height,
                                bypass_deintl,
                                deintl_base0,
                                deintl_base1,
                                deintl_base2,
                                deintl_base3);
    VIOC_VIQE_SetControlEnable(dev->vioc.viqe.address,
                               cdf_lut_en,
                               his_en,
                               gamut_en,
                               d3d_en,
                               deintl_en);
    return 0;
}

void hdin_set_cam_dly(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct device_node	*hdin_np = dev->np;
    struct device_node *ddi_np;
    volatile void __iomem *cam_dly_base_addr;
    unsigned int clk_delay_value = 0;
    unsigned int de_delay_value = 0;
    unsigned int field_delay_value = 0;
    unsigned int hsync_delay_value = 0;
    unsigned int vsync_delay_value = 0;
    unsigned int data_delay_value[30] = {0,};
    int index = 0, port = 0, width_condition = 0, height_condition = 0;
    char node_name[32];
    unsigned long val = 0;

    ddi_np = of_parse_phandle(hdin_np,"hdmi_rx_ddi_config", 0);
    cam_dly_base_addr = of_iomap(ddi_np, 0);

    of_property_read_u32_index(hdin_np,"hdmi_rx_port",1,&port);
    of_property_read_u32_index(hdin_np,"hdmi_rx_clk_dly",0,&clk_delay_value);
    of_property_read_u32_index(hdin_np,"hdmi_rx_de_dly",0,&de_delay_value);
    of_property_read_u32_index(hdin_np,"hdmi_rx_field_dly",0,&field_delay_value);
    of_property_read_u32_index(hdin_np,"hdmi_rx_hsync_dly",0,&hsync_delay_value);
    of_property_read_u32_index(hdin_np,"hdmi_rx_vsync_dly",0,&vsync_delay_value);

    for(index = 0;index < 30;index++) {
        sprintf(node_name,"hdmi_rx_data%02d_dly",index);
        of_property_read_u32_index(hdin_np,node_name,0,&data_delay_value[index]);
    }
    of_property_read_u32_index(hdin_np,"hdmi_rx_dly_width",0,&width_condition);
    of_property_read_u32_index(hdin_np,"hdmi_rx_dly_height",0,&height_condition);

    if(dev->input_width >= width_condition && dev->input_height >= height_condition) {
        //Clock delay
        val = __raw_readl(cam_dly_base_addr + 0x280 + (0x20 * port));
        val |= clk_delay_value;
        __raw_writel(val, cam_dly_base_addr + 0x280 + (0x20 * port));
        //Sync delay
        val = __raw_readl(cam_dly_base_addr + 0x284 + (0x20 * port));
        val |= ((de_delay_value << 12) |
                (field_delay_value << 8) |
                (hsync_delay_value << 4) |
                 vsync_delay_value) ;
        __raw_writel(val, cam_dly_base_addr + 0x284 + (0x20 * port)); 
        //Data delay
        val = __raw_readl(cam_dly_base_addr + 0x288 + (0x20 * port));
        val |= ((data_delay_value[7] << 28) |
                (data_delay_value[6] << 24) |
                (data_delay_value[5] << 20) |
                (data_delay_value[4] << 16) |
                (data_delay_value[3] << 12) |
                (data_delay_value[2] << 8) |
                (data_delay_value[1] << 4) |
                (data_delay_value[0])) ;
        __raw_writel(val, cam_dly_base_addr + 0x288 + (0x20 * port)); 
    
        val = __raw_readl(cam_dly_base_addr + 0x28C + (0x20 * port));
        val |= ((data_delay_value[15] << 28) |
                (data_delay_value[14] << 24) |
                (data_delay_value[13] << 20) |
                (data_delay_value[12] << 16) |
                (data_delay_value[11] << 12) |
                (data_delay_value[10] << 8) |
                (data_delay_value[9] << 4) |
                (data_delay_value[8])) ;
        __raw_writel(val, cam_dly_base_addr + 0x28C + (0x20 * port));
        
        val = __raw_readl(cam_dly_base_addr + 0x290 + (0x20 * port));
        val |= ((data_delay_value[23] << 28) |
                (data_delay_value[22] << 24) |
                (data_delay_value[21] << 20) |
                (data_delay_value[20] << 16) |
                (data_delay_value[19] << 12) |
                (data_delay_value[18] << 8) |
                (data_delay_value[17] << 4) |
                (data_delay_value[16])) ;
        __raw_writel(val, cam_dly_base_addr + 0x290 + (0x20 * port));
    
        val = __raw_readl(cam_dly_base_addr + 0x294 + (0x20 * port));
        val |= ((data_delay_value[29] << 20) |
                (data_delay_value[28] << 16) |
                (data_delay_value[27] << 12) |
                (data_delay_value[26] << 8) |
                (data_delay_value[25] << 4) |
                (data_delay_value[24])) ;
        __raw_writel(val, cam_dly_base_addr + 0x294 + (0x20 * port));  
    } else {
        for(index = 0; index < 6; index++) {
            __raw_writel(0x0, cam_dly_base_addr + 0x280 + (0x4 * index) + (0x20 * port));  
        }
    }
}

void hdin_set_port(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct device_node	*hdin_np = dev->np;
    struct device_node *port_np;
    unsigned int nUsingPort,nCIF_8bit_connect;
    volatile void __iomem *cifport_addr;
    unsigned long val;
    
    port_np = of_parse_phandle(hdin_np,"hdmi_rx_port", 0);
    of_property_read_u32_index(hdin_np,"hdmi_rx_port",1,&nUsingPort);
    of_property_read_u32_index(hdin_np,"hdmi_rx_port",2,&nCIF_8bit_connect);
    cifport_addr = of_iomap(port_np, 0);
    val = __raw_readl(cifport_addr);
    val |= (nUsingPort << (get_vioc_index(dev->vioc.vin.index) * 4)) | 
           (nCIF_8bit_connect << (get_vioc_index(dev->vioc.vin.index) + 20));
    __raw_writel(val, cifport_addr);
    
}

int hdin_video_start_stream(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    VIOC_PlugInOutCheck plug_in_status;
	
    logd("Start!! \n");
    VIOC_CONFIG_SWReset(dev->vioc.wdma.index,VIOC_CONFIG_RESET);
    VIOC_CONFIG_SWReset(dev->vioc.wmixer.index,VIOC_CONFIG_RESET);
#ifdef HDIN_USE_SCALER
    VIOC_CONFIG_SWReset(dev->vioc.scaler.index,VIOC_CONFIG_RESET);	
#endif
    VIOC_CONFIG_SWReset(dev->vioc.vin.index,VIOC_CONFIG_RESET);
    if(dev->hdin_interlaced_mode) {
        VIOC_CONFIG_SWReset(dev->vioc.viqe.index,VIOC_CONFIG_RESET);
        VIOC_CONFIG_SWReset(dev->vioc.viqe.index,VIOC_CONFIG_CLEAR);
    }
    VIOC_CONFIG_SWReset(dev->vioc.vin.index,VIOC_CONFIG_CLEAR);
#ifdef HDIN_USE_SCALER
    VIOC_CONFIG_SWReset(dev->vioc.scaler.index,VIOC_CONFIG_CLEAR);
#endif
    VIOC_CONFIG_SWReset(dev->vioc.wmixer.index,VIOC_CONFIG_CLEAR);
    VIOC_CONFIG_SWReset(dev->vioc.wdma.index,VIOC_CONFIG_CLEAR);
    
    data->stream_state = STREAM_ON;
    data->prev_buf = NULL;
    hdin_set_cam_dly(dev);
    hdin_vioc_vin_main(dev);
#ifdef HDIN_USE_SCALER
    hdin_vioc_scaler_set(dev);
#endif
    VIOC_CONFIG_Device_PlugState(dev->vioc.viqe.index, &plug_in_status);
    if(plug_in_status.enable && plug_in_status.connect_statue == VIOC_PATH_CONNECTED) {
        VIOC_CONFIG_PlugOut(dev->vioc.viqe.index);
        hdin_ctrl_delay(2);
    }
    if(dev->hdin_interlaced_mode) {
        VIOC_CONFIG_PlugIn(dev->vioc.viqe.index, dev->vioc.vin.index);
        hdin_vioc_viqe_main(dev);
    }
    hdin_vioc_vin_wdma_set(dev);
    logd("End!! \n");
    return 0;
}

int hdin_video_stop_stream(struct tcc_hdin_device *vdev)
{	
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    VIOC_PlugInOutCheck plug_in_status;
    
    logd("Start!! \n");
    mutex_lock(&data->lock);
    VIOC_WDMA_SetIreqMask(dev->vioc.wdma.address, VIOC_WDMA_IREQ_ALL_MASK, 0x1); // disable WDMA interrupt
    VIOC_WDMA_SetImageDisable(dev->vioc.wdma.address);
    VIOC_VIN_SetEnable(dev->vioc.vin.address, OFF); // disable VIN
    VIOC_CONFIG_Device_PlugState(dev->vioc.viqe.index, &plug_in_status);
    if(plug_in_status.enable && plug_in_status.connect_statue == VIOC_PATH_CONNECTED) {
        VIOC_CONFIG_PlugOut(dev->vioc.viqe.index);
        hdin_ctrl_delay(2);
    }
#ifdef HDIN_USE_SCALER
    VIOC_CONFIG_Device_PlugState(dev->vioc.scaler.index, &plug_in_status);
    if(plug_in_status.enable && plug_in_status.connect_statue == VIOC_PATH_CONNECTED) {
        VIOC_CONFIG_PlugOut(dev->vioc.scaler.index);
    }
#endif
    mutex_unlock(&data->lock);
    data->stream_state = STREAM_OFF;	
    logd("End!! \n");
    return 0;
}


int hdin_video_buffer_set(struct tcc_hdin_device *vdev,struct v4l2_requestbuffers *req)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    struct tcc_hdin_buffer *buf = NULL;
    unsigned int y_offset = 0, uv_offset = 0, stride = 0;
    unsigned int buff_size = 0;
    unsigned long flags;
    
    if(req->count == 0) {
        memset(data->static_buf, 0x00, TCC_HDIN_MAX_BUFNBRS * sizeof(struct tcc_hdin_buffer));
        spin_lock_irqsave(&data->spin_lock,flags);
        data->active_empty_buf_q.prev = data->active_empty_buf_q.next = &data->active_empty_buf_q;
        data->wait_capture_buf_q.prev = data->wait_capture_buf_q.next = &data->wait_capture_buf_q;
        data->capture_done_buf_q.prev = data->capture_done_buf_q.next = &data->capture_done_buf_q;
        spin_unlock_irqrestore(&data->spin_lock, flags);
    }
    stride = ALIGNED_BUFF(data->hdin_cfg.main_set.target_x, 128/*L_STRIDE_ALIGN*/);
    y_offset = ALIGNED_BUFF(stride * data->hdin_cfg.main_set.target_y, 64);
    uv_offset = ALIGNED_BUFF(ALIGNED_BUFF((stride/2), 16) * (data->hdin_cfg.main_set.target_y/2),64);
   
    if(data->hdin_cfg.fmt == M420_ZERO) {
        buff_size = PAGE_ALIGN(y_offset*2);
    } else {
        buff_size = PAGE_ALIGN(y_offset + y_offset/2);
    }
    buf = &(data->static_buf[req->count]);
    buf->v4lbuf.index = req->count;
    buf->v4lbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf->v4lbuf.field = V4L2_FIELD_NONE;
    buf->v4lbuf.memory = V4L2_MEMORY_MMAP;
    buf->v4lbuf.length = buff_size;
    
    data->hdin_cfg.preview_buf[req->count].p_Y  = (unsigned int)req->reserved[0];
    data->hdin_cfg.preview_buf[req->count].p_Cb = (unsigned int)data->hdin_cfg.preview_buf[req->count].p_Y + y_offset;
    data->hdin_cfg.preview_buf[req->count].p_Cr = (unsigned int)data->hdin_cfg.preview_buf[req->count].p_Cb + uv_offset;
    data->hdin_cfg.pp_num = req->count;
    return 0;
}

void hdin_video_set_addr(struct tcc_hdin_device *vdev,int index, unsigned int addr)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    unsigned int y_offset = 0, uv_offset = 0, stride = 0;
    
    stride = ALIGNED_BUFF(data->hdin_cfg.main_set.target_x, 128/*L_STRIDE_ALIGN*/);
    y_offset = ALIGNED_BUFF(stride * data->hdin_cfg.main_set.target_y, 64);
    uv_offset = ALIGNED_BUFF(ALIGNED_BUFF((stride/2), 16) * (data->hdin_cfg.main_set.target_y/2),64);

    data->hdin_cfg.preview_buf[index].p_Y  = addr;
    data->hdin_cfg.preview_buf[index].p_Cb = (unsigned int)data->hdin_cfg.preview_buf[index].p_Y + y_offset;
    data->hdin_cfg.preview_buf[index].p_Cr = (unsigned int)data->hdin_cfg.preview_buf[index].p_Cb + uv_offset;
}

int hdin_video_set_resolution(struct tcc_hdin_device *vdev,unsigned int pixel_fmt, unsigned short width, unsigned short height)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    
    if(pixel_fmt == V4L2_PIX_FMT_YUYV) {
        data->hdin_cfg.fmt = M420_ZERO;  // yuv422
    } else if(pixel_fmt == V4L2_PIX_FMT_RGB32) {
        data->hdin_cfg.fmt = ARGB8888;
    } else if(pixel_fmt == V4L2_PIX_FMT_RGB565){
        data->hdin_cfg.fmt = RGB565;
    } else {
        data->hdin_cfg.fmt = M420_ODD; 	// yuv420
    }
    data->hdin_cfg.main_set.target_x = width;
    data->hdin_cfg.main_set.target_y = height;
    return 0;
}

static irqreturn_t hdin_video_stream_isr(int irq, void *client_data)
{
    struct tcc_hdin_device *dev = (struct tcc_hdin_device*)client_data;
    struct TCC_HDIN *data = &dev->data;
    struct tcc_hdin_buffer *next_buf;
    unsigned long flags;
    unsigned int status;
    
    if (is_vioc_intr_activatied(data->vioc_intr->id, data->vioc_intr->bits) == false) {
        return IRQ_NONE;
    }
    
    VIOC_WDMA_GetStatus(dev->vioc.wdma.address,&status);
    if(status & VIOC_WDMA_IREQ_EOFF_MASK) {
        // If the buffer has an image was captured from video-in,
        // The buffer is moved to the end of the capture completed list(capture_done_buf_q).
        if(data->prev_buf != NULL) {
            list_move_tail(&data->prev_buf->buf_list, &data->capture_done_buf_q);
        }
        if(!list_empty(&data->wait_capture_buf_q)) {
            spin_lock_irqsave(&data->spin_lock,flags);
            data->prev_buf = list_first_entry(&data->wait_capture_buf_q, struct tcc_hdin_buffer, buf_list);
            data->prev_buf->v4lbuf.flags &= ~V4L2_BUF_FLAG_QUEUED;
            data->prev_buf->v4lbuf.flags |= V4L2_BUF_FLAG_DONE;
            spin_unlock_irqrestore(&data->spin_lock, flags);
        }
        if(!list_empty(&data->active_empty_buf_q)) {
            spin_lock_irqsave(&data->spin_lock,flags);
            // Gets the next buffer from the next list of prepared(queued) buffers.
            // and using to capture.
            next_buf = list_first_entry(&data->active_empty_buf_q, struct tcc_hdin_buffer, buf_list);
            list_move_tail(&next_buf->buf_list, &data->wait_capture_buf_q);
            spin_unlock_irqrestore(&data->spin_lock, flags);
            hdin_set_buf_addr_in_wdma(dev,(unsigned int)next_buf->v4lbuf.index);
            VIOC_WDMA_SetImageUpdate(dev->vioc.wdma.address);
        } else {
            data->prev_buf = NULL;
        }
        data->wakeup_int = 1;
        wake_up_interruptible(&data->frame_wait);
        VIOC_WDMA_ClearEOFF(dev->vioc.wdma.address); 
    }
    return IRQ_HANDLED;
}

void hdin_vioc_dt_parse_data(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct device_node	*hdin_np = dev->np;
    struct device_node *vioc_np;
    
    if(hdin_np)	{
        if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_rx_wmixer", 0))) {
            log("could not find hdin wmixer node!! \n");
            dev->vioc.wmixer.address = NULL;
        } else {
            of_property_read_u32_index(hdin_np,"hdmi_rx_wmixer", 1, &dev->vioc.wmixer.index);
            dev->vioc.wmixer.address = VIOC_WMIX_GetAddress(dev->vioc.wmixer.index);
        }
        if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_rx_wdma", 0))) {
            log("could not find hdin wdma node!! \n");
            dev->vioc.wdma.address = NULL;
        } else {
            of_property_read_u32_index(hdin_np,"hdmi_rx_wdma", 1, &dev->vioc.wdma.index);
            dev->vioc.wdma.address	= VIOC_WDMA_GetAddress(dev->vioc.wdma.index);
        }
#ifdef HDIN_USE_SCALER
        if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_rx_scaler", 0))) {
            log("could not find hdin scaler node!! \n");
        } else {
            of_property_read_u32_index(hdin_np,"hdmi_rx_scaler", 1, &dev->vioc.scaler.index);
            dev->vioc.scaler.address	= VIOC_SC_GetAddress(dev->vioc.scaler.index);
        }
#endif
        if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_rx_videoin", 0))) {
            log("could not find hdin input node!! \n");
        } else {
            of_property_read_u32_index(hdin_np,"hdmi_rx_videoin", 1, &dev->vioc.vin.index);
            dev->vioc.vin.address = VIOC_VIN_GetAddress(dev->vioc.vin.index);
        }
#if 0
        if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_rx_lut", 0))) {
            log("could not find hdin input node!! \n");
        } else {
            of_property_read_u32_index(hdin_np,"hdmi_rx_lut", 1, &dev->vioc.lut.index);
            dev->vioc.lut.address = of_iomap(vioc_np, dev->vioc.lut.index);
        }

		if(!(vioc_np = of_parse_phandle(hdin_np, "hdin_config", 0))) 
		{
			log("could not find hdin irqe node!! \n");
		}
		else
		{
			of_property_read_u32_index(hdin_np,"hdin_config", 1, &dev->vioc.config.index);
			dev->vioc.config.address= (unsigned int*)of_iomap(vioc_np, dev->vioc.config.index);			
		}
#endif
        if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_rx_viqe", 0))) {
            log("could not find hdin viqe node!! \n");
        } else {
            of_property_read_u32_index(hdin_np,"hdmi_rx_viqe", 1, &dev->vioc.viqe.index);
            dev->vioc.viqe.address = VIOC_VIQE_GetAddress(dev->vioc.viqe.index);
        }

#if 0
		//for TCC892X ~ 7X, HDMI IP has changed to other Vendor as Synopsys after TCC898X
		if(!(vioc_np = of_parse_phandle(hdin_np, "hdmi_spdif_op", 0)))
		{
			//log("could not find hdmi_spdif_op node!! \n");
			dev->hdmi_spdif_op_addr = 0;
		}
		else
		{
			of_property_read_u32_index(hdin_np,"hdmi_spdif_op", 1, &dev->hdmi_spdif_op_value);
			dev->hdmi_spdif_op_addr = (unsigned int*)of_iomap(vioc_np, 0);			
		}
#endif
    }else {
        log("could not find hdin platform node!! \n");
    }
}

int hdin_video_irq_request(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;
    struct device_node *hdin_np = dev->np;
    struct device_node *irq_np;
    int ret = -1, irq_num;

    irq_np = of_parse_phandle(hdin_np, "hdmi_rx_wdma", 0);
    if(irq_np) {
        of_property_read_u32_index(hdin_np,"hdmi_rx_wdma", 1, &dev->vioc.wdma.index);
        irq_num = irq_of_parse_and_map(irq_np, get_vioc_index(dev->vioc.wdma.index));
        synchronize_irq(irq_num);
        data->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
        if (data->vioc_intr  == 0) {
            log("could not get hdin_data.vioc_intr !! \n");
            return ret;
        }
        data->vioc_intr->id = VIOC_INTR_WD0 + get_vioc_index(dev->vioc.wdma.index);
        data->vioc_intr->bits = VIOC_WDMA_IREQ_EOFF_MASK;/*1<<(VIOC_WDMA_INTR_EOFF|VIOC_WDMA_INTR_EOFR);*/
        vioc_intr_clear(data->vioc_intr->id,data->vioc_intr->bits);
        ret = request_irq(irq_num, hdin_video_stream_isr, IRQF_SHARED, DRIVER_NAME,dev);
        vioc_intr_enable(irq_num,data->vioc_intr->id,data->vioc_intr->bits);
        if(ret) {
            log("failed to aquire hdin(wdma %d) request_irq. \n",data->vioc_intr->id);
        }
    }
    return ret;
}

void hdin_video_irq_free(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;	
    struct device_node *hdin_np = dev->np;
    struct device_node *irq_np;
    int irq_num;
    
    irq_np = of_parse_phandle(hdin_np, "hdmi_rx_wdma", 0);
	if(irq_np) {
        of_property_read_u32_index(hdin_np,"hdmi_rx_wdma", 1, &dev->vioc.wdma.index);
        irq_num = irq_of_parse_and_map(irq_np, get_vioc_index(dev->vioc.wdma.index));
        vioc_intr_clear(data->vioc_intr->id, data->vioc_intr->bits);
        vioc_intr_disable(irq_num, data->vioc_intr->id, data->vioc_intr->bits);
        free_irq(irq_num, dev);        
        kfree(data->vioc_intr);
    }
}

int hdin_video_open(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
	pmap_t viqe_area;
    int ret = 0;

	if(0 > pmap_get_info("video", &viqe_area)){
		loge("viqe_area allocation is failed.\n");
		return -ENOMEM;
	}

    dev->viqe_area = viqe_area.base;

    if(!dev->hdin_irq) {
        if((ret = hdin_video_irq_request(dev)) < 0) {
            loge("FAILED to aquire hdin irq(%d).\n", ret);
            return ret;
        }
        dev->hdin_irq = 1;
    }
    return 0;
}

int hdin_video_close(struct file *file)
{
    struct tcc_hdin_device *dev = video_drvdata(file);
    struct TCC_HDIN *data = &dev->data;
    
    mutex_destroy(&data->lock);
    hdin_ctrl_cleanup(file);
    if(dev->hdin_opend== ON) {
        dev->hdin_opend = OFF;
    }
    if(dev->hdin_irq) {
        hdin_video_irq_free(dev);
    }
    dev->hdin_irq = 0;
	pmap_release_info("video");
    return 0;
}

int  hdin_video_init(struct tcc_hdin_device *vdev)
{
    struct tcc_hdin_device *dev = vdev;
    struct TCC_HDIN *data = &dev->data;

    logd("hdin_video_init!! \n");
    if(dev->hdin_opend == OFF) {
        memset(data,0x00,sizeof(struct TCC_HDIN));
        dev->viqe_area = 0x00;
        dev->hdin_opend = ON;
        dev->current_resolution = -1;
        dev->interrupt_status = 0;
        if(dev->np) {
            hdin_vioc_dt_parse_data(dev);
            hdin_set_port(dev);
        } else {
            log("could not find hdin node!! \n");
            return -ENODEV;	
        }
        hdin_data_init((void*)dev);
        mdelay(5);
        init_waitqueue_head(&data->frame_wait);
        INIT_LIST_HEAD(&data->active_empty_buf_q);
        INIT_LIST_HEAD(&data->wait_capture_buf_q);
        INIT_LIST_HEAD(&data->capture_done_buf_q);
        mutex_init(&data->lock);
        spin_lock_init(&data->spin_lock);
    }
    return 0;
}


