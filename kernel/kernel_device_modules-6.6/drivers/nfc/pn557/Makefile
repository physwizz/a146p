#
# Makefile for PN544 HCI based NFC driver
#
#obj-$(CONFIG_NFC_PN557)            += nq-nci.o
#obj-y        +=nq-nci.o
ccflags-y :=
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/nfc/pn557/
obj-$(CONFIG_NXP_NFC_PN8XT) += nfc_i2c.o
nfc_i2c-y := common.o \
             common_ese.o \
             i2c_drv.o
#ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG

ifneq ($(CONFIG_NXP_NFC_PN8XT),)
ccflags-y += -DDYNAMIC_DEBUG_MODULE
ccflags-y += -DNFC_PLATFORM=pn8xt
endif

#ccflags-$(CONFIG_NFC_DEBUG) += -DDEBUG
#ccflags-y := -DDEBUG
#ccflags-$(CONFIG_NXP_NFC_SN1XX)  := -DNFC_PLATFORM=sn1xx
#ccflags-$(CONFIG_NXP_NFC_PN8XT)  := -DNFC_PLATFORM=pn8xt
