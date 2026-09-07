#ifndef __WING_CAM_CAL__
#define __WING_CAM_CAL__
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Wintech Inc.
 */
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "kd_camera_feature.h"

#define NONE_SENSOR_NUM (0xFFFF)
#define MAX_EEPROM_ARRAY_DATA_SZIE 5
void set_eeprom_read_device_id_bywing(u32 sensorId, u32 deviceId);
unsigned int Common_read_region_bywing(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size);
#endif