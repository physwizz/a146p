#ifndef _GC08A3MIPI_OTP_H
#define _GC08A3MIPI_OTP_H

#include "cam_cal_define.h"
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include <linux/slab.h>

/* otp enable */
#define GC08A3_OTP_FUNCTION 1

#define MODULE_INFO_SIZE 7
#define MODULE_INFO_FLAG 0x15A0
#define MODULE_GROUP1_ADDR  0x15A8
#define MODULE_GROUP2_ADDR  0x15E8
#define MODULE_GROUP3_ADDR  0x1628
#define SN_INFO_SIZE 12
#define SN_INFO_FLAG 0x1668
#define SN_GROUP1_ADDR  0x1670
#define SN_GROUP2_ADDR  0x16D8
#define SN_GROUP3_ADDR  0x1740
#define AWB_INFO_SIZE 16
#define AWB_INFO_FLAG 0x17A8
#define AWB_GROUP1_FLAG  0x17B0
#define AWB_GROUP2_FLAG  0x1838
#define AWB_GROUP3_FLAG  0x18C0
#define LSC_INFO_SIZE 1868
#define LSC_INFO_FLAG 0x1948
#define LSC_GROUP1_FLAG  0x1950
#define LSC_GROUP2_FLAG  0x53B8
#define LSC_GROUP3_FLAG  0x8E20

static struct stCAM_CAL_DATAINFO_STRUCT n28gc0a83frontcxt_eeprom_data ={
    .sensorID= N28GC08A3FRONTCXT_SENSOR_ID,
    .deviceID = 0x02,
    .dataLength = 0x076F,//1903
    .sensorVendorid = 0x16050000,
    .vendorByte = {1,2,3,4},
    .dataBuffer = NULL,
};

extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
extern void write_cmos_sensor_8bit(kal_uint32 addr, kal_uint32 para);
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
int gc0a83_sensor_otp_info(void);
kal_uint16 read_cmos_gc08a3_sensor(kal_uint32 addr);
void write_cmos_gc08a3_sensor(kal_uint32 addr, kal_uint32 para);
#endif