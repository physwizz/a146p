# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.
IMGSENSOR_SUBDRV_OBJ := \
		$(subdrv-rpath)/n28_hi846_front_truly_mipiraw_Sensor.o \
		$(subdrv-rpath)/n28_hi846_front_truly_mipiraw_otp.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(IMGSENSOR_SUBDRV_OBJ)
endif
