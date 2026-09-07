#ifndef _HI846MIPI_OTP_H
#define _HI846MIPI_OTP_H

#include "cam_cal_define.h"
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include <linux/slab.h>

#define Hi846_OTP_FUNCTION 1

#define MODULE_INFO_SIZE 7
#define AWB_DATA_SIZE 16
#define LSC_DATA_SIZE 1868
//#define SN_INFO_SIZE 12

#define MODULE_GROUP_FLAG 0x201
#define AWB_GROUP_FLAG 0x242
#define LSC_GROUP_FLAG 0x276
#define SN_INFO_FLAG 0x21A

#define VALID_OTP_GROUP1_FLAG 0x01
#define VALID_OTP_GROUP2_FLAG 0x13
#define VALID_OTP_GROUP3_FLAG 0x37

static struct stCAM_CAL_DATAINFO_STRUCT n28hi846fronttruly_eeprom_data = {
	.sensorID= N28HI846FRONTTRULY_SENSOR_ID,
	.deviceID = 0x02,
	.dataLength = 0x0763,//1912
	.sensorVendorid = 0x060500ff,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};
extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
int hi846_sensor_otp_info(void);
kal_uint16 read_cmos_hi846_sensor(kal_uint32 addr);
void write_cmos_hi846_sensor(kal_uint32 addr, kal_uint32 para);
void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para);
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
#endif