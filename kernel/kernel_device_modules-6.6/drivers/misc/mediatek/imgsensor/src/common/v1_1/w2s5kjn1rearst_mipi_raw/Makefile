#obj-y += w2s5kjn1rearst_mipiraw_Sensor.o
IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/w2s5kjn1rearst_mipiraw_Sensor.o \
	$(subdrv-rpath)/w2s5kjn1rearst_mipiraw_otp.o \

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MON), m y))
imgsensor_isp6s_mon-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MOU), m y))
imgsensor_isp6s_mou-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif