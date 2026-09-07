// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#if defined(__CONFIG_WING_CAM_CAL__)
extern unsigned int Common_read_region_bywing(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size);
#endif

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	//+S96818AA3-866, chenjiaoyan.wt, ADD, 2024/08/28, main sensor OTP bringup
	#if defined(__CONFIG_WING_CAM_CAL__)
	{N28HI5021QREARTRULY_SENSOR_ID, 0xA0, Common_read_region_bywing, 8525},
	{N28HI5021QREARDC_SENSOR_ID, 0xA0, Common_read_region_bywing, 8525},
	{N28S5KJN1REARTRULY_SENSOR_ID, 0xA0, Common_read_region_bywing, 12679},
	{N28S5KJN1REARDC_SENSOR_ID, 0xA0, Common_read_region_bywing, 12679},
	{N28HI846FRONTTRULY_SENSOR_ID, 0x40, Common_read_region_bywing},
	{N28SC800CSFRONTDC_SENSOR_ID, 0xA0, Common_read_region_bywing},
	{N28C8496FRONTDC_SENSOR_ID, 0x20, Common_read_region_bywing},
	{N28SC800CSAFRONTDC_SENSOR_ID, 0x20, Common_read_region_bywing},
	{N28GC08A3FRONTCXT_SENSOR_ID, 0x62, Common_read_region_bywing},
	#endif
	//-S96818AA3-866, chenjiaoyan.wt, ADD, 2024/08/28, main sensor OTP bringup
#if 0
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B. No Cal data for main2 OV8856*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
#endif
#ifdef SUPPORT_S5K4H7
	{S5K4H7_SENSOR_ID, 0xA0, zte_s5k4h7_read_region},
	{S5K4H7SUB_SENSOR_ID, 0xA0, zte_s5k4h7_sub_read_region},
#endif
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


