# In case the platform does NOT support this type of sensors

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include

obj-$(CONFIG_MTK_SENSOR_SUPPORT)	+= situation.o
obj-y += situation_hub/
obj-$(CONFIG_SMART_ALERT_HUB)     += smart_alert/
obj-$(CONFIG_LIFT_TO_WAKE)     += lift_to_wake/
