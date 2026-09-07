#ifndef _SC800CSAMIPI_OTP_H
#define _SC800CSAMIPI_OTP_H

#include "cam_cal_define.h"
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

/* otp enable */
#define SC800CSA_OTP_FUNCTION 1

#define SC800CSA_ST_RET_FAIL 1
#define SC800CSA_ST_RET_SUCCESS 0
#define SC800CSA_ST_PAGE2 2
#define SC800CSA_ST_DEBUG_ON 0

/* OTP Map Info */
/* Module Cal */
#define MODULE_INFO_FLAG        0x827a
#define MODULE_GROUP1_ADDR      0x827b
#define MODULE_GROUP2_ADDR      0x8283
/* SN Cal */
#define SN_INFO_FLAG            0x828b
#define SN_GROUP1_ADDR          0x828c
#define SN_GROUP2_ADDR          0x8299
/* AWB Cal */
#define AWB_INFO_FLAG           0x82a6
#define AWB_GROUP1_FLAG         0x82a7
#define AWB_GROUP2_FLAG         0x82b8
/* LSC Cal */  /* group1: 2,3,4,5,6 page; group2: 6,7,8,9,10,11 page;*/
#define LSC_INFO_FLAG           0x82c9
#define LSC_GROUP1_CHECKSUM     0x8BFE
#define LSC_GROUP2_CHECKSUM     0x95AD
#define LSC_GROUP2_FIRST        0X8BFF


/* OTP data info */
#define MODULE_INFO_SIZE  7
#define SN_INFO_SIZE      12
#define AWB_INFO_SIZE     16
#define LSC_INFO_SIZE     1868

#define MODULE_OFFSET     (0 + OTP_MAP_VENDOR_HEAD_ID_BYTES)
#define SN_OFFSET         (MODULE_OFFSET + MODULE_INFO_SIZE)
#define AWB_OFFSET        (SN_OFFSET + SN_INFO_SIZE)
#define LSC_OFFSET        (AWB_OFFSET + AWB_INFO_SIZE)
static struct stCAM_CAL_DATAINFO_STRUCT n28sc800csafrontdc_eeprom_data ={
	.sensorID= N28SC800CSAFRONTDC_SENSOR_ID,
	.deviceID = 0x02,
	.dataLength = 0x076F, //1912
	.sensorVendorid = 0x13050002,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};

extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
int sc800csa_sensor_otp_info(void);
#endif