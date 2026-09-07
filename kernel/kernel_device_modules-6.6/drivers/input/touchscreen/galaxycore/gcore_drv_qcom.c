/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "gcore_drv_common.h"

void gcore_suspend(void)
{
	//struct gcore_dev *gdev = fn_data.gdev;

	GTP_DEBUG("enter gcore suspend");

#ifdef	CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	if(gcore_tpd_proximity_flag && gcore_tpd_proximity_flag_off){
		fn_data.gdev->ts_stat = TS_SUSPEND;
		GTP_DEBUG("Proximity TP Now.");
		return ;
	}

#endif

#if GCORE_WDT_RECOVERY_ENABLE
	cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif
	
	cancel_delayed_work_sync(&fn_data.gdev->fwu_work);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if(fn_data.gdev->gesture_wakeup_en){
		enable_irq_wake(fn_data.gdev->touch_irq);
	}
#endif

#ifdef CONFIG_SAVE_CB_CHECK_VALUE
	fn_data.gdev->CB_ckstat = false;
#endif

	fn_data.gdev->ts_stat = TS_SUSPEND;

	gcore_touch_release_all_point(fn_data.gdev->input_device);
	
	GTP_DEBUG("gcore suspend end");

}

void gcore_resume(void)
{
	//struct gcore_dev *gdev = fn_data.gdev;

	GTP_DEBUG("enter gcore resume");
	
#ifdef	CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
			if(fn_data.gdev->PS_Enale == true){
				tpd_enable_ps(1);
			}
		
#endif


#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if(fn_data.gdev->gesture_wakeup_en){
		disable_irq_wake(fn_data.gdev->touch_irq);
	}
#endif
	
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
    if(fn_data.gdev->ts_stat == TS_MPTEST){
        GTP_DEBUG("ITO test is runing");
    	queue_delayed_work(fn_data.gdev->fwu_workqueue, &fn_data.gdev->fwu_work, \
    						msecs_to_jiffies(2000));
    }else{
        gcore_request_firmware_update_work(NULL);
    }
#else
#if CONFIG_GCORE_RESUME_EVENT_NOTIFY
	queue_delayed_work(fn_data.gdev->gtp_workqueue, &fn_data.gdev->resume_notify_work, msecs_to_jiffies(200));
#endif
	gcore_touch_release_all_point(fn_data.gdev->input_device);
#endif
	fn_data.gdev->ts_stat = TS_NORMAL;

	GTP_DEBUG("gcore resume end");
}

#if defined(CONFIG_DRM)
int gcore_ts_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	int *blank = (int *)data;

	if (!blank){
		GTP_DEBUG("Invalid blank");
		return 0;
	}

//	GTP_DEBUG("event=%lu, *blank=%d\n", event, *blank);

	if (event == MTK_DISP_EARLY_EVENT_BLANK) {
		if (*blank == MTK_DISP_BLANK_POWERDOWN) {
			GTP_DEBUG("suspend,event=%lu, *blank=%d\n", event, *blank);
			gcore_suspend();
		}
	} else if (event == MTK_DISP_EVENT_BLANK) {
		if (*blank == MTK_DISP_BLANK_UNBLANK) {
			GTP_DEBUG("resume,event=%lu, *blank=%d\n", event, *blank);
//			gcore_resume();
			queue_work(charger_notify_wq_gc,&gc_resume_work);
		}
	}
	return 0;
}

#elif defined(CONFIG_FB)
int gcore_ts_fb_notifier_callback(struct notifier_block *self, \
			unsigned long event, void *data)
{
	unsigned int blank;
	struct fb_event *evdata = data;

	if (!evdata)
		return 0;

	blank = *(int *)(evdata->data);
	GTP_DEBUG("event = %d, blank = %d", event, blank);

	if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
		GTP_DEBUG("event(%lu) do not need process\n", event);
		return 0;
	}

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		if (event == FB_EARLY_EVENT_BLANK) {
			gcore_suspend();
		}
		break;

	case FB_BLANK_UNBLANK:
		if (event == FB_EVENT_BLANK) {
			gcore_resume();
		}
		break;

	default:
		break;
	}
	return 0;

}
#endif

static int __init touch_driver_init(void)
{
	GTP_DEBUG("touch driver init.");

	if (gcore_touch_bus_init()) {
		GTP_ERROR("bus init fail!");
		return -EPERM;
	}

	return 0;
}

/* should never be called */
static void __exit touch_driver_exit(void)
{
	gcore_touch_bus_exit();
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);

#if GKI_CLOSE_CAN_SAVE_FILE
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
#endif

MODULE_AUTHOR("GalaxyCore, Inc.");
MODULE_DESCRIPTION("GalaxyCore Touch Main Mudule");
MODULE_LICENSE("GPL");
