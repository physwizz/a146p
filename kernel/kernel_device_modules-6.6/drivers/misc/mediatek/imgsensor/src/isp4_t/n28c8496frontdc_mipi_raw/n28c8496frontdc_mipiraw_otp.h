#ifndef _C8496MIPI_OTP_H
#define _C8496MIPI_OTP_H

#include "cam_cal_define.h"
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include <linux/slab.h>

/* otp enable */
#define C8496_OTP_FUNCTION 1

#define MODULE_INFO_SIZE 7
#define MODULE_INFO_FLAG 0x4800
#define MODULE_GROUP1_ADDR  0x4801
#define MODULE_GROUP2_ADDR  0x4809
#define MODULE_GROUP3_ADDR  0x4811
#define SN_INFO_SIZE 12
#define SN_INFO_FLAG 0x4819
#define SN_GROUP1_ADDR  0x481A
#define SN_GROUP2_ADDR  0x4827
#define SN_GROUP3_ADDR  0x4834
#define AWB_INFO_SIZE 16
#define AWB_INFO_FLAG 0x4841
#define AWB_GROUP1_FLAG  0x4842
#define AWB_GROUP2_FLAG  0x4853
#define AWB_GROUP3_FLAG  0x4864
#define LSC_INFO_SIZE 1868
#define LSC_INFO_FLAG 0x4875
#define LSC_GROUP1_FLAG  0x4876
#define LSC_GROUP2_FLAG  0x4FC3
#define LSC_GROUP3_FLAG  0x5710

static struct stCAM_CAL_DATAINFO_STRUCT n28c8496frontdc_eeprom_data ={
    .sensorID= N28C8496FRONTDC_SENSOR_ID,
    .deviceID = 0x02,
    .dataLength = 0x076F,//1912
    .sensorVendorid = 0x13050001,
    .vendorByte = {1,2,3,4},
    .dataBuffer = NULL,
};

extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
int c8496_sensor_otp_info(void);
#endif