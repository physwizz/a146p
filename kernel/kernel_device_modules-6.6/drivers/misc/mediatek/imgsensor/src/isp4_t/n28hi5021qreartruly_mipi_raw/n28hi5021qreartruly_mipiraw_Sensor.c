/*
  * Copyright (C) 2018 MediaTek Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
  */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "cam_cal_define.h"
#include <linux/slab.h>
#include "n28hi5021qreartruly_mipiraw_Sensor.h"
#include "n28hi5021qreartruly_mipiraw_Sensor_setting.h"
#include "n28hi5021qreartruly_mipiraw_otp.h"

#define PFX "truly_hi5021q_camera_sensor"
#define LOG_INF(format, args...)    \
     pr_info(PFX "[%s] " format, __func__, ##args)

//PDAF
#define ENABLE_PDAF 1
#define per_frame 1

#define I2C_BUFFER_LEN 1020
#define HYNIX_SIZEOF_I2C_BUF 254

static DEFINE_SPINLOCK(imgsensor_drv_lock);
// +bug727089, liangyiyi.wt, MODIFY, 2022/5/16, modify the cap mipi clk 2148Mbps for solving the problem of RF interference
static struct imgsensor_info_struct imgsensor_info = {
     .sensor_id = N28HI5021QREARTRULY_SENSOR_ID,

     .checksum_value = 0x4f1b1d5e,       //0x6d01485c // Auto Test Mode
     .pre = {
         .pclk = 124000000,                //record different mode's pclk
         .linelength =  1022,             //record different mode'slinelength
         .framelength = 4043,             //record different mode'sframelength
         .startx = 0,                    //record different mode'sstartx of grabwindow
         .starty = 0,                    //record different mode'sstarty of grabwindow
         .grabwindow_width = 2048,         //record different mode'swidth of grabwindow
         .grabwindow_height = 1536,        //record different mode'sheight of grabwindow
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 300,
         .mipi_pixel_rate =600000000,//1500Mbps*4/10=600 000 000
     },
     .cap = {
         .pclk = 124000000,
         .linelength =  859,
         .framelength = 4810,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 4096,
         .grabwindow_height = 3072,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 300,
         .mipi_pixel_rate =600000000,//2148Mbps*4/10=859 200 000
     },
     // need to setting
     .cap1 = {
         .pclk = 124000000,
         .linelength =  859,
         .framelength = 4810,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 4096,
         .grabwindow_height = 3072,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 300,
         .mipi_pixel_rate =600000000,//2148Mbps*4/10=859 200 000
     },
     .normal_video = { //4096X2304@30fps
         .pclk = 124000000,
         .linelength =  859,
         .framelength = 4810,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 4096,
         .grabwindow_height = 2304,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 300,
         .mipi_pixel_rate =600000000,//1500Mbpsx4/10=600 000 000
     },
     .hs_video = {//1280X720@120fps
         .pclk = 124000000,
         .linelength =  1002,
         .framelength = 1031,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 2000,
         .grabwindow_height = 1132,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 1201,
         .mipi_pixel_rate = 600000000,//1500Mbps*4/10=600 000 000
     },
     .slim_video = {//1280X720@120fps
         .pclk = 124000000,
         .linelength =  848,
         .framelength = 1218,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 1280,
         .grabwindow_height = 720,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 1201,
         .mipi_pixel_rate = 600000000,//1500Mbps*4/10=600 000 000
     },
     .custom1 = {//binning 12.5
         .pclk = 124000000,
         .linelength =  938,
         .framelength = 5505,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 4096,
         .grabwindow_height = 3072,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 240,
         .mipi_pixel_rate =600000000,//1500mBPS*4/10=600 000 000
     },
     .custom2 = {//Q2B_full
         .pclk = 124000000,
         .linelength =  891,
         .framelength = 13903,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 8192,
         .grabwindow_height = 6144,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 100,
         .mipi_pixel_rate = 600000000,//1500Mbps*4/10=6000000000/10=600000000
     },
      .custom3 = {//Q2B_full_crop_to 12.5
         .pclk = 124000000,
         .linelength =  938,
         .framelength = 5505,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 4096,
         .grabwindow_height = 3072,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 240,
         .mipi_pixel_rate = 600000000,//1500Mbps*4/10=600 000 000
     },
      .custom4 = {//FHD（1920x1080） 60fps
         .pclk = 124000000,
         .linelength =  969,
         .framelength = 2132,
         .startx = 0,
         .starty = 0,
         .grabwindow_width = 1920,
         .grabwindow_height = 1080,
         /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCountby different scenario    */
         .mipi_data_lp2hs_settle_dc = 85,
         /*     following for GetDefaultFramerateByScenario()    */
         .max_framerate = 600,
         .mipi_pixel_rate = 600000000,//1500Mbps*4/10=600 000 000
     },

     .margin = 16,
     .min_shutter = 16,

     .min_gain = 64,
     .max_gain = 2048,
     .min_gain_iso = 100,
     .exp_step = 1,
     .gain_step = 8,//64/16=4
     .gain_type = 3,
     .max_frame_length = 0xFFFFFFFF,
#if per_frame
     .ae_shut_delay_frame = 0,
     .ae_sensor_gain_delay_frame = 0,
     .ae_ispGain_delay_frame = 2,
#else
     .ae_shut_delay_frame = 0,
     .ae_sensor_gain_delay_frame = 1,
     .ae_ispGain_delay_frame = 2,
#endif
     .ihdr_support = 0,      //1, support; 0,not support
     .ihdr_le_firstline = 0,  //1,le first ; 0, se first
     .sensor_mode_num = 9,      //support sensor mode num

     .cap_delay_frame = 2,
     .pre_delay_frame = 2,
     .video_delay_frame = 2,
     .frame_time_delay_frame = 1,//bug 720367 qinduilin.wt, add, 2022/03/03, modify for dualcam sync
     .hs_video_delay_frame = 3,
     .slim_video_delay_frame = 3,
     .custom1_delay_frame = 2,
     .custom2_delay_frame = 2,
     .custom3_delay_frame = 2,
     .isp_driving_current = ISP_DRIVING_4MA,// bug727089, liangyiyi.wt, MODIFY, 2022/6/14, modify the isp_driving_current for solving the problem of RF interference
     .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
     .mipi_sensor_type = MIPI_OPHY_NCSI2,
     .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
     .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
     .mclk = 24,
     .mipi_lane_num = SENSOR_MIPI_4_LANE,
     .i2c_addr_table = {0x40, 0xff},
     .i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
     .mirror = IMAGE_NORMAL,
     .sensor_mode = IMGSENSOR_MODE_INIT,
     .shutter = 0x0100,
     .gain = 0xe0,
     .dummy_pixel = 0,
     .dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
     .current_fps = 300,
     .autoflicker_en = KAL_FALSE,
     .test_pattern = KAL_FALSE,
     .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
     .ihdr_en = 0,
     .i2c_write_id = 0x40,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[9] = {
     {8224, 6176, 16,   16, 8192, 6144, 2048, 1536,    0, 0,  2048, 1536, 0,0, 2048, 1536},  //preview(2048x1536)@30fps
     {8224, 6176, 16,   16, 8192, 6144, 4096, 3072,    0, 0,  4096, 3072, 0,0, 4096, 3072},  //capture(4096 x 3072)@30fps
     {8224, 6176, 16,   16, 8192, 6144, 4096, 3072,    0, 0,  4096, 3072, 0,0, 4096, 3072},  //capture1(4096 x 3072)@30fps
     {8224, 6176, 16,   24, 8192, 6128, 4096, 3064,   0,380,  4096, 2304, 0,0, 4096, 2304},  //normal_video(4096 x 2304)@30fps
     {8224, 6176, 16,   72, 8192, 6032, 2048, 1508,  24, 188, 2000, 1132, 0,0, 2000,  1132},  //hs_video(2000 x 1132)@120fps 
     {8224, 6176, 16,   16, 8192, 6144, 2048, 1536,  384,408, 1280,  720, 0,0, 1280,  720},  //slim video(1280 x 720)@120fps
     {8224, 6176, 16,   16, 8192, 6144, 4096, 3072,    0, 0,  4096, 3072, 0,0, 4096, 3072},  //custom(4096 x 3072)
     {8224, 6176, 16,   16, 8192, 6144, 8192, 6144,    0, 0,  8192, 6144, 0,0, 8192, 6144},  //custom Q2B_full(8192 x 6144)@10fps
     {8224, 6176, 16,  176, 8192, 5824, 2048, 1456,   64,188, 1920, 1080, 0,0, 1920, 1080},  //custom(1920 x 1080) FHD60fps
};

#if ENABLE_PDAF
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[2]=
{
     /* Capture mode setting */
      {0x02, //VC_Num
       0x0a, //VC_PixelNum
       0x00, //ModeSelect    /* 0:auto 1:direct */
       0x00, //EXPO_Ratio    /* 1/1, 1/2, 1/4, 1/8 */
       0x00, //0DValue        /* 0D Value */
       0x00, //RG_STATSMODE    /* STATS divistion mode 0:16x16  1:8x8    2:4x4  3:1x1 */
       0x00, 0x2B, 0x1000, 0x0C00,    // VC0 Maybe image data?
       0x00, 0x00, 0x0000, 0x0000,    // VC1 MVHDR
       0x01, 0x30, 0x0500, 0x0600,    // VC2 PDAF
       0x00, 0x00, 0x0000, 0x0000},   // VC3 ??
     /* Video mode setting */
      {0x02, //VC_Num
       0x0a, //VC_PixelNum
       0x00, //ModeSelect    /* 0:auto 1:direct */
       0x00, //EXPO_Ratio    /* 1/1, 1/2, 1/4, 1/8 */
       0x00, //0DValue		/* 0D Value */
       0x00, //RG_STATSMODE	 /* STATS divistion mode 0:16x16 1:8x82:4x4  3:1x1 */
       0x00, 0x2B, 0x1000, 0x0900,	// VC0 Maybe image data?
       0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
       0x01, 0x30, 0x0500, 0x0480,   // VC2 PDAF	0x01, 0x30, 0x03E0,0x02E8,
       0x00, 0x00, 0x0000, 0x0000},	 // VC3 ??
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
     .i4OffsetX   = 0,
     .i4OffsetY   = 0,
     .i4PitchX    = 8,
     .i4PitchY    = 8,
     .i4PairNum   = 4,
     .i4SubBlkW   = 8,
     .i4SubBlkH   = 2,
     .i4BlockNumX = 512,
     .i4BlockNumY = 384,
     .iMirrorFlip = 0,
     .i4PosR = { {0,0}, {2,3}, {6,4}, {4,7} },
     .i4PosL = { {1,0}, {3,3}, {7,4}, {5,7} },
     .i4Crop = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
};
static struct SET_PD_BLOCK_INFO_T imgsensor_4Kpd_info =
{
     .i4OffsetX   = 0,
     .i4OffsetY   = 0,
     .i4PitchX    = 8,
     .i4PitchY    = 8,
     .i4PairNum   = 4,
     .i4SubBlkW   = 8,
     .i4SubBlkH   = 2,
     .i4BlockNumX = 512,
     .i4BlockNumY = 288,
     .iMirrorFlip = 0,
     .i4PosR = { {0,0}, {2,3}, {4,7}, {6,4} },
     .i4PosL = { {1,0}, {3,3}, {5,7}, {7,4} },
     .i4Crop = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 384}, {0, 0}, {0, 0} },
};
#endif

static kal_uint16 hi5021q_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
     char puSendCmd[I2C_BUFFER_LEN];
     kal_uint32 tosend, IDX;
     kal_uint16 addr = 0, addr_last = 0, data;

     tosend = 0;
     IDX = 0;
     while (len > IDX) {
         addr = para[IDX];

         {
             puSendCmd[tosend++] = (char)(addr >> 8);
             puSendCmd[tosend++] = (char)(addr & 0xFF);
             data = para[IDX + 1];
             puSendCmd[tosend++] = (char)(data >> 8);
             puSendCmd[tosend++] = (char)(data & 0xFF);
             IDX += 2;
             addr_last = addr;
         }

         if ((I2C_BUFFER_LEN - tosend) < 4 ||
             len == IDX ||
             addr != addr_last) {
             iBurstWriteReg_multi(puSendCmd, tosend,
                 imgsensor.i2c_write_id,
                 4, imgsensor_info.i2c_speed);

             tosend = 0;
         }
     }
     return 0;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
     kal_uint16 get_byte = 0;
     char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

     iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

     return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
     char pu_send_cmd[4] = {(char)(addr >> 8),
         (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

     iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
     LOG_INF("dummyline = %d, dummypixels = %d\n",
         imgsensor.dummy_line, imgsensor.dummy_pixel);
     write_cmos_sensor(0x0210, imgsensor.frame_length & 0xFFFF);
     write_cmos_sensor(0x0206,imgsensor .line_length);
}    /*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
     return ((read_cmos_sensor(0x0716) << 8) | read_cmos_sensor(0x0717)) + 4;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
     kal_uint32 frame_length = imgsensor.frame_length;

     frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
     spin_lock(&imgsensor_drv_lock);
     imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
     imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

     if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
         imgsensor.frame_length = imgsensor_info.max_frame_length;
         imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
     }
     if (min_framelength_en)
         imgsensor.min_frame_length = imgsensor.frame_length;

     spin_unlock(&imgsensor_drv_lock);
     set_dummy();
}    /*    set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
     kal_uint16 realtime_fps = 0;

     spin_lock(&imgsensor_drv_lock);

     if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
         imgsensor.frame_length = shutter + imgsensor_info.margin;
     else
         imgsensor.frame_length = imgsensor.min_frame_length;
     if (imgsensor.frame_length > imgsensor_info.max_frame_length)
         imgsensor.frame_length = imgsensor_info.max_frame_length;
     spin_unlock(&imgsensor_drv_lock);

     LOG_INF("shutter = %d, imgsensor.frame_length = %d,imgsensor.min_frame_length = %d\n",
         shutter, imgsensor.frame_length, imgsensor.min_frame_length);

     shutter = (shutter < imgsensor_info.min_shutter) ?
         imgsensor_info.min_shutter : shutter;
     shutter = (shutter >
         (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
         (imgsensor_info.max_frame_length - imgsensor_info.margin) :
         shutter;
     if (imgsensor.autoflicker_en) {
         realtime_fps = imgsensor.pclk * 10 /
             (imgsensor.line_length * imgsensor.frame_length);
         if (realtime_fps >= 297 && realtime_fps <= 305)
             set_max_framerate(296, 0);
         else if (realtime_fps >= 147 && realtime_fps <= 150)
             set_max_framerate(146, 0);
         else
             write_cmos_sensor(0x0210, imgsensor.frame_length);

     } else{
             write_cmos_sensor(0x0210, imgsensor.frame_length);
     }

     write_cmos_sensor(0x0510, shutter);
     write_cmos_sensor(0x0512, (shutter >> 16) & 0xFFFF);

     LOG_INF("frame_length = %d , shutter = %d \n",imgsensor.frame_length, shutter);
}    /*    write_shutter  */


static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
     unsigned long flags;
     kal_int32 dummy_line = 0;
     kal_uint16 realtime_fps = 0;
     spin_lock_irqsave(&imgsensor_drv_lock, flags);
     imgsensor.shutter = shutter;
     spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
     spin_lock(&imgsensor_drv_lock);
     /*Change frame time */
     if (frame_length > 1)
         dummy_line = frame_length - imgsensor.frame_length;
     imgsensor.frame_length = imgsensor.frame_length + dummy_line;
     /*  */
     if (shutter > imgsensor.frame_length - imgsensor_info.margin)
         imgsensor.frame_length = shutter + imgsensor_info.margin;

     if (imgsensor.frame_length > imgsensor_info.max_frame_length)
         imgsensor.frame_length = imgsensor_info.max_frame_length;
     spin_unlock(&imgsensor_drv_lock);

     shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
     shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

     set_dummy();

     if (imgsensor.autoflicker_en) {
         realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
         if(realtime_fps >= 297 && realtime_fps <= 305)
             set_max_framerate(296, 0);
         else if(realtime_fps >= 147 && realtime_fps <= 150)
             set_max_framerate(146, 0);
     } else {
         // Extend frame length
         write_cmos_sensor(0x0210, imgsensor.frame_length & 0xFFFF);
     }

     // Update Shutter
     write_cmos_sensor(0x0510, shutter);
     write_cmos_sensor(0x0512, (shutter >> 16) & 0xFFFF);
     LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}    /*    write_shutter  */

/*************************************************************************
  * FUNCTION
  *    set_shutter
  *
  * DESCRIPTION
  *    This function set e-shutter of sensor to change exposure time.
  *
  * PARAMETERS
  *    iShutter : exposured lines
  *
  * RETURNS
  *    None
  *
  * GLOBALS AFFECTED
  *
  *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
     unsigned long flags;

     LOG_INF("set_shutter");
     spin_lock_irqsave(&imgsensor_drv_lock, flags);
     imgsensor.shutter = shutter;
     spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

     write_shutter(shutter);
}    /*    set_shutter */

/*************************************************************************
  * FUNCTION
  *    set_gain
  *
  * DESCRIPTION
  *    This function is to set global gain to sensor.
  *
  * PARAMETERS
  *    iGain : sensor global gain(base: 0x40)
  *
  * RETURNS
  *    the actually gain set to sensor.
  *
  * GLOBALS AFFECTED
  *
  *************************************************************************/
static kal_uint16 gain2reg(kal_uint16 gain)
{
     kal_uint16 reg_gain = 0x0000;
     reg_gain = gain / 4 - 16;

     return (kal_uint16)reg_gain;

}

static kal_uint16 set_gain(kal_uint16 gain)
{
     kal_uint16 reg_gain;

     /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
     /* [0:3] = N meams N /16 X    */
     /* [4:9] = M meams M X         */
     /* Total gain = M + N /16 X   */

     if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
         LOG_INF("Error gain setting");

         if (gain < BASEGAIN)
             gain = BASEGAIN;
         else if (gain > 32 * BASEGAIN)
             gain = 32 * BASEGAIN;
     }

     reg_gain = gain2reg(gain);
     spin_lock(&imgsensor_drv_lock);
     imgsensor.gain = reg_gain;
     spin_unlock(&imgsensor_drv_lock);
     LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

     write_cmos_sensor(0x050A,reg_gain);
     return gain;

}

#if 1
static void set_mirror_flip(kal_uint8 image_mirror)
{
     int mirrorflip;
     mirrorflip = read_cmos_sensor(0x0202);
     LOG_INF("image_mirror = %d", image_mirror);
     switch (image_mirror) {
     case IMAGE_NORMAL:
         write_cmos_sensor(0x0202, mirrorflip|0x0000);
         break;
     case IMAGE_H_MIRROR:
         write_cmos_sensor(0x0202, mirrorflip|0x0100);
         break;
     case IMAGE_V_MIRROR:
         write_cmos_sensor(0x0202, mirrorflip|0x0280);
         break;
     case IMAGE_HV_MIRROR:
         write_cmos_sensor(0x0202, mirrorflip|0x03c0);
         break;
     default:
         LOG_INF("Error image_mirror setting");
         break;
     }
}
#endif
/*************************************************************************
  * FUNCTION
  *    night_mode
  *
  * DESCRIPTION
  *    This function night mode of sensor.
  *
  * PARAMETERS
  *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
  *
  * RETURNS
  *    None
  *
  * GLOBALS AFFECTED
  *
  *************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}    /*    night_mode    */

static void sensor_init(void)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_init_hi5021q,
         sizeof(addr_data_pair_init_hi5021q) /
         sizeof(kal_uint16));
}

static void preview_setting(void)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_preview_hi5021q,
         sizeof(addr_data_pair_preview_hi5021q) /
         sizeof(kal_uint16));
}

static void capture_setting(kal_uint16 currefps)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_capture_30fps_hi5021q,
         sizeof(addr_data_pair_capture_30fps_hi5021q) /
         sizeof(kal_uint16));
}

static void video_setting(void)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_video_hi5021q,
         sizeof(addr_data_pair_video_hi5021q) /
         sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_hs_video_hi5021q,
         sizeof(addr_data_pair_hs_video_hi5021q) /
         sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_slim_video_hi5021q,
         sizeof(addr_data_pair_slim_video_hi5021q) /
         sizeof(kal_uint16));
}

static void custom1_setting(void)
{
     LOG_INF("E\n");
     hi5021q_table_write_cmos_sensor(
         addr_data_pair_custom1_hi5021q,
         sizeof(addr_data_pair_custom1_hi5021q) /
         sizeof(kal_uint16));
}

static void custom2_setting(void)
{
           LOG_INF("E\n");
           hi5021q_table_write_cmos_sensor(
                addr_data_pair_custom2_hi5021q,
                sizeof(addr_data_pair_custom2_hi5021q) /
                sizeof(kal_uint16));
}

// -bug727089, liangyiyi.wt, MODIFY, 2022/5/16, modify the cap mipi clk 2148Mbps for solving the problem of RF interference
static void custom3_setting(void)
{
           LOG_INF("E\n");
           hi5021q_table_write_cmos_sensor(
                addr_data_pair_custom3_hi5021q,
                sizeof(addr_data_pair_custom3_hi5021q) /
                sizeof(kal_uint16));
}

static void custom4_setting(void)
{
           LOG_INF("E\n");
           hi5021q_table_write_cmos_sensor(
                addr_data_pair_custom4_hi5021q,
                sizeof(addr_data_pair_custom4_hi5021q) /
                sizeof(kal_uint16));
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
     kal_uint8 i = 0;
     kal_uint8 retry = 2;
     kal_int32 eepromModuleValue = 0;

     while (imgsensor_info.i2c_addr_table[i] != 0xff) {
         spin_lock(&imgsensor_drv_lock);
         imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
         spin_unlock(&imgsensor_drv_lock);
         do {
             *sensor_id = return_sensor_id();
             if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("truly i2c write id : 0x%x, sensor id: 0x%x\n",imgsensor.i2c_write_id, *sensor_id);

                eepromModuleValue = hi5021q_read_eeprom(0x0001);
                LOG_INF("truly eepromModuleValue : 0x%x,", eepromModuleValue);
                if((n28hi5021qreartruly_eeprom_data.sensorVendorid >> 24) != eepromModuleValue){
                    *sensor_id = 0xFFFFFFFF;
                    LOG_INF("Fail probe:the otp eepromModuleValue: 0x%x, is not the same with sensorVendorid", eepromModuleValue);
                    return ERROR_SENSOR_CONNECT_FAIL;
                }
                hi5021qtruly_init_sensor_cali_info();
                return ERROR_NONE;
             }
             retry--;
         } while (retry > 0);
         i++;
         retry = 2;
     }
     if (*sensor_id != imgsensor_info.sensor_id) {
        LOG_INF("Read id fail,sensor id: 0x%x\n", *sensor_id);
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
     }
     return ERROR_NONE;
}

/*************************************************************************
  * FUNCTION
  *    open
  *
  * DESCRIPTION
  *    This function initialize the registers of CMOS sensor
  *
  * PARAMETERS
  *    None
  *
  * RETURNS
  *    None
  *
  * GLOBALS AFFECTED
  *
  *************************************************************************/
static kal_uint32 open(void)
{
     kal_uint8 i = 0;
     kal_uint8 retry = 2;
     kal_uint16 sensor_id = 0;

     LOG_INF("[open]: PLATFORM:MT6737,MIPI 24LANE\n");
     LOG_INF("preview 1296*972@30fps,360Mbps/lane;"
         "capture 2592*1944@30fps,880Mbps/lane\n");
     while (imgsensor_info.i2c_addr_table[i] != 0xff) {
         spin_lock(&imgsensor_drv_lock);
         imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
         spin_unlock(&imgsensor_drv_lock);
         do {
             sensor_id = return_sensor_id();
             if (sensor_id == imgsensor_info.sensor_id) {
                 LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
                     imgsensor.i2c_write_id, sensor_id);
                 break;
             }
             retry--;
         } while (retry > 0);
         i++;
         if (sensor_id == imgsensor_info.sensor_id)
             break;
         retry = 2;
     }
     if (imgsensor_info.sensor_id != sensor_id) {
         LOG_INF("open sensor id fail: 0x%x\n", sensor_id);
         return ERROR_SENSOR_CONNECT_FAIL;
     }
     /* initail sequence write in  */
     sensor_init();
     Truly_apply_sensor_Cali();

     spin_lock(&imgsensor_drv_lock);
     imgsensor.autoflicker_en = KAL_FALSE;
     imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
     imgsensor.pclk = imgsensor_info.pre.pclk;
     imgsensor.frame_length = imgsensor_info.pre.framelength;
     imgsensor.line_length = imgsensor_info.pre.linelength;
     imgsensor.min_frame_length = imgsensor_info.pre.framelength;
     imgsensor.dummy_pixel = 0;
     imgsensor.dummy_line = 0;
     imgsensor.ihdr_en = 0;
     imgsensor.test_pattern = KAL_FALSE;
     imgsensor.current_fps = imgsensor_info.pre.max_framerate;
     //imgsensor.pdaf_mode = 1;
     spin_unlock(&imgsensor_drv_lock);
     return ERROR_NONE;
}    /*    open  */
static kal_uint32 close(void)
{
     return ERROR_NONE;
}    /*    close  */

/*************************************************************************
  * FUNCTION
  * preview
  *
  * DESCRIPTION
  *    This function start the sensor preview.
  *
  * PARAMETERS
  *    *image_window : address pointer of pixel numbers in one period of
HSYNC
  *  *sensor_config_data : address pointer of line numbers in one period
of VSYNC
  *
  * RETURNS
  *    None
  *
  * GLOBALS AFFECTED
  *
  *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
             MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     LOG_INF("E");
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
     imgsensor.pclk = imgsensor_info.pre.pclk;
     imgsensor.line_length = imgsensor_info.pre.linelength;
     imgsensor.frame_length = imgsensor_info.pre.framelength;
     imgsensor.min_frame_length = imgsensor_info.pre.framelength;
     imgsensor.current_fps = imgsensor_info.pre.max_framerate;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);
     preview_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}    /*    preview   */

/*************************************************************************
  * FUNCTION
  *    capture
  *
  * DESCRIPTION
  *    This function setup the CMOS sensor in capture MY_OUTPUT mode
  *
  * PARAMETERS
  *
  * RETURNS
  *    None
  *
  * GLOBALS AFFECTED
  *
  *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
             MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

     if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)    {
         imgsensor.pclk = imgsensor_info.cap.pclk;
         imgsensor.line_length = imgsensor_info.cap.linelength;
         imgsensor.frame_length = imgsensor_info.cap.framelength;
         imgsensor.min_frame_length = imgsensor_info.cap.framelength;
         imgsensor.autoflicker_en = KAL_FALSE;
     } else {
      //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
         imgsensor.pclk = imgsensor_info.cap1.pclk;
         imgsensor.line_length = imgsensor_info.cap1.linelength;
         imgsensor.frame_length = imgsensor_info.cap1.framelength;
         imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
         imgsensor.autoflicker_en = KAL_FALSE;
     }

     spin_unlock(&imgsensor_drv_lock);
     LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
     capture_setting(imgsensor.current_fps);
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;

}    /* capture() */
//    #if 0 //normal video, hs video, slim video to be added

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
               MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
     imgsensor.pclk = imgsensor_info.normal_video.pclk;
     imgsensor.line_length = imgsensor_info.normal_video.linelength;
     imgsensor.frame_length = imgsensor_info.normal_video.framelength;
     imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
     imgsensor.current_fps = 300;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);
     video_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
             MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
     imgsensor.pclk = imgsensor_info.hs_video.pclk;
     //imgsensor.video_mode = KAL_TRUE;
     imgsensor.line_length = imgsensor_info.hs_video.linelength;
     imgsensor.frame_length = imgsensor_info.hs_video.framelength;
     imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
     imgsensor.dummy_line = 0;
     imgsensor.dummy_pixel = 0;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);
     hs_video_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
               MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
     imgsensor.pclk = imgsensor_info.slim_video.pclk;
     imgsensor.line_length = imgsensor_info.slim_video.linelength;
     imgsensor.frame_length = imgsensor_info.slim_video.framelength;
     imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
     imgsensor.dummy_line = 0;
     imgsensor.dummy_pixel = 0;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);
     slim_video_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}    /*    slim_video     */
//#endif

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                 MSDK_SENSOR_CONFIG_STRUCT
*sensor_config_data)
{
     LOG_INF("E\n");

     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
     imgsensor.pclk = imgsensor_info.custom1.pclk;
     imgsensor.line_length = imgsensor_info.custom1.linelength;
     imgsensor.frame_length = imgsensor_info.custom1.framelength;
     imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
     imgsensor.dummy_line = 0;
     imgsensor.dummy_pixel = 0;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);
     custom1_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
     imgsensor.pclk = imgsensor_info.custom2.pclk;
     imgsensor.line_length = imgsensor_info.custom2.linelength;
     imgsensor.frame_length = imgsensor_info.custom2.framelength;
     imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
     imgsensor.dummy_line = 0;
     imgsensor.dummy_pixel = 0;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);

     custom2_setting();
     set_mirror_flip(imgsensor.mirror);
      return ERROR_NONE;
}

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
     imgsensor.pclk = imgsensor_info.custom3.pclk;
     imgsensor.line_length = imgsensor_info.custom3.linelength;
     imgsensor.frame_length = imgsensor_info.custom3.framelength;
     imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
     imgsensor.dummy_line = 0;
     imgsensor.dummy_pixel = 0;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);

     custom3_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}   /*  Custom3    */

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     spin_lock(&imgsensor_drv_lock);
     imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
     imgsensor.pclk = imgsensor_info.custom4.pclk;
     imgsensor.line_length = imgsensor_info.custom4.linelength;
     imgsensor.frame_length = imgsensor_info.custom4.framelength;
     imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
     imgsensor.dummy_line = 0;
     imgsensor.dummy_pixel = 0;
     imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);

     custom4_setting();
     set_mirror_flip(imgsensor.mirror);
     return ERROR_NONE;
}   /*  Custom4    */

static kal_uint32 get_resolution(
         MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
     sensor_resolution->SensorFullWidth =
         imgsensor_info.cap.grabwindow_width;
     sensor_resolution->SensorFullHeight =
         imgsensor_info.cap.grabwindow_height;

     sensor_resolution->SensorPreviewWidth =
         imgsensor_info.pre.grabwindow_width;
     sensor_resolution->SensorPreviewHeight =
         imgsensor_info.pre.grabwindow_height;

//#if 0
     sensor_resolution->SensorVideoWidth =
         imgsensor_info.normal_video.grabwindow_width;
     sensor_resolution->SensorVideoHeight =
         imgsensor_info.normal_video.grabwindow_height;

     sensor_resolution->SensorHighSpeedVideoWidth =
         imgsensor_info.hs_video.grabwindow_width;
     sensor_resolution->SensorHighSpeedVideoHeight =
         imgsensor_info.hs_video.grabwindow_height;

     sensor_resolution->SensorSlimVideoWidth =
         imgsensor_info.slim_video.grabwindow_width;
     sensor_resolution->SensorSlimVideoHeight =
         imgsensor_info.slim_video.grabwindow_height;

     sensor_resolution->SensorCustom1Width  =
         imgsensor_info.custom1.grabwindow_width;
     sensor_resolution->SensorCustom1Height  =
         imgsensor_info.custom1.grabwindow_height;
//#endif
     sensor_resolution->SensorCustom2Width =
         imgsensor_info.custom2.grabwindow_width;
     sensor_resolution->SensorCustom2Height =
         imgsensor_info.custom2.grabwindow_height;

     sensor_resolution->SensorCustom3Width =
         imgsensor_info.custom3.grabwindow_width;
     sensor_resolution->SensorCustom3Height =
         imgsensor_info.custom3.grabwindow_height;

    sensor_resolution->SensorCustom4Width =
         imgsensor_info.custom4.grabwindow_width;
     sensor_resolution->SensorCustom4Height =
         imgsensor_info.custom4.grabwindow_height;
     return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
             MSDK_SENSOR_INFO_STRUCT *sensor_info,
             MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     LOG_INF("scenario_id = %d\n", scenario_id);

     sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
     sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
     sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
     sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
     sensor_info->SensorInterruptDelayLines = 4; /* not use */
     sensor_info->SensorResetActiveHigh = FALSE; /* not use */
     sensor_info->SensorResetDelayCount = 5; /* not use */

     sensor_info->SensroInterfaceType =
     imgsensor_info.sensor_interface_type;
     sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
     sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
     sensor_info->SensorOutputDataFormat =
         imgsensor_info.sensor_output_dataformat;

     sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
     sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
     sensor_info->VideoDelayFrame =
         imgsensor_info.video_delay_frame;
     sensor_info->HighSpeedVideoDelayFrame =
         imgsensor_info.hs_video_delay_frame;
     sensor_info->SlimVideoDelayFrame =
         imgsensor_info.slim_video_delay_frame;

     sensor_info->Custom1DelayFrame =
         imgsensor_info.custom1_delay_frame;
     sensor_info->Custom2DelayFrame =
         imgsensor_info.custom2_delay_frame;
     sensor_info->Custom3DelayFrame =
         imgsensor_info.custom3_delay_frame;
    sensor_info->Custom4DelayFrame =
         imgsensor_info.custom4_delay_frame;
     sensor_info->SensorMasterClockSwitch = 0; /* not use */
     sensor_info->SensorDrivingCurrent =
         imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
     sensor_info->AEShutDelayFrame =
         imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
     sensor_info->AESensorGainDelayFrame =
         imgsensor_info.ae_sensor_gain_delay_frame;
     sensor_info->AEISPGainDelayFrame =
         imgsensor_info.ae_ispGain_delay_frame;
     sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;//bug 720367 qinduilin.wt, add, 2022/03/03, modify for dualcam sync
     sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
     sensor_info->IHDR_LE_FirstLine =
         imgsensor_info.ihdr_le_firstline;
     sensor_info->SensorModeNum =
         imgsensor_info.sensor_mode_num;

     sensor_info->SensorMIPILaneNumber =
         imgsensor_info.mipi_lane_num;
     sensor_info->SensorClockFreq = imgsensor_info.mclk;
     sensor_info->SensorClockDividCount = 3; /* not use */
     sensor_info->SensorClockRisingCount = 0;
     sensor_info->SensorClockFallingCount = 2; /* not use */
     sensor_info->SensorPixelClockCount = 3; /* not use */
     sensor_info->SensorDataLatchCount = 2; /* not use */

     sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
     sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
     sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
     sensor_info->SensorHightSampling = 0;    // 0 is default 1x
     sensor_info->SensorPacketECCOrder = 1;

#if ENABLE_PDAF
     sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#endif

     switch (scenario_id) {
     case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
         sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
                 imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
     break;
     case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
         sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
             imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
     break;
     case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
         sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
             imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
     break;
     case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
         sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
             imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
     break;
     case MSDK_SCENARIO_ID_SLIM_VIDEO:
         sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
             imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
     break;
     case MSDK_SCENARIO_ID_CUSTOM1:
         sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
         break;
     case MSDK_SCENARIO_ID_CUSTOM2:
         sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

         break;
     case MSDK_SCENARIO_ID_CUSTOM3:
         sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
     break;
     case MSDK_SCENARIO_ID_CUSTOM4:
         sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
     break;
     default:
         sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
         sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

         sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
             imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
     break;
     }

     return ERROR_NONE;
}    /*    get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
             MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
             MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
     LOG_INF("scenario_id = %d\n", scenario_id);
     spin_lock(&imgsensor_drv_lock);
     imgsensor.current_scenario_id = scenario_id;
     spin_unlock(&imgsensor_drv_lock);
     switch (scenario_id) {
     case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
         LOG_INF("preview\n");
         preview(image_window, sensor_config_data);
         break;
     case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
         LOG_INF("capture\n");
         capture(image_window, sensor_config_data);
         break;
     case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
         LOG_INF("video preview\n");
         normal_video(image_window, sensor_config_data);
         break;
     case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
         hs_video(image_window, sensor_config_data);
         break;
     case MSDK_SCENARIO_ID_SLIM_VIDEO:
         slim_video(image_window, sensor_config_data);
         break;
     case MSDK_SCENARIO_ID_CUSTOM1:
         Custom1(image_window, sensor_config_data); // Custom1
         break;
     case MSDK_SCENARIO_ID_CUSTOM2:
         Custom2(image_window, sensor_config_data);
         break;
     case MSDK_SCENARIO_ID_CUSTOM3:
         Custom3(image_window, sensor_config_data);
         break;
    case MSDK_SCENARIO_ID_CUSTOM4:
         Custom4(image_window, sensor_config_data);
         break;
     default:
         LOG_INF("default mode\n");
         preview(image_window, sensor_config_data);
         return ERROR_INVALID_SCENARIO_ID;
     }
     return ERROR_NONE;
}    /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
     LOG_INF("framerate = %d ", framerate);
     // SetVideoMode Function should fix framerate
     if (framerate == 0)
         // Dynamic frame rate
         return ERROR_NONE;
     spin_lock(&imgsensor_drv_lock);

     if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
         imgsensor.current_fps = 296;
     else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
         imgsensor.current_fps = 146;
     else
         imgsensor.current_fps = 10 * framerate;
     spin_unlock(&imgsensor_drv_lock);
     set_max_framerate(imgsensor.current_fps, 1);
     set_dummy();
     return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
             UINT16 framerate)
{
     LOG_INF("enable = %d, framerate = %d ", enable, framerate);
     spin_lock(&imgsensor_drv_lock);
     if (enable)
         imgsensor.autoflicker_en = KAL_TRUE;
     else //Cancel Auto flick
         imgsensor.autoflicker_en = KAL_FALSE;
     spin_unlock(&imgsensor_drv_lock);
     return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
             enum MSDK_SCENARIO_ID_ENUM scenario_id,
             MUINT32 framerate)
{
     kal_uint32 frame_length;

     LOG_INF("scenario_id = %d, framerate = %d\n",
                 scenario_id, framerate);

     switch (scenario_id) {
     case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
         frame_length = imgsensor_info.pre.pclk / framerate * 10 /
             imgsensor_info.pre.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length >
             imgsensor_info.pre.framelength) ?
             (frame_length - imgsensor_info.pre.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.pre.framelength +
             imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
     break;
     case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
         if (framerate == 0)
             return ERROR_NONE;
         frame_length = imgsensor_info.normal_video.pclk /
             framerate * 10 / imgsensor_info.normal_video.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length >
             imgsensor_info.normal_video.framelength) ?
         (frame_length - imgsensor_info.normal_video.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.normal_video.framelength +
             imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
     break;
     case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
         if (imgsensor.current_fps ==
                 imgsensor_info.cap1.max_framerate) {
         frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
                 imgsensor_info.cap1.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length >
             imgsensor_info.cap1.framelength) ?
             (frame_length - imgsensor_info.cap1.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.cap1.framelength +
                 imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         } else {
             if (imgsensor.current_fps !=
                 imgsensor_info.cap.max_framerate)
             LOG_INF("fps %d fps not support,use cap: %d fps!\n",
             framerate, imgsensor_info.cap.max_framerate/10);
             frame_length = imgsensor_info.cap.pclk /
                 framerate * 10 / imgsensor_info.cap.linelength;
             spin_lock(&imgsensor_drv_lock);
             imgsensor.dummy_line = (frame_length >
                 imgsensor_info.cap.framelength) ?
             (frame_length - imgsensor_info.cap.framelength) : 0;
             imgsensor.frame_length =
                 imgsensor_info.cap.framelength +
                 imgsensor.dummy_line;
             imgsensor.min_frame_length = imgsensor.frame_length;
             spin_unlock(&imgsensor_drv_lock);
         }
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
     break;
     case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
         frame_length = imgsensor_info.hs_video.pclk /
             framerate * 10 / imgsensor_info.hs_video.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length >
             imgsensor_info.hs_video.framelength) ? (frame_length -
             imgsensor_info.hs_video.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.hs_video.framelength +
             imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
     break;
     case MSDK_SCENARIO_ID_SLIM_VIDEO:
         frame_length = imgsensor_info.slim_video.pclk /
             framerate * 10 / imgsensor_info.slim_video.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length >
             imgsensor_info.slim_video.framelength) ? (frame_length -
             imgsensor_info.slim_video.framelength) : 0;
         imgsensor.frame_length =
             imgsensor_info.slim_video.framelength +
             imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
     break;
     case MSDK_SCENARIO_ID_CUSTOM1:
         frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
     break;
     case MSDK_SCENARIO_ID_CUSTOM2:
         frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
         break;
     case MSDK_SCENARIO_ID_CUSTOM3:
         frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
         break;
    case MSDK_SCENARIO_ID_CUSTOM4:
         frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ? (frame_length - imgsensor_info.custom4.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
         break;
     default:  //coding with  preview scenario by default
         frame_length = imgsensor_info.pre.pclk / framerate * 10 /
                         imgsensor_info.pre.linelength;
         spin_lock(&imgsensor_drv_lock);
         imgsensor.dummy_line = (frame_length >
             imgsensor_info.pre.framelength) ?
             (frame_length - imgsensor_info.pre.framelength) : 0;
         imgsensor.frame_length = imgsensor_info.pre.framelength +
                 imgsensor.dummy_line;
         imgsensor.min_frame_length = imgsensor.frame_length;
         spin_unlock(&imgsensor_drv_lock);
         if (imgsensor.frame_length > imgsensor.shutter)
             set_dummy();
         LOG_INF("error scenario_id = %d, we use preview scenario\n",
                 scenario_id);
     break;
     }
     return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
                 enum MSDK_SCENARIO_ID_ENUM scenario_id,
                 MUINT32 *framerate)
{
     LOG_INF("scenario_id = %d\n", scenario_id);

     switch (scenario_id) {
     case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
         *framerate = imgsensor_info.pre.max_framerate;
     break;
     case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
         *framerate = imgsensor_info.normal_video.max_framerate;
     break;
     case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
         *framerate = imgsensor_info.cap.max_framerate;
     break;
     case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
         *framerate = imgsensor_info.hs_video.max_framerate;
     break;
     case MSDK_SCENARIO_ID_SLIM_VIDEO:
         *framerate = imgsensor_info.slim_video.max_framerate;
     break;
     case MSDK_SCENARIO_ID_CUSTOM1:
         *framerate = imgsensor_info.custom1.max_framerate;
     break;
     case MSDK_SCENARIO_ID_CUSTOM2:
         *framerate = imgsensor_info.custom2.max_framerate;
         break;
     case MSDK_SCENARIO_ID_CUSTOM3:
         *framerate = imgsensor_info.custom3.max_framerate;
         break;
     case MSDK_SCENARIO_ID_CUSTOM4:
         *framerate = imgsensor_info.custom4.max_framerate;
         break;
     default:
         break;
     }

     return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
     LOG_INF("set_test_pattern_mode enable: %d", enable);

     if(enable) {
         write_cmos_sensor(0x0b04, 0x1F3B);
         write_cmos_sensor(0x0C0A, 0x0100);
     } else {
         write_cmos_sensor(0x0b04, 0x1F3A);
         write_cmos_sensor(0x0C0A, 0x0000);
     }
     spin_lock(&imgsensor_drv_lock);
     imgsensor.test_pattern = enable;
     spin_unlock(&imgsensor_drv_lock);
     return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
     pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
     if (enable)
         write_cmos_sensor(0x0b00, 0x0100); // stream on
     else
         write_cmos_sensor(0x0b00, 0x0000); // stream off
     mdelay(5);
     return ERROR_NONE;
}

static kal_uint32 feature_control(
             MSDK_SENSOR_FEATURE_ENUM feature_id,
             UINT8 *feature_para, UINT32 *feature_para_len)
{
     UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
     UINT16 *feature_data_16 = (UINT16 *) feature_para;
     UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
     UINT32 *feature_data_32 = (UINT32 *) feature_para;
     INT32 *feature_return_para_i32 = (INT32 *) feature_para;

#if ENABLE_PDAF
     struct SET_PD_BLOCK_INFO_T *PDAFinfo;
     struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
     unsigned long long *feature_data =
         (unsigned long long *) feature_para;

     struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
     MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
         (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

     LOG_INF("feature_id = %d\n", feature_id);
     switch (feature_id) {

#if 0//ENABLE_SEAMLESS
   case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
        LOG_INF(" feature_id = %d\n", feature_id);
    pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
        LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n", *feature_data, *pScenarios);
        *pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
        LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n", *feature_data, *pScenarios);
    switch (*feature_data) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
      *pScenarios = MSDK_SCENARIO_ID_CUSTOM1;
      break;
    case MSDK_SCENARIO_ID_CUSTOM1:
      *pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
      break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
    case MSDK_SCENARIO_ID_CUSTOM2:
    case MSDK_SCENARIO_ID_CUSTOM4:
    case MSDK_SCENARIO_ID_CUSTOM5:
    case MSDK_SCENARIO_ID_CUSTOM3:
    default:
      *pScenarios = 0xff;
      break;
    }
    LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n", *feature_data, *pScenarios);
    break;

    case SENSOR_FEATURE_SEAMLESS_SWITCH:
    pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
    if (pAeCtrls)
      seamless_switch((*feature_data),*pAeCtrls,*(pAeCtrls+1),*(pAeCtrls+4),*(pAeCtrls+5));
    else
      seamless_switch((*feature_data), 0, 0, 0, 0);
#endif

  case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
    *(feature_data + 1) = imgsensor_info.min_gain;
    *(feature_data + 2) = imgsensor_info.max_gain;
    break;
  case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
    *(feature_data + 0) = imgsensor_info.min_gain_iso;
    *(feature_data + 1) = imgsensor_info.gain_step;
    *(feature_data + 2) = imgsensor_info.gain_type;
    break;
  case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
  *(feature_data + 1) = imgsensor_info.min_shutter;
    switch (*feature_data) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
    case MSDK_SCENARIO_ID_CUSTOM1:
    case MSDK_SCENARIO_ID_CUSTOM2:
    case MSDK_SCENARIO_ID_CUSTOM4:
    case MSDK_SCENARIO_ID_CUSTOM5:
      *(feature_data + 2) = 2;
      break;
    case MSDK_SCENARIO_ID_CUSTOM3:
    default:
      *(feature_data + 2) = 1;
      break;
    }
    break;
      case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
    switch (*feature_data) {
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.cap.pclk;
      break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.normal_video.pclk;
      break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.hs_video.pclk;
      break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.slim_video.pclk;
      break;
    case MSDK_SCENARIO_ID_CUSTOM1:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.custom1.pclk;
      break;
    case MSDK_SCENARIO_ID_CUSTOM2:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.custom2.pclk;
      break;
    case MSDK_SCENARIO_ID_CUSTOM3:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.custom3.pclk;
      break;
    case MSDK_SCENARIO_ID_CUSTOM4:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.custom4.pclk;
        break;
    case MSDK_SCENARIO_ID_CUSTOM5:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.custom5.pclk;
        break;
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    default:
      *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
        = imgsensor_info.pre.pclk;
      break;
    }
    break;
     case SENSOR_FEATURE_GET_PERIOD:
         *feature_return_para_16++ = imgsensor.line_length;
         *feature_return_para_16 = imgsensor.frame_length;
         *feature_para_len = 4;
     break;
     case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
         *feature_return_para_32 = imgsensor.pclk;
         *feature_para_len = 4;
     break;
     case SENSOR_FEATURE_SET_ESHUTTER:
         set_shutter(*feature_data);
     break;
     case SENSOR_FEATURE_SET_NIGHTMODE:
         night_mode((BOOL) * feature_data);
     break;
     case SENSOR_FEATURE_SET_GAIN:
         set_gain((UINT16) *feature_data);
     break;
     case SENSOR_FEATURE_SET_FLASHLIGHT:
     break;
     case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
     break;
     case SENSOR_FEATURE_SET_REGISTER:
         write_cmos_sensor(sensor_reg_data->RegAddr,
                         sensor_reg_data->RegData);
     break;
     case SENSOR_FEATURE_GET_REGISTER:
         sensor_reg_data->RegData =
                 read_cmos_sensor(sensor_reg_data->RegAddr);
     break;
     case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
         *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
         *feature_para_len = 4;
     break;
     case SENSOR_FEATURE_SET_VIDEO_MODE:
         set_video_mode(*feature_data);
     break;
     case SENSOR_FEATURE_CHECK_SENSOR_ID:
         get_imgsensor_id(feature_return_para_32);
     break;
     case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
         set_auto_flicker_mode((BOOL)*feature_data_16,
             *(feature_data_16+1));
     break;
     case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
         set_max_framerate_by_scenario(
             (enum MSDK_SCENARIO_ID_ENUM)*feature_data,
             *(feature_data+1));
     break;
     case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
         get_default_framerate_by_scenario(
             (enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
             (MUINT32 *)(uintptr_t)(*(feature_data+1)));
     break;
     case SENSOR_FEATURE_SET_TEST_PATTERN:
         set_test_pattern_mode((BOOL)*feature_data);
     break;
     case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
         *feature_return_para_32 = imgsensor_info.checksum_value;
         *feature_para_len = 4;
     break;
     //+bug727089, liangyiyi.wt,MODIFY, 2022.03.09, fix KASAN opened to Monitor memory, camera  memory access error.
     case SENSOR_FEATURE_SET_FRAMERATE:
         LOG_INF("current fps :%d\n", *feature_data_32);
         spin_lock(&imgsensor_drv_lock);
         imgsensor.current_fps = (UINT16)*feature_data_32;
         spin_unlock(&imgsensor_drv_lock);
     break;
     case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
         set_shutter_frame_length((UINT32) *feature_data, (UINT16) *(feature_data + 1));
     break;

     case SENSOR_FEATURE_SET_HDR:
         LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
         spin_lock(&imgsensor_drv_lock);
         imgsensor.ihdr_en =  (BOOL)*feature_data_32;
         spin_unlock(&imgsensor_drv_lock);
     break;
     //-bug727089, liangyiyi.wt,MODIFY, 2022.03.09, fix KASAN opened to Monitor memory, camera  memory access error.
     case SENSOR_FEATURE_GET_CROP_INFO:
         LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
                 (UINT32)*feature_data);

         wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
             (uintptr_t)(*(feature_data+1));

         switch (*feature_data_32) {
         case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[1],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[3],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[4],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_SLIM_VIDEO:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[5],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_CUSTOM1:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[6],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_CUSTOM2:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[7],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_CUSTOM3:
         memcpy((void *)wininfo,
             (void *)&imgsensor_winsize_info[8],
             sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_CUSTOM4:
         memcpy((void *)wininfo,
             (void *)&imgsensor_winsize_info[8],
             sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
         default:
             memcpy((void *)wininfo,
                 (void *)&imgsensor_winsize_info[0],
                 sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
         break;
         }
     break;
     case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
         LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
             (UINT16)*feature_data, (UINT16)*(feature_data+1),
             (UINT16)*(feature_data+2));
     #if 0
         ihdr_write_shutter_gain((UINT16)*feature_data,
             (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
     #endif
     break;
     case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
         *feature_return_para_i32 = 0;
         *feature_para_len = 4;
         break;

     case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
     {
         switch (*feature_data) {
         case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.cap.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG\n");
             break;
         case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.normal_video.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_VIDEO_PREVIEW\n");
             break;
         case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.hs_video.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_HIGH_SPEED_VIDEO\n");
             break;
         case MSDK_SCENARIO_ID_SLIM_VIDEO:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.slim_video.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_SLIM_VIDEO\n");
             break;
         case MSDK_SCENARIO_ID_CUSTOM1:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom1.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_CUSTOM1\n");
             break;
         case MSDK_SCENARIO_ID_CUSTOM2:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom2.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_CUSTOM2\n");
             break;
         case MSDK_SCENARIO_ID_CUSTOM3:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom3.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_CUSTOM3\n");
             break;
        case MSDK_SCENARIO_ID_CUSTOM4:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.custom4.mipi_pixel_rate;
             LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATEMSDK_SCENARIO_ID_CUSTOM4\n");
             break;
         case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
         default:
             *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                 = imgsensor_info.pre.mipi_pixel_rate;
             break;
         }
     }
     break;
#if ENABLE_PDAF
         case SENSOR_FEATURE_GET_VC_INFO:
                 LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",(UINT16)*feature_data);
                 pvcinfo = (struct SENSOR_VC_INFO_STRUCT*)(uintptr_t)(*(feature_data+1));
                 switch (*feature_data_32)
                 {
                     case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO CAPTURE_JPEG\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO VIDEO PREVIEW\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[1],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     case MSDK_SCENARIO_ID_CUSTOM1:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO CUSTOM1\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     case MSDK_SCENARIO_ID_CUSTOM2:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO CUSTOM2\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     case MSDK_SCENARIO_ID_CUSTOM3:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO CUSTOM3\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     case MSDK_SCENARIO_ID_CUSTOM4:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO CUSTOM4\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO PREVIEW\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                     default:
                         LOG_INF("SENSOR_FEATURE_GET_VC_INFO DEFAULT_PREVIEW\n");
                         memcpy((void *)pvcinfo,(void*)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
                         break;
                 }
                 break;

     case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
         LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITYscenarioId:%lld\n", *feature_data);
         //PDAF capacity enable or not, 2p8 only full size support PDAF
         switch (*feature_data) {
             case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; //type2 - VC enable
                 break;
             case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
                 break;
             case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                 break;
             case MSDK_SCENARIO_ID_SLIM_VIDEO:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                 break;
             case MSDK_SCENARIO_ID_CUSTOM4:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                 break;
             case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                 break;
             case MSDK_SCENARIO_ID_CUSTOM1:
             //case MSDK_SCENARIO_ID_CUSTOM2:
             case MSDK_SCENARIO_ID_CUSTOM3:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; //type2 - VC enable
                 break;
             default:
                 *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                 break;
         }
         LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
         break;

        case SENSOR_FEATURE_GET_PDAF_DATA:
         LOG_INF(" GET_PDAF_DATA EEPROM\n");
         break;
         case SENSOR_FEATURE_GET_PDAF_INFO:
             PDAFinfo= (struct SET_PD_BLOCK_INFO_T*)(uintptr_t)(*(feature_data+1));
             switch( *feature_data)
             {
                 case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                 break;
                 case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                 case MSDK_SCENARIO_ID_CUSTOM1:
                 memcpy((void *)PDAFinfo, (void*)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
                 break;
                 case MSDK_SCENARIO_ID_CUSTOM2:
                 //memcpy((void *)PDAFinfo, (void*)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
                 break;
                 case MSDK_SCENARIO_ID_CUSTOM3:
                 memcpy((void *)PDAFinfo, (void*)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
                 break;
                 case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                 memcpy((void *)PDAFinfo, (void*)&imgsensor_4Kpd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
                 break;
                 case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                 case MSDK_SCENARIO_ID_SLIM_VIDEO:
                 case MSDK_SCENARIO_ID_CUSTOM4:
                 default:
                     break;
             }
         break;

     case SENSOR_FEATURE_SET_PDAF:
             imgsensor.pdaf_mode = *feature_data_16;
             LOG_INF(" pdaf mode : %d \n", imgsensor.pdaf_mode);
             break;
#endif
     case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
         streaming_control(KAL_FALSE);
         break;
     case SENSOR_FEATURE_SET_STREAMING_RESUME:
         if (*feature_data != 0)
             set_shutter(*feature_data);
         streaming_control(KAL_TRUE);
         break;
     case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
          *feature_return_para_32 = imgsensor.current_ae_effective_frame;
          break;

     default:
     break;
     }

     return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
     open,
     get_info,
     get_resolution,
     feature_control,
     control,
     close
};

UINT32 N28HI5021QREARTRULY_MIPI_RAW_SensorInit(struct
SENSOR_FUNCTION_STRUCT **pfFunc)
{
     /* To Do : Check Sensor status here */
     if (pfFunc != NULL)
         *pfFunc =  &sensor_func;
     return ERROR_NONE;
}    /*    hi5021q_MIPI_RAW_SensorInit    */
