# Makefile for the focaltech touchscreen drivers.

# Makefile for the focaltech touchscreen drivers.


ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-$(CONFIG_TOUCHSCREEN_FTS_FT8722) += focaltech_tp.o
#obj-m += focaltech_tp.o
focaltech_tp-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_proximity.o

focaltech_tp-y += focaltech_flash_spi.o


focaltech_tp-y += focaltech_spi_v2.o

focaltech_tp-y += focaltech_debug.o
focaltech_tp-y += focaltech_hp.o
focaltech_tp-y += focaltech_test/focaltech_test.o
focaltech_tp-y += focaltech_test/focaltech_test_ini.o
focaltech_tp-y += focaltech_test/supported_ic/focaltech_test_ft8722.o
