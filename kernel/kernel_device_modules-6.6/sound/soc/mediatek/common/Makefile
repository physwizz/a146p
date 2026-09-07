# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif
DSP_VERSION := hifi3_v2

# adsp include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv

# common driver
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MEDIATEK) += snd-soc-mtk-common.o
snd-soc-mtk-common-objs += mtk-afe-platform-driver.o mtk-afe-fe-dai.o mtk-mmap-ion.o mtk-usb-offload-ops.o
ifeq ($(CONFIG_MTK_USIP_DRIVER), m)
    snd-soc-mtk-common-m += mtk-usip.o
else
    snd-soc-mtk-common-$(CONFIG_MTK_ECCCI_DRIVER) += mtk-usip.o
endif
snd-soc-mtk-common-$(CONFIG_SND_SOC_MTK_SRAM) += mtk-sram-manager.o
snd-soc-mtk-common-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += mtk-sp-pcm-ops.o mtk-afe-debug.o

# functions for external modules
obj-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += mtk-afe-external.o

# speaker dynamic detect
obj-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += mtk-sp-spk-amp.o

# BT CVSD driver
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MTK_BTCVSD) += mtk-btcvsd.o

#+S96901AA5-563, wangdawen.wt, add, 2024/08/14, external spk amp
ifeq ($(CONFIG_SND_SOC_FS1815N),m)
subdir-ccflags-m += -I$(srctree)/sound/soc/codecs/fs1815/
endif
#-S96901AA5-563, wangdawen.wt, add, 2024/08/14, external spk amp
