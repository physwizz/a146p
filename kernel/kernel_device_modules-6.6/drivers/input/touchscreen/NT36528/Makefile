#
# Makefile for the Novatek NT36xxx touchscreen driver.
#
#+S96818AA1-1936,daijun1.wt,modify,2023/07/31,Modify TP nt36528 blocking shutdown issue
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/misc
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/charger
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
subdir-ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
#-S96818AA1-1936,daijun1.wt,modify,2023/07/31,Modify TP nt36528 blocking shutdown issue
# Each configuration option enables a list of files.
#ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
#	obj-$(CONFIG_TOUCHSCREEN_NT36528_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_sec_fn.o
#else
#	obj-$(CONFIG_TOUCHSCREEN_NT36528_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_sec_fn.o nvt_ts_only_vendor.o
#endif

obj-$(CONFIG_TOUCHSCREEN_NT36528_HOSTDL_SPI) += nt36528_tp.o
nt36528_tp-y := nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o nt36xxx_sec_fn.o
#obj-$(CONFIG_TOUCHSCREEN_NT36528_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o nt36xxx_sec_fn.o
