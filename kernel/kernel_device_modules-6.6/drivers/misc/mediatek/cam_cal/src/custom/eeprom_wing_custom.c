/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Wintech Inc.
 */
#include "kd_imgsensor.h"

#include "cam_cal_define.h"
#include "eeprom_wing_custom.h"
#include "eeprom_i2c_common_driver.h"
#define __EEPROM_OTP_DUMP_BYWING__ 0

#undef PFX
#define PFX "WING_CAM_CAL"
#define LOG_INF(format, args...)    pr_info(PFX "[%s][%d] " format, __FUNCTION__, __LINE__, ##args)
#define LOG_ERR(format, args...)    pr_err(PFX "[%s][%d] " format, __FUNCTION__, __LINE__, ##args)

static struct stCAM_CAL_DATAINFO_STRUCT *g_eepromDataArray[MAX_EEPROM_ARRAY_DATA_SZIE] = {NULL, NULL, NULL, NULL, NULL,};
static struct stCAM_CAL_CHECKSUM_STRUCT *g_eepromDataChecksumArray[MAX_EEPROM_ARRAY_DATA_SZIE] = {NULL, NULL, NULL, NULL, NULL,};
static unsigned int g_eepromCurrentDeviceID = DUAL_CAMERA_SENSOR_MAX;
#define __MY_MEMCPY__ 0

#if __EEPROM_OTP_DUMP_BYWING__
void dump_eeprom_data_bywing(int u4Length,u8* pu1Params)
{
	int i = 0;
	for (i = 0; i < u4Length; i += 16) {
		if (u4Length - i  >= 16) {
			LOG_INF("cam_eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
			i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
			,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
			,pu1Params[i+15]);
		} else {
			int j = i;
			for (; j < u4Length; j++)
				LOG_INF("cam_eeprom[%d] = 0x%2x ",j,pu1Params[j]);
		}
	}
	LOG_INF("cam_eeprom end\n");
}
#endif


inline void * my_memcpy(void * dest, const void* src, int n)
{
	const char *s = (const char *)(src) + (n) - 1;
	char *d = (char *)(dest);
	while (n--) {
		*d++ = *s--;
	}
	return dest;
}
EXPORT_SYMBOL (my_memcpy);

inline u32  get_device_id_form_sensorid_bywing(u32 sensor_id)
{
	switch (sensor_id) {
	case W2S5KJN1REARTRULY_SENSOR_ID:
	case W2S5KJN1REARST_SENSOR_ID:
	case W2HI5022QREARTXD_SENSOR_ID:
		return DUAL_CAMERA_MAIN_SENSOR;
	case W2HI1336FRONTTRULY_SENSOR_ID:
	case W2SC1300MCSFRONTTXD_SENSOR_ID:
	case W2SC1300MCSFRONTST_SENSOR_ID:
		return DUAL_CAMERA_SUB_SENSOR;
	case W2GC02M1MICROCXT_SENSOR_ID:
	case W2BF2253LMICROSJ_SENSOR_ID:
	case W2SC202CSMICROLH_SENSOR_ID:
		return DUAL_CAMERA_MAIN_3_SENSOR;
	default:
		return NONE_SENSOR_NUM;
	}
}

void set_eeprom_read_device_id_bywing(u32 sensorId, u32 deviceId)
{
	u32 tempDeviceID = NONE_SENSOR_NUM;

	if (NONE_SENSOR_NUM != deviceId) {
		g_eepromCurrentDeviceID = deviceId;
		LOG_INF("set g_eepromCurrentDeviceID is %d",g_eepromCurrentDeviceID);
		return;
	}

	if (NONE_SENSOR_NUM == sensorId) {
		LOG_INF("set deviceId error");
		return;		
	}
	LOG_INF("current sensorId is 0x%x",sensorId);

	tempDeviceID = get_device_id_form_sensorid_bywing(sensorId);

	if (NONE_SENSOR_NUM != tempDeviceID) {
		g_eepromCurrentDeviceID = tempDeviceID;
		LOG_INF("set g_eepromCurrentDeviceID is %d",g_eepromCurrentDeviceID);
		return;		
	}
}

int eeprom_do_checksum(u8* kbuffer, struct stCAM_CAL_CHECKSUM_STRUCT* cData) {
	int i = 0;
	int length = 0;
	int count;
	u32 sum = 0;

	if ((kbuffer != NULL) && (cData != NULL)) {
		u8* buffer = kbuffer;
		//verity validflag and checksum
		for ((count = 0); count < MAX_ITEM; count++) {
			if (cData[count].item < MAX_ITEM) {
				if(buffer[cData[count].flagAdrees]!= cData[count].validFlag){
					LOG_ERR("invalid otp data cItem=%d,flag=%d failed\n", cData[count].item,buffer[cData[count].flagAdrees]);
					return -ENODEV;
				} else {
					LOG_INF("check cTtem=%d,flag=%d otp flag data successful!\n", cData[count].item,buffer[cData[count].flagAdrees]);
				}
				sum = 0;
				length = cData[count].endAdress - cData[count].startAdress;
				for (i = 0; i <= length; i++) {
					sum += buffer[cData[count].startAdress+i];
				}
				if (((sum%0xff)+1) != buffer[cData[count].checksumAdress]) {
					LOG_ERR("checksum cItem=%d,0x%x,length = 0x%x failed\n",cData[count].item,sum,length);
					return -ENODEV;
				} else {
					LOG_INF("checksum cItem=%d,0x%x,length = 0x%x successful!\n",cData[count].item,sum,length);
				}
			} else {
				break;
			}
		}
	} else {
		LOG_ERR("some data not inited!\n");
		return -ENODEV;
	}

	return 0;
}

EXPORT_SYMBOL (eeprom_do_checksum);

bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData)
{
	int camIndex = MAX_EEPROM_ARRAY_DATA_SZIE;
	if (NULL == pData)
		return false;
	camIndex = IMGSENSOR_SENSOR_DUAL2IDX(pData->deviceID);
	if (camIndex >= MAX_EEPROM_ARRAY_DATA_SZIE) {
		LOG_ERR("camIndex = %d more than custom size(%d)",camIndex,MAX_EEPROM_ARRAY_DATA_SZIE);
		return false;
	}
	if ((NULL != g_eepromDataArray[camIndex]) || (NULL != g_eepromDataChecksumArray[camIndex])) {
		LOG_ERR("maybe some error camIndex%d,data(%p),checksum(%p)",camIndex,g_eepromDataArray[camIndex],g_eepromDataChecksumArray[camIndex]);
		//return false;
	}
	LOG_INF("set camIndex is %d",camIndex);
	g_eepromDataArray[camIndex] = pData;
	g_eepromDataChecksumArray[camIndex] = checksumData;
	return true;
}
EXPORT_SYMBOL (imgsensor_set_eeprom_data_bywing);


unsigned int Common_read_region_real_bywing(struct i2c_client *client, struct stCAM_CAL_DATAINFO_STRUCT * eepromDataTemp, 
	struct stCAM_CAL_CHECKSUM_STRUCT * eepromChecksumDataTemp)
{
	u8 *kbuf = NULL;
	unsigned int ret = 0;

	if (NULL == eepromDataTemp) {
		LOG_ERR("eeprom configure error,deviceid(0x%x)",g_eepromCurrentDeviceID);
		return 0;
	}

	kbuf = kmalloc(eepromDataTemp->dataLength, GFP_KERNEL);

	if (NULL == kbuf) {
		LOG_ERR("kmalloc memory failed,deviceid(0x%x)",g_eepromCurrentDeviceID);
		return 0;
	}
	
	ret = Common_read_region(client, 0, kbuf, eepromDataTemp->dataLength);

	if (ret != eepromDataTemp->dataLength) {
		LOG_ERR("ret (%d) is not equal read size %d\n", ret, eepromDataTemp->dataLength);
		kfree(kbuf);
		return ret;
	}
	#if __EEPROM_OTP_DUMP_BYWING__
	dump_eeprom_data_bywing(eepromDataTemp->dataLength, kbuf);
	#endif

	if (NULL == eepromDataTemp->dataBuffer) {
		if (NULL == eepromChecksumDataTemp) {
			LOG_ERR("please check where do checksum");
		}
		else if (eeprom_do_checksum(kbuf,eepromChecksumDataTemp) != 0) {
			LOG_ERR("checksum failed");
			kfree(kbuf);
			return 0;
		}

		eepromDataTemp->dataBuffer = kmalloc(eepromDataTemp->dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES, GFP_KERNEL);

		if (NULL == eepromDataTemp->dataBuffer) {
			LOG_ERR("kmalloc memory failed,deviceid(0x%x)",g_eepromCurrentDeviceID);
			kfree(kbuf);
			return 0;
		}
		#if __MY_MEMCPY__
		my_memcpy(eepromDataTemp->dataBuffer, &eepromDataTemp->sensorVendorid, OTP_MAP_VENDOR_HEAD_ID_BYTES);
		#else
		memcpy((void *) eepromDataTemp->dataBuffer, (void *) &eepromDataTemp->sensorVendorid, OTP_MAP_VENDOR_HEAD_ID_BYTES);
		#endif
		memcpy((void *) eepromDataTemp->dataBuffer + OTP_MAP_VENDOR_HEAD_ID_BYTES , (void *) kbuf , eepromDataTemp->dataLength);
		ret =  eepromDataTemp->dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES;
		LOG_INF("deviceid(0x%x),read size %d success",g_eepromCurrentDeviceID,ret);
	}

	if (NULL != kbuf)
		kfree(kbuf);
	return ret;
}

unsigned int Common_read_region_bywing(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	unsigned int ret = 0;
	u32 totalLength = 0;
	int camIndex = IMGSENSOR_SENSOR_DUAL2IDX(g_eepromCurrentDeviceID);
	struct stCAM_CAL_DATAINFO_STRUCT * eepromDataTemp = NULL;
	struct stCAM_CAL_CHECKSUM_STRUCT * eepromChecksumDataTemp = NULL;

	if (camIndex >= MAX_EEPROM_ARRAY_DATA_SZIE) {
		LOG_ERR("please check the current deviceID 0x%x setting",g_eepromCurrentDeviceID);
		return 0;
	}
	
	eepromDataTemp = g_eepromDataArray[camIndex];
	eepromChecksumDataTemp = g_eepromDataChecksumArray[camIndex];

	if (NULL == eepromDataTemp) {
		LOG_ERR("please check the eeprom data setting");
		return 0;
	}

	if (NULL == eepromChecksumDataTemp) {
		LOG_INF("please check the eeprom checksum data setting");
	}

	if (NULL == eepromDataTemp->dataBuffer) {//check id
		Common_read_region_real_bywing(client, eepromDataTemp, eepromChecksumDataTemp);
	} 

	totalLength = addr + size;

	if ((NULL != eepromDataTemp->dataBuffer) && (totalLength <= (eepromDataTemp->dataLength + OTP_MAP_VENDOR_HEAD_ID_BYTES))) {
		memcpy((void *) data,(void *) (eepromDataTemp->dataBuffer + addr), size);
		ret = size;
		LOG_INF("g_eepromCurrentDeviceID= 0x%x ,offset=%d, length=%d\n", g_eepromCurrentDeviceID, addr , size);
		return ret;
	}
	return ret;
}

//EXPORT_SYMBOL (Common_read_region_bywing);
