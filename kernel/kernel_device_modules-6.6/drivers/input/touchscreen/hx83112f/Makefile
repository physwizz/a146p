# Makefile for the Himax touchscreen drivers.

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/


ccflags-y += -DHX_MTK_K510

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

# Generally, it is useless for this platform
# ifneq ($(filter y, $(CONFIG_DRM)),)
# 	ccflags-y += -DHX_CONFIG_DRM
# endif

obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET) += hxchipset.o
# obj-y += himax_modular_table.o

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	hxchipset-objs += himax_debug.o
endif


hxchipset-objs += himax_inspection.o
hxchipset-objs += himax_ic_incell_core.o
hxchipset-objs += himax_ic_HX83112.o
hxchipset-objs += himax_common.o
hxchipset-objs += himax_platform.o
hxchipset-objs += himax_firmware.o


