// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/cdev.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/suspend.h>
#include <net/sock.h>
#include <linux/rtc.h>

#include "mtk_battery.h"
#include "mtk_gauge.h"
#include "mtk_charger.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

//#define BM_NO_SLEEP
//#define BM_USE_ALARM_TIMER
#define BM_USE_HRTIMER

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
struct wt_charging_type wt_ta_type[] = {
	{SEC_BATTERY_CABLE_UNKNOWN, "UNDEFINED"},
	{SEC_BATTERY_CABLE_NONE, "NONE"},
	{SEC_BATTERY_CABLE_TA, "TA"},
	{SEC_BATTERY_CABLE_USB, "USB"},
	{SEC_BATTERY_CABLE_USB_CDP, "USB_CDP"},
	{SEC_BATTERY_CABLE_9V_TA, "9V_TA"},
	{SEC_BATTERY_CABLE_PDIC, "9V_TA"},
	{SEC_BATTERY_CABLE_PDIC_APDO, "PDIC_APDO"},
};

int batt_temp_value = 0;
int batt_vol_value = 0;
char str_batt_type[64] = {0};
extern int batt_slate_mode;
extern int temp_cycle;
extern void set_batt_slate_mode(int *en);
extern int charger_manager_disable_charging_new(struct mtk_charger *info,
	bool en);
extern void pd_dpm_send_source_caps_0a(bool val);
#endif

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct mtk_battery_manager *get_mtk_battery_manager(void)
{
	static struct mtk_battery_manager *bm;
	struct power_supply *psy;

	if (bm == NULL) {
		psy = power_supply_get_by_name("battery");
		if (psy == NULL) {
			pr_err("[%s]psy is not rdy\n", __func__);
			return NULL;
		}
		bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);
		power_supply_put(psy);
		if (bm == NULL) {
			pr_err("[%s]mtk_battery_manager is not rdy\n", __func__);
			return NULL;
		}
	}

	return bm;
}

/* ============================================================ */
/* power supply: battery */
/* ============================================================ */
int check_cap_level(int uisoc)
{
	if (uisoc >= 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc >= 80 && uisoc < 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc >= 20 && uisoc < 80)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > 0 && uisoc < 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (uisoc == 0)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
}

int next_waketime(int polling)
{
	if (polling <= 0)
		return 0;
	else
		return 10;
}

static int shutdown_event_handler(struct mtk_battery *gm)
{
	ktime_t now, duraction;
	struct timespec64 tmp_duraction;
	int polling = 0;
	int now_current = 0;
	int current_ui_soc = gm->ui_soc;
	int current_soc = gm->soc;
	int vbat = 0;
	int tmp = 25;
	bool is_single;
	//struct shutdown_controller *sdd = &gm->sdc;
	//struct shutdown_controller *sdc = &gm->bm->sdc;
	struct battery_shutdown_unit *sdu = &gm->bm->sdc.bat[gm->id];

	tmp_duraction.tv_sec = 0;
	tmp_duraction.tv_nsec = 0;

	now = ktime_get_boottime();

	pr_debug("%s, %s:soc_zero:%d,ui 1percent:%d,dlpt_shut:%d,under_shutdown_volt:%d\n",
		gm->gauge->name, __func__,
		sdu->shutdown_status.is_soc_zero_percent,
		sdu->shutdown_status.is_uisoc_one_percent,
		sdu->shutdown_status.is_dlpt_shutdown,
		sdu->shutdown_status.is_under_shutdown_voltage);

	if (gm->bm->gm_no == 1)
		is_single = true;
	else
		is_single = false;

	if (sdu->shutdown_status.is_soc_zero_percent) {
		if (current_ui_soc == 0) {
			duraction = ktime_sub(
				now, sdu->pre_time[SOC_ZERO_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			polling++;
			if (is_single && tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_debug("soc zero notify zero percent after %d\n", (int)tmp_duraction.tv_sec);
				return next_waketime(polling);
			}
		} else if (current_soc > 0) {
			sdu->shutdown_status.is_soc_zero_percent = false;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
	}

	if (sdu->shutdown_status.is_uisoc_one_percent) {
		now_current = gauge_get_int_property(gm,
			GAUGE_PROP_BATTERY_CURRENT);

		if (current_ui_soc == 0) {
			duraction =
				ktime_sub(
				now, sdu->pre_time[UISOC_ONE_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			if (is_single && tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_debug("uisoc one notify zero percent after %d\n", (int)tmp_duraction.tv_sec);
				return next_waketime(polling);
			}
		} else if (now_current > 0 && current_soc > 0) {
			sdu->shutdown_status.is_uisoc_one_percent = 0;
			pr_err("disable uisoc_one_percent shutdown cur:%d soc:%d\n",
				now_current, current_soc);
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}

	}

	if (sdu->shutdown_status.is_dlpt_shutdown) {
		duraction = ktime_sub(now, sdu->pre_time[DLPT_SHUTDOWN]);
		tmp_duraction = ktime_to_timespec64(duraction);
		polling++;
		if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
			pr_err("dlpt shutdown count, %d\n",
				(int)tmp_duraction.tv_sec);
			return next_waketime(polling);
		}
	}

	if (sdu->shutdown_status.is_under_shutdown_voltage) {

		int vbatcnt = 0, i;

		if (gm->disableGM30)
			vbat = 4000;
		else
			vbat = bm_get_vsys(gm->bm);

		sdu->batdata[sdu->batidx] = vbat;

		for (i = 0; i < gm->avgvbat_array_size; i++)
			vbatcnt += sdu->batdata[i];
		sdu->avgvbat = vbatcnt / gm->avgvbat_array_size;
		tmp = battery_get_int_property(gm, BAT_PROP_TEMPERATURE);

		pr_debug("%s, lbatcheck vbat:%d avgvbat:%d %d,%d tmp:%d,bound:%d,th:%d %d,en:%d\n",
			gm->gauge->name, vbat,
			sdu->avgvbat,
			sdu->vbat_lt,
			sdu->vbat_lt_lv1,
			tmp,
			gm->bat_voltage_low_bound,
			LOW_TEMP_THRESHOLD,
			gm->low_tmp_bat_voltage_low_bound,
			LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN);

		if (sdu->avgvbat < gm->bat_voltage_low_bound) {
			/* avg vbat less than 3.4v */
			sdu->lowbatteryshutdown = true;
			polling++;

			if (sdu->down_to_low_bat == 0) {
				if (IS_ENABLED(
					LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN)) {
					if (tmp >= LOW_TEMP_THRESHOLD) {
						sdu->down_to_low_bat = 1;
						pr_err("%s, normal tmp, battery voltage is low shutdown\n",
							gm->gauge->name);
						battery_set_property(gm,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else if (sdu->avgvbat <=
						gm->low_tmp_bat_voltage_low_bound) {
						sdu->down_to_low_bat = 1;
						pr_err("%s, cold tmp, battery voltage is low shutdown\n",
							gm->gauge->name);
						battery_set_property(gm,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else
						pr_err("%s, low temp disable low battery sd\n",
							gm->gauge->name);
				} else {
					sdu->down_to_low_bat = 1;
					pr_err("%s, [%s]avg vbat is low to shutdown\n",
						gm->gauge->name, __func__);
					battery_set_property(gm,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
				}
			}

			if ((current_ui_soc == 0) && (sdu->ui_zero_time_flag == 0)) {
				sdu->pre_time[LOW_BAT_VOLT] =
					ktime_get_boottime();
				sdu->ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				duraction = ktime_sub(
					now, sdu->pre_time[LOW_BAT_VOLT]);

				tmp_duraction  = ktime_to_timespec64(duraction);
				if (is_single && tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
					pr_debug("low bat zero percent, over %d second\n",
						SHUTDOWN_TIME);
					return next_waketime(polling);
				}
			}
		} else {
			/* greater than 3.4v, clear status */
			sdu->down_to_low_bat = 0;
			sdu->ui_zero_time_flag = 0;
			sdu->pre_time[LOW_BAT_VOLT] = 0;
			sdu->lowbatteryshutdown = false;
			polling++;
		}

		polling++;
		pr_debug("%s, [%s][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d %d\n",
			gm->gauge->name, __func__,
			sdu->avgvbat, current_ui_soc,
			(int)tmp_duraction.tv_sec,
			sdu->down_to_low_bat, sdu->ui_zero_time_flag,
			(int)sdu->pre_time[LOW_BAT_VOLT],
			sdu->lowbatteryshutdown,
			sdu->batidx, sdu->batdata[sdu->batidx], gm->avgvbat_array_size);

		sdu->batidx++;
		if (sdu->batidx >= gm->avgvbat_array_size)
			sdu->batidx = 0;
	}

	pr_debug(
		"%s, %s %d avgvbat:%d sec:%d lowst:%d\n",
		gm->gauge->name, __func__,
		polling, sdu->avgvbat,
		(int)tmp_duraction.tv_sec, sdu->lowbatteryshutdown);

	return polling;
}

static int bm_shutdown_event_handler(struct mtk_battery_manager *bm)
{
	ktime_t now, duraction;
	struct timespec64 tmp_duraction;
	int polling = 0;
	int now_current1 = 0, now_current2 = 0;
	int current_ui_soc = bm->uisoc;
	int current_gm1_soc = bm->gm1->soc;
	int current_gm2_soc = bm->gm2->soc;
	int vbat1 = 0, vbat2 = 0, chg_vbat = 0;
	int tmp = 25;
	struct battery_shutdown_unit *sdu = &bm->sdc.bmsdu;
	struct battery_shutdown_unit *sdu1 = &bm->sdc.bat[bm->gm1->id];
	struct battery_shutdown_unit *sdu2 = &bm->sdc.bat[bm->gm2->id];

	tmp_duraction.tv_sec = 0;
	tmp_duraction.tv_nsec = 0;

	now = ktime_get_boottime();

	pr_debug("%s,BM:soc_zero:%d,ui 1percent:%d,dlpt_shut:%d,under_shutdown_volt:%d\n",
		__func__,
		sdu->shutdown_status.is_soc_zero_percent,
		sdu->shutdown_status.is_uisoc_one_percent,
		sdu->shutdown_status.is_dlpt_shutdown,
		sdu->shutdown_status.is_under_shutdown_voltage);

	if (sdu->shutdown_status.is_soc_zero_percent) {
		vbat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE);
		vbat2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_BATTERY_VOLTAGE);

		if (current_ui_soc == 0) {
			if (sdu->pre_time[SOC_ZERO_PERCENT] == 0)
				sdu->pre_time[SOC_ZERO_PERCENT] = now;

			duraction = ktime_sub(
				now, sdu->pre_time[SOC_ZERO_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			polling++;
			if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_debug("soc zero notify zero percent after %d\n", (int)tmp_duraction.tv_sec);
				return polling;
			}
		} else if (current_gm1_soc > 0 && current_gm2_soc > 0) {
			sdu->shutdown_status.is_soc_zero_percent = false;
			sdu->pre_time[SOC_ZERO_PERCENT] = 0;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
		pr_debug("%s, !!! SOC_ZERO_PERCENT, vbat1:%d, vbat2:%d, current_gm1_soc:%d, current_gm2_soc:%d\n",
			__func__, vbat1, vbat2, current_gm1_soc, current_gm2_soc);
	}

	if (sdu->shutdown_status.is_uisoc_one_percent) {
		now_current1 = gauge_get_int_property(bm->gm1,
			GAUGE_PROP_BATTERY_CURRENT);
		now_current2 = gauge_get_int_property(bm->gm2,
			GAUGE_PROP_BATTERY_CURRENT);

		if (current_ui_soc == 0) {
			if (sdu->pre_time[UISOC_ONE_PERCENT] == 0)
				sdu->pre_time[UISOC_ONE_PERCENT] = now;

			duraction =
				ktime_sub(
				now, sdu->pre_time[UISOC_ONE_PERCENT]);

			polling++;
			tmp_duraction = ktime_to_timespec64(duraction);
			if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				pr_debug("uisoc one notify zero percent after %d\n", (int)tmp_duraction.tv_sec);
				return polling;
			}
		} else if (now_current1 > 0 && now_current2 > 0 &&
			current_gm1_soc > 0 && current_gm2_soc > 0) {
			sdu->shutdown_status.is_uisoc_one_percent = 0;
			sdu->pre_time[UISOC_ONE_PERCENT] = 0;
			sdu->pre_time[SHUTDOWN_1_TIME] = 0;
			bm->force_ui_zero = 0;
			return polling;
		}
		/* ui_soc is not zero, check it after 10s */
		polling++;

	}

	if (sdu->shutdown_status.is_dlpt_shutdown) {
		duraction = ktime_sub(now, sdu->pre_time[DLPT_SHUTDOWN]);
		tmp_duraction = ktime_to_timespec64(duraction);
		polling++;
		if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
			pr_err("dlpt shutdown count, %d\n",
				(int)tmp_duraction.tv_sec);
			return next_waketime(polling);
		}
	}

	if (sdu->shutdown_status.is_under_shutdown_voltage) {

		int vbatcnt = 0, i;

		if (bm->gm1->disableGM30)
			chg_vbat = 4000;
		else
			chg_vbat = bm_get_vsys(bm);

		sdu->batdata[sdu->batidx] = chg_vbat;

		for (i = 0; i < bm->gm1->avgvbat_array_size; i++)
			vbatcnt += sdu->batdata[i];
		sdu->avgvbat = vbatcnt / bm->gm1->avgvbat_array_size;
		tmp = battery_get_int_property(bm->gm1, BAT_PROP_TEMPERATURE);
		tmp += battery_get_int_property(bm->gm2, BAT_PROP_TEMPERATURE);
		tmp = tmp / 2;

		pr_debug("lbatcheck vbat:%d avgvbat:%d tmp:%d,bound:%d,th:%d %d,en:%d\n",
			chg_vbat,
			sdu->avgvbat,
			tmp,
			bm->gm1->bat_voltage_low_bound,
			LOW_TEMP_THRESHOLD,
			bm->gm1->low_tmp_bat_voltage_low_bound,
			LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN);

		if (sdu->avgvbat < bm->gm1->bat_voltage_low_bound) {
			/* avg vbat less than 3.4v */
			sdu->lowbatteryshutdown = true;
			polling++;

			if (sdu->down_to_low_bat == 0) {
				if (IS_ENABLED(
					LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN)) {
					if (tmp >= LOW_TEMP_THRESHOLD) {
						sdu->down_to_low_bat = 1;
						pr_err("normal tmp, battery voltage is low shutdown\n");
						battery_set_property(bm->gm1,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
						battery_set_property(bm->gm2,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else if (sdu->avgvbat <=
						bm->gm1->low_tmp_bat_voltage_low_bound) {
						sdu->down_to_low_bat = 1;
						pr_err("cold tmp, battery voltage is low shutdown\n");
						battery_set_property(bm->gm1,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
						battery_set_property(bm->gm2,
							BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					} else
						pr_err("low temp disable low battery sd\n");
				} else {
					sdu->down_to_low_bat = 1;
					pr_err("[%s]avg vbat is low to shutdown\n",
						__func__);
					battery_set_property(bm->gm1,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
					battery_set_property(bm->gm2,
						BAT_PROP_WAKEUP_FG_ALGO, FG_INTR_SHUTDOWN);
				}
			}

			if ((current_ui_soc == 0) && (sdu->ui_zero_time_flag == 0)) {
				sdu->pre_time[LOW_BAT_VOLT] =
					ktime_get_boottime();
				sdu->ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				duraction = ktime_sub(
					now, sdu->pre_time[LOW_BAT_VOLT]);

				tmp_duraction  = ktime_to_timespec64(duraction);
				polling++;
				if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
					pr_debug("low bat notify zero percent, over %d second\n",
						SHUTDOWN_TIME);
					return polling;
				}
			}
		} else {
			/* greater than 3.4v, clear status */
			sdu->down_to_low_bat = 0;
			sdu->ui_zero_time_flag = 0;
			sdu->pre_time[LOW_BAT_VOLT] = 0;
			sdu->lowbatteryshutdown = false;
			if (sdu1->shutdown_status.is_under_shutdown_voltage == false &&
				sdu2->shutdown_status.is_under_shutdown_voltage == false)
				sdu->shutdown_status.is_under_shutdown_voltage = false;
			else
				polling++;
		}

		polling++;
		pr_debug("[%s][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d %d\n",
			__func__,
			sdu->avgvbat, current_ui_soc,
			(int)tmp_duraction.tv_sec,
			sdu->down_to_low_bat, sdu->ui_zero_time_flag,
			(int)sdu->pre_time[LOW_BAT_VOLT],
			sdu->lowbatteryshutdown,
			sdu->batidx, sdu->batdata[sdu->batidx], bm->gm1->avgvbat_array_size);

		sdu->batidx++;
		if (sdu->batidx >= bm->gm1->avgvbat_array_size)
			sdu->batidx = 0;
	}

	pr_debug(
		"%s %d avgvbat:%d sec:%d lowst:%d\n",
		__func__,
		polling, sdu->avgvbat,
		(int)tmp_duraction.tv_sec, sdu->lowbatteryshutdown);

	return polling;

}

static enum alarmtimer_restart power_misc_kthread_bm_timer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer[BATTERY_MANAGER]);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->timeout |= 0x1 << BATTERY_MANAGER;
	spin_unlock_irqrestore(&info->slock, flags);
	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart power_misc_kthread_gm2_timer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer[BATTERY_SLAVE]);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->timeout |= 0x1 << BATTERY_SLAVE;
	spin_unlock_irqrestore(&info->slock, flags);
	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart power_misc_kthread_gm1_timer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer[BATTERY_MAIN]);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->timeout |= 0x1 << BATTERY_MAIN;
	spin_unlock_irqrestore(&info->slock, flags);
	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}


static ktime_t check_power_misc_time(struct mtk_battery_manager *bm)
{
	ktime_t ktime;
	int vsys = 0;


	if (bm->disable_quick_shutdown == 1) {
		ktime = ktime_set(10, 0);
		goto out;
	}

	if (bm->gm_no == 1) {
		if (bm->sdc.bat[0].down_to_low_bat == 1) {
			ktime = ktime_set(10, 0);
			goto out;
		}
	} else {
		if (bm->sdc.bmsdu.down_to_low_bat == 1) {
			ktime = ktime_set(10, 0);
			goto out;
		}
	}

	vsys = bm_get_vsys(bm);
	if (vsys > bm->vsys_det_voltage1)
		ktime = ktime_set(10, 0);
	else if (vsys > bm->vsys_det_voltage2)
		ktime = ktime_set(1, 0);
	else
		ktime = ktime_set(0, 100 * NSEC_PER_MSEC);

out:
	pr_debug("%s check average timer vsys:%d, time(msec):%lld disable: %d bound: %d %d\n",
		__func__, vsys, ktime_to_ms(ktime),
			bm->disable_quick_shutdown, bm->vsys_det_voltage1, bm->vsys_det_voltage2);

	return ktime;
}

static int power_misc_routine_thread(void *arg)
{
	struct mtk_battery_manager *bm = arg;
	int ret = 0, i, retry = 0;
	unsigned long flags;
	int pending_flags;
	int polling[BATTERY_SDC_MAX] = {0};
	ktime_t ktime, time_now;

	while (1) {
		pr_debug("[%s] into\n", __func__);
		ret = wait_event_interruptible(bm->sdc.wait_que,
			(bm->sdc.timeout != 0) && !bm->is_suspend);

		if (ret < 0) {
			retry++;
			if (retry < 0xFFFFFFFF)
				continue;
			else {
				bm_err(bm->gm1, "%s something wrong retry: %d\n", __func__, retry);
				break;
			}
		}

		spin_lock_irqsave(&bm->sdc.slock, flags);
		pending_flags = bm->sdc.timeout;
		bm->sdc.timeout = 0;
		spin_unlock_irqrestore(&bm->sdc.slock, flags);

		pr_err("[%s] before %d\n", __func__, pending_flags);

		if(pending_flags & 1<<BATTERY_MAIN)
			polling[BATTERY_MAIN] = shutdown_event_handler(bm->gm1);

		if(pending_flags & 1<<BATTERY_SLAVE && bm->gm_no == 2)
			polling[BATTERY_SLAVE] = shutdown_event_handler(bm->gm2);

		if(pending_flags & 1<<BATTERY_MANAGER && bm->gm_no == 2)
			polling[BATTERY_MANAGER] = bm_shutdown_event_handler(bm);

		spin_lock_irqsave(&bm->sdc.slock, flags);
		pending_flags = bm->sdc.timeout;
		spin_unlock_irqrestore(&bm->sdc.slock, flags);

		pr_err("[%s] after %d M:%d F:%d S:%d\n", __func__,pending_flags, polling[0], polling[1], polling[2]);
		time_now  = ktime_get_boottime();
		ktime = check_power_misc_time(bm);
		for (i = 0; i < BATTERY_SDC_MAX; i++ ) {
			if (pending_flags & (1 << i) || polling[i]) {
				bm->sdc.endtime[i] = ktime_add(time_now, ktime);
				alarm_start(&bm->sdc.kthread_fgtimer[i], bm->sdc.endtime[i]);
			}

		}
		spin_lock_irqsave(&bm->sdc.slock, flags);
		__pm_relax(bm->sdc.sdc_wakelock);
		spin_unlock_irqrestore(&bm->sdc.slock, flags);
	}

	return 0;
}

void mtk_power_misc_init(struct mtk_battery_manager *bm, struct shutdown_controller *sdc)
{
	mutex_init(&sdc->lock);

	alarm_init(&sdc->kthread_fgtimer[BATTERY_MAIN], ALARM_BOOTTIME,
		power_misc_kthread_gm1_timer_func);

	if (bm->gm_no == 2) {
		alarm_init(&sdc->kthread_fgtimer[BATTERY_SLAVE], ALARM_BOOTTIME,
			power_misc_kthread_gm2_timer_func);
		alarm_init(&sdc->kthread_fgtimer[BATTERY_MANAGER], ALARM_BOOTTIME,
			power_misc_kthread_bm_timer_func);
	}

	init_waitqueue_head(&sdc->wait_que);

	if (bm->gm_no == 2) {
		sdc->bmsdu.type = BATTERY_MANAGER;
		sdc->bat[0].type = BATTERY_MAIN;
		sdc->bat[1].type = BATTERY_SLAVE;

		sdc->bat[0].gm = bm->gm1;
		sdc->bat[1].gm = bm->gm2;
	} else if (bm->gm_no == 1) {
		sdc->bmsdu.type = BATTERY_MANAGER;
		sdc->bat[0].type = BATTERY_MAIN;
		sdc->bat[0].gm = bm->gm1;
	}
	kthread_run(power_misc_routine_thread, bm, "power_misc_thread");
}


void bm_check_bootmode(struct device *dev,
	struct mtk_battery_manager *bm)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		pr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			pr_err("%s: failed to get atag,boot\n", __func__);
		else {
			pr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			bm->bootmode = tag->bootmode;
			bm->boottype = tag->boottype;
		}
	}
}

static void bm_update_status(struct mtk_battery_manager *bm)
{

	int vbat1, vbat2, ibat1, ibat2, vbat3 =0, vbat4 = 0;
	int car1, car2;
	struct mtk_battery *gm1;
	struct mtk_battery *gm2;
	struct fgd_cmd_daemon_data *d1;
	struct fgd_cmd_daemon_data *d2;

	if (bm->gm_no == 2) {

		gm1 = bm->gm1;
		gm2 = bm->gm2;
		d1 = &gm1->daemon_data;
		d2 = &gm2->daemon_data;
		vbat3 = get_charger_vbat(bm);
		vbat4 = bm_get_vsys(bm);

		vbat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE);
		ibat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_CURRENT);
		vbat2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_BATTERY_VOLTAGE);
		ibat2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_BATTERY_CURRENT);
		car1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_COULOMB);
		car2 = gauge_get_int_property(bm->gm2,
				GAUGE_PROP_COULOMB);

		pr_err("[%s] uisoc:%d %d %d soc:%d %d vbat:%d %d %d %d ibat:%d %d car:%d %d\n", __func__,
			bm->uisoc,
			bm->gm1->ui_soc, bm->gm2->ui_soc,
			bm->gm1->soc, bm->gm2->soc,
			vbat1, vbat2, vbat3, vbat4,
			ibat1, ibat2,
			car1, car2);

		pr_err("[bm_update_daemon1][%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
			d1->uisoc, d1->fg_c_soc, d1->fg_v_soc, d1->soc, d1->fg_c_d0_soc, d1->car_c, d1->fg_v_d0_soc,
			d1->car_v, d1->qmxa_t_0ma, d1->quse, d1->tmp, d1->vbat, d1->iavg, d1->aging_factor,
			d1->loading_factor1, d1->loading_factor2, d1->g_zcv_data, d1->g_zcv_data_soc,
			d1->g_zcv_data_mah, d1->tmp_show_ag, d1->tmp_bh_ag);

		pr_err("[bm_update_daemon2][%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
			d2->uisoc, d2->fg_c_soc, d2->fg_v_soc, d2->soc, d2->fg_c_d0_soc, d2->car_c, d2->fg_v_d0_soc,
			d2->car_v, d2->qmxa_t_0ma, d2->quse, d2->tmp, d2->vbat, d2->iavg, d2->aging_factor,
			d2->loading_factor1, d2->loading_factor2, d2->g_zcv_data, d2->g_zcv_data_soc,
			d2->g_zcv_data_mah, d2->tmp_show_ag, d2->tmp_bh_ag);

	} else if (bm->gm_no ==1) {
		vbat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_VOLTAGE);
		ibat1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_BATTERY_CURRENT);
		car1 = gauge_get_int_property(bm->gm1,
				GAUGE_PROP_COULOMB);

		pr_err("[%s] uisoc:%d %d soc:%d vbat:%d ibat:%d car:%d\n", __func__,
			bm->uisoc, bm->gm1->ui_soc,
			bm->gm1->soc,
			vbat1, ibat1, car1);
	}
}

static int battery_manager_routine_thread(void *arg)
{
	struct mtk_battery_manager *bm = arg;
	int ret = 0, retry = 0;
	struct timespec64 end_time, tmp_time_now;
	ktime_t ktime, time_now;
	unsigned long flags;

	while (1) {
		ret = wait_event_interruptible(bm->wait_que,
			(bm->bm_update_flag  > 0) && !bm->is_suspend);

		if (ret < 0) {
			retry++;
			if (retry < 0xFFFFFFFF)
				continue;
			else {
				bm_err(bm->gm1, "%s something wrong retry: %d\n", __func__, retry);
				break;
			}
		}

		spin_lock_irqsave(&bm->slock, flags);
		bm->bm_update_flag = 0;
		if (!bm->bm_wakelock->active)
			__pm_stay_awake(bm->bm_wakelock);
		spin_unlock_irqrestore(&bm->slock, flags);


		bm_update_status(bm);

		time_now  = ktime_get_boottime();
		tmp_time_now  = ktime_to_timespec64(time_now);
		end_time.tv_sec = tmp_time_now.tv_sec + 10;
		end_time.tv_nsec = tmp_time_now.tv_nsec;
		bm->endtime = end_time;
#ifdef BM_USE_HRTIMER
		ktime = ktime_set(10, 0);
		hrtimer_start(&bm->bm_hrtimer, ktime, HRTIMER_MODE_REL);
#endif
#ifdef BM_USE_ALARM_TIMER
		ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&bm->bm_alarmtimer, ktime);
#endif
		spin_lock_irqsave(&bm->slock, flags);
		__pm_relax(bm->bm_wakelock);
		spin_unlock_irqrestore(&bm->slock, flags);

	}

	return 0;
}

void battery_manager_routine_wakeup(struct mtk_battery_manager *bm)
{
	unsigned long flags;

	spin_lock_irqsave(&bm->slock, flags);
	bm->bm_update_flag = 1;
	if (!bm->bm_wakelock->active)
		__pm_stay_awake(bm->bm_wakelock);
	spin_unlock_irqrestore(&bm->slock, flags);
	wake_up(&bm->wait_que);
}

#ifdef BM_USE_HRTIMER
enum hrtimer_restart battery_manager_thread_hrtimer_func(struct hrtimer *timer)
{
	struct mtk_battery_manager *bm;

	bm = container_of(timer,
		struct mtk_battery_manager, bm_hrtimer);
	battery_manager_routine_wakeup(bm);
	return HRTIMER_NORESTART;
}

void battery_manager_thread_hrtimer_init(struct mtk_battery_manager *bm)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	hrtimer_init(&bm->bm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bm->bm_hrtimer.function = battery_manager_thread_hrtimer_func;
	hrtimer_start(&bm->bm_hrtimer, ktime, HRTIMER_MODE_REL);
}
#endif


#ifdef CONFIG_PM
static int bm_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	ktime_t ktime_now;
	struct timespec64 now;
	struct mtk_battery_manager *bm;
	unsigned long flags;
	int i, pending_flags, wake_up_power = 0;

	bm = container_of(notifier,
		struct mtk_battery_manager, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		bm->is_suspend = true;
		pr_err("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		bm->is_suspend = false;
		pr_err("%s: enter PM_POST_SUSPEND\n", __func__);
		ktime_now = ktime_get_boottime();
		now = ktime_to_timespec64(ktime_now);

		if (timespec64_compare(&now, &bm->endtime) >= 0 &&
			bm->endtime.tv_sec != 0 &&
			bm->endtime.tv_nsec != 0) {
			pr_err("%s: alarm timeout, wake up charger\n",
				__func__);

			spin_lock_irqsave(&bm->slock, flags);
			__pm_relax(bm->bm_wakelock);
			spin_unlock_irqrestore(&bm->slock, flags);
			bm->endtime.tv_sec = 0;
			bm->endtime.tv_nsec = 0;
			battery_manager_routine_wakeup(bm);
		}

		spin_lock_irqsave(&bm->sdc.slock, flags);
		pending_flags = bm->sdc.timeout;
		__pm_relax(bm->sdc.sdc_wakelock);
		spin_unlock_irqrestore(&bm->sdc.slock, flags);
		for (i = 0; i < BATTERY_SDC_MAX; i++ ) {
			if (pending_flags & (1 << i) &&
				ktime_compare(ktime_now, bm->sdc.endtime[i]) >= 0) {
				pr_err("%s: alarm timeout, wake up power %d\n",
					__func__, i);
				spin_lock_irqsave(&bm->sdc.slock, flags);
				bm->sdc.timeout |= 0x1 << i;
				spin_unlock_irqrestore(&bm->sdc.slock, flags);
				wake_up_power = 1;
			}

		}
		if (wake_up_power)
			wake_up_power_misc(&bm->sdc);

		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

#ifdef BM_USE_ALARM_TIMER
enum alarmtimer_restart battery_manager_thread_alarm_func(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery_manager *bm;
	unsigned long flags;

	bm = container_of(alarm,
		struct mtk_battery_manager, bm_alarmtimer);

	if (bm->is_suspend == false) {
		pr_err("%s: not suspend, wake up charger\n", __func__);
		battery_manager_routine_wakeup(bm);
	} else {
		pr_err("%s: alarm timer timeout\n", __func__);
			spin_lock_irqsave(&bm->slock, flags);
		if (!bm->bm_wakelock->active)
			__pm_stay_awake(bm->bm_wakelock);
		spin_unlock_irqrestore(&bm->slock, flags);
	}

	return ALARMTIMER_NORESTART;
}

void battery_manager_thread_alarm_init(struct mtk_battery_manager *bm)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	alarm_init(&bm->bm_alarmtimer, ALARM_BOOTTIME,
		battery_manager_thread_alarm_func);
	alarm_start(&bm->bm_alarmtimer, ktime);
}
#endif

static void bm_send_cmd(struct mtk_battery_manager *bm, enum manager_cmd cmd, int val)
{
	int ret = 0;

	if (bm->gm1 != NULL && bm->gm1->manager_send != NULL)
		ret = bm->gm1->manager_send(bm->gm1, cmd, val);
	else
		pr_err("%s gm1->manager_send is null\n", __func__);

	if (bm->gm_no == 2) {
		if (bm->gm2 != NULL && bm->gm2->manager_send != NULL)
			ret |= bm->gm2->manager_send(bm->gm2, cmd, val);
		else
			pr_err("%s gm2->manager_send is null\n", __func__);
	}

	if (ret < 0)
		pr_err("%s manager_send return faiil\n",__func__);
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

static int bm_update_psy_property(struct mtk_battery *gm, enum bm_psy_prop prop)
{
	int ret_val = 0, ret = 0;
	int curr_now = 0, curr_avg = 0, voltage_now = 0;

	switch (prop) {
	case CURRENT_NOW:
		ret = gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
			&curr_now, 1);

		if (ret == -EHOSTDOWN)
			ret_val = gm->ibat;
		else {
			ret_val = curr_now;
			gm->ibat = curr_now;
		}
		break;
	case CURRENT_AVG:
		ret = gauge_get_property_control(gm, GAUGE_PROP_AVERAGE_CURRENT,
			&curr_avg, 1);

		if (ret == -EHOSTDOWN)
			ret_val = gm->ibat;
		else
			ret_val = curr_avg;
		break;
	case VOLTAGE_NOW:
		/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
		/* 6= ATE_factory_boot */
		if (gm->bootmode == 1 || gm->bootmode == 4
			|| gm->bootmode == 5 || gm->bootmode == 6) {
			ret_val = 4000;
			break;
		}

		if (gm->disableGM30)
			voltage_now = 4000;
		else
			ret = gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
				&voltage_now, 1);

		if (ret == -EHOSTDOWN)
			ret_val = gm->vbat;
		else {
			gm->vbat = voltage_now;
			ret_val = voltage_now;
		}
		break;
	case TEMP:
		ret_val = battery_get_int_property(gm, BAT_PROP_TEMPERATURE);
		break;
	case QMAX_DESIGN:
		ret_val = gm->fg_table_cust_data.fg_profile[0].q_max * 10;
		break;
	case QMAX:
		ret_val = gm->daemon_data.qmxa_t_0ma;
		break;
	default:
		break;
	}
	return ret_val;
}

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
#define DESIGNED_CAPACITY 5000 //mAh
#define CHARGE_FULL_SOC 100
#define CHARGE_30_SOC 30
#define CHARGE_50_SOC 50
#define CHARGE_53_SOC 53
#define CHARGE_75_SOC 75
#define CHARGE_80_SOC 80
#define CHARGE_90_SOC 90
#define CHARGE_93_SOC 93
#define CHARGE_SOC_OFFSET 5
#define DEADSOC_COEFFICIENT1 98
#define DEADSOC_COEFFICIENT2 97
#define DEADSOC_COEFFICIENT3 91
#define DEADSOC_COEFFICIENT4 90
#define DEADSOC_COEFFICIENT5 88

#define CHARGE_STATE_CHANGE_SOC1 84
#define CHARGE_STATE_CHANGE_SOC2 90
#define CHARGE_STATE_CHANGE_SOC3 95
#define CHARGE_STATE_CHANGE_SOC4 97

#define CHARGE_PPS_5A_CC_CURRENT_THRESHOLD     4500
#define CHARGE_PPS_4A_CC_CURRENT_THRESHOLD     3500
#define CHARGE_PPS_3A_CC_CURRENT_THRESHOLD     2200
#define CHARGE_PPS_2A_CC_CURRENT_THRESHOLD     1500
#define CHARGE_PPS_1A_CC_CURRENT_THRESHOLD     900

#define CHARGE_3A_CC_CURRENT_THRESHOLD         2200
#define CHARGE_2A_CC_CURRENT_THRESHOLD         1600
#define CHARGE_1A_CC_CURRENT_THRESHOLD         900

#define CHARGE_CC_CURRENT_THRESHOLD 1700

#define MAGIC_PPS_CHARGE_5A_CC_CURRENT1       3500
#define MAGIC_PPS_CHARGE_4A_CC_CURRENT1       3200
#define MAGIC_PPS_CHARGE_4A_CC_CURRENT2       2800
#define MAGIC_PPS_CHARGE_3P5A_CC_CURRENT1     2500
#define MAGIC_PPS_CHARGE_3A_CC_CURRENT1       2200
#define MAGIC_PPS_CHARGE_3A_CC_CURRENT2       2000
#define MAGIC_PPS_CHARGE_2P5A_CC_CURRENT1     2700
#define MAGIC_PPS_CHARGE_2A_CC_CURRENT1       1800
#define MAGIC_PPS_CHARGE_2A_CC_CURRENT2       1500
#define MAGIC_PPS_CHARGE_1P5A_CC_CURRENT1     1400
#define MAGIC_PPS_CHARGE_1A_CC_CURRENT1       1000

#define MAGIC_CHARGE_3A_CC_CURRENT1 2200
#define MAGIC_CHARGE_3A_CC_CURRENT2 2600
#define MAGIC_CHARGE_3A_CC_CURRENT3 2000

#define MAGIC_CHARGE_2A_CC_CURRENT1 1800
#define MAGIC_CHARGE_2A_CC_CURRENT2 1600
#define MAGIC_CHARGE_2A_CC_CURRENT3 1700
#define MAGIC_CHARGE_2A_CC_CURRENT4 1500

#define MAGIC_CHARGE_1A_CC_CURRENT1 1200
#define MAGIC_CHARGE_1A_CC_CURRENT2 900

#define MAGIC_CHARGE_CC_USB_CURRENT 380

#define MAGIC_CHARGE_END_CV_CURRENT 700

#define UPDATE_TO_FULL_INTERVAL_S 12
#define RECHECK_DCP_INTERVAL_S 5

#define WT_INTERMAL_TIME_STEP 10
#define WT_INTERMAL_TIME_NORMAL 60
#define WT_INTERMAL_TIME_LOW1 50
#define WT_INTERMAL_TIME_LOW2 40
#define WT_INTERMAL_TIME_HIGH1 70
#define WT_INTERMAL_TIME_HIGH2 80
#define WT_INTERMAL_TIME_HIGH3 90
#define WT_INTERMAL_TIME_HIGH4 100
#define WT_INTERMAL_TIME_HIGH5 110
#define WT_INTERMAL_TIME_HIGH6 120
#define WT_INTERMAL_TIME_HIGH7 180
#define WT_INTERMAL_TIME_HIGH8 240
#define WT_INTERMAL_TIME_HIGH9 300
#define WT_INTERMAL_TIME_HIGH10 600
#define WT_INTERMAL_TIME_MAX   2000

#if defined (WT_OPTIMIZE_USING_HYSTERESIS)
#define CURRENT_FALL_HYS_MA  100
#define CURRENT_RISE_HYS_MA  50
#else
#define CURRENT_FALL_HYS_MA  0
#define CURRENT_RISE_HYS_MA  0
#endif

#define ACG_CURRENT_SIZE 80
int wt_avg_current[ACG_CURRENT_SIZE];
int wt_current_sum = 0;

static void init_avg_current(int vfgcurrent)
{
	int index = 0;
	for (index = 0; index < ACG_CURRENT_SIZE; index++) {
		wt_avg_current[index] = vfgcurrent;
		//pr_err("%s: wt_avg_current[%d]=%d\n", __func__, index, wt_avg_current[index]);
	}
	wt_current_sum = vfgcurrent * ACG_CURRENT_SIZE;
}

static int calculate_avg_current(int vfgcurrent)
{
	int index = 0;
	int current_value = vfgcurrent;

	if (vfgcurrent <= 10)
		return vfgcurrent;
	if (wt_avg_current[ACG_CURRENT_SIZE-1] == -1) {
		init_avg_current(current_value);
		return current_value;
	}

	wt_current_sum -= wt_avg_current[index];

	for (index = 0; index < (ACG_CURRENT_SIZE - 1); index++) {
		wt_avg_current[index] = wt_avg_current[index+1];
		//pr_err("%s: wt_avg_current[%d]=%d\n", __func__,index, wt_avg_current[index]);
	}

	wt_avg_current[index] = vfgcurrent;

	//pr_err("%s: wt_avg_current[%d]=%d\n", __func__, index, wt_avg_current[index]);

	wt_current_sum += wt_avg_current[index];

	current_value = wt_current_sum / ACG_CURRENT_SIZE;

	return current_value;

}


static int fulltime_get_sys_time(void)
{
	struct rtc_time tm_android = {0};
	struct timespec64 tv_android = {0};
	int timep = 0;

	ktime_get_real_ts64(&tv_android);
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	timep = tm_android.tm_sec + tm_android.tm_min * 60 + tm_android.tm_hour * 3600;

	return timep;
}

static int select_apdo_magic_current(int fgcurrent, int capacity, int interval)
{
	int magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
	static int apdo_pre_magic_current = 0;
	static int apdo_current_threshold = 0;
	int apdo_init_avg_current = 0;
	int current_fall_hys = CURRENT_FALL_HYS_MA;
	int current_rise_hys = CURRENT_RISE_HYS_MA;

	if (interval > UPDATE_TO_FULL_INTERVAL_S) {
		if (fgcurrent > CHARGE_PPS_5A_CC_CURRENT_THRESHOLD) {
			if ((apdo_current_threshold == CURRENT_LEVEL2)
				&& (fgcurrent < (CHARGE_PPS_5A_CC_CURRENT_THRESHOLD + current_rise_hys))) {
				magic_current = apdo_pre_magic_current;
			} else {
				magic_current = MAGIC_PPS_CHARGE_5A_CC_CURRENT1;
				apdo_current_threshold = CURRENT_LEVEL1;
			}
		} else if (fgcurrent > CHARGE_PPS_4A_CC_CURRENT_THRESHOLD) {
			if (((apdo_current_threshold == CURRENT_LEVEL1)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_PPS_5A_CC_CURRENT_THRESHOLD))
				|| ((apdo_current_threshold == CURRENT_LEVEL3)
				&& (fgcurrent < (CHARGE_PPS_4A_CC_CURRENT_THRESHOLD + current_rise_hys)))) {
				magic_current = apdo_pre_magic_current;
			} else {
				magic_current = MAGIC_PPS_CHARGE_4A_CC_CURRENT1;
				apdo_current_threshold = CURRENT_LEVEL2;
			}
		} else if (fgcurrent > CHARGE_PPS_3A_CC_CURRENT_THRESHOLD) {
			if (((apdo_current_threshold == CURRENT_LEVEL2)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_PPS_4A_CC_CURRENT_THRESHOLD))
				|| ((apdo_current_threshold == CURRENT_LEVEL4)
				&& (fgcurrent < (CHARGE_PPS_3A_CC_CURRENT_THRESHOLD + current_rise_hys)))) {
				magic_current = apdo_pre_magic_current;
			} else {
				magic_current = MAGIC_PPS_CHARGE_3A_CC_CURRENT1;
				apdo_current_threshold = CURRENT_LEVEL3;
			}
		} else if (fgcurrent > CHARGE_PPS_2A_CC_CURRENT_THRESHOLD) {
			if (((apdo_current_threshold == CURRENT_LEVEL3)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_PPS_3A_CC_CURRENT_THRESHOLD))
				|| ((apdo_current_threshold == CURRENT_LEVEL5)
				&& (fgcurrent < (CHARGE_PPS_2A_CC_CURRENT_THRESHOLD + current_rise_hys)))) {
				magic_current = apdo_pre_magic_current;
			} else {
				magic_current = MAGIC_PPS_CHARGE_2A_CC_CURRENT1;
				apdo_current_threshold = CURRENT_LEVEL4;
			}
		} else if (fgcurrent > CHARGE_PPS_1A_CC_CURRENT_THRESHOLD) {
			if (((apdo_current_threshold == CURRENT_LEVEL4)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_PPS_2A_CC_CURRENT_THRESHOLD))
				|| ((apdo_current_threshold == CURRENT_LEVEL6)
				&& (fgcurrent < (CHARGE_PPS_1A_CC_CURRENT_THRESHOLD + current_rise_hys)))) {
				magic_current = apdo_pre_magic_current;
			} else {
				magic_current = MAGIC_PPS_CHARGE_1A_CC_CURRENT1;
				apdo_current_threshold = CURRENT_LEVEL5;
			}
		} else if ((fgcurrent <= CHARGE_PPS_1A_CC_CURRENT_THRESHOLD) && (fgcurrent > 10)) {
			if (((apdo_current_threshold == CURRENT_LEVEL5)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_PPS_1A_CC_CURRENT_THRESHOLD))) {
				magic_current = apdo_pre_magic_current;
			} else {
				magic_current = MAGIC_CHARGE_END_CV_CURRENT;
				apdo_current_threshold = CURRENT_LEVEL6;
			}
		} else {
			magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
			apdo_current_threshold = CURRENT_LEVEL7;
		}
		apdo_pre_magic_current = magic_current;
	} else {
		if (capacity < CHARGE_53_SOC) {
			magic_current = MAGIC_PPS_CHARGE_5A_CC_CURRENT1;
			apdo_init_avg_current =  CHARGE_PPS_5A_CC_CURRENT_THRESHOLD;
		} else if (capacity < CHARGE_80_SOC) {
			magic_current = MAGIC_PPS_CHARGE_4A_CC_CURRENT1;
			apdo_init_avg_current =  CHARGE_PPS_4A_CC_CURRENT_THRESHOLD;
		} else if (capacity < CHARGE_93_SOC) {
			magic_current = MAGIC_PPS_CHARGE_2A_CC_CURRENT1;
			apdo_init_avg_current =  magic_current;
		} else if (capacity < CHARGE_STATE_CHANGE_SOC4) {
			magic_current = MAGIC_PPS_CHARGE_1P5A_CC_CURRENT1;
			apdo_init_avg_current =  magic_current;
		}  else {
			magic_current = MAGIC_PPS_CHARGE_1A_CC_CURRENT1;
			apdo_init_avg_current =  magic_current;
		}

		init_avg_current(apdo_init_avg_current);

		apdo_pre_magic_current = 0;
		apdo_current_threshold = 0;
	}

	pr_err("%s222: current_threshold=%d, pre_magic_current=%d\n",
		__func__, apdo_current_threshold, apdo_pre_magic_current);

	return magic_current;
}

static int wt_get_charge_source(struct mtk_charger *pinfo)
{
	int batt_charging_source = 0;
	if (pinfo == NULL)
		return SEC_BATTERY_CABLE_UNKNOWN;

	if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
		pinfo->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		batt_charging_source = SEC_BATTERY_CABLE_USB;
	} else if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
#if defined (CONFIG_W2_CHARGER_PRIVATE)
		batt_charging_source = SEC_BATTERY_CABLE_PDIC;
#else
		batt_charging_source = SEC_BATTERY_CABLE_PDIC_APDO;
#endif
	} else if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) {
		batt_charging_source = SEC_BATTERY_CABLE_PDIC;
#ifdef CONFIG_AFC_CHARGER
	} else if (afc_get_is_connect(pinfo)) {
		batt_charging_source = SEC_BATTERY_CABLE_9V_TA;
#endif
	} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		batt_charging_source = SEC_BATTERY_CABLE_USB_CDP;
	}  else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		batt_charging_source = SEC_BATTERY_CABLE_TA;
	} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
		pinfo->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
	} else {
		batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
	}

	return batt_charging_source;
}

static int select_basic_magic_current(int fgcurrent,
			int capacity, struct mtk_charger *pinfo, int interval)
{
	int magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
	int batt_charging_source = 0;
	static int pre_magic_current = 0;
	static int current_threshold = 0;
	int current_fall_hys = CURRENT_FALL_HYS_MA;
	int current_rise_hys = CURRENT_RISE_HYS_MA;

	if (interval > UPDATE_TO_FULL_INTERVAL_S) {
		pr_err("%s111: current_threshold=%d, pre_magic_current=%d\n",
			__func__, current_threshold, pre_magic_current);
		if (fgcurrent > CHARGE_3A_CC_CURRENT_THRESHOLD) {
			if ((current_threshold == CURRENT_LEVEL2)
				&& ((fgcurrent - current_rise_hys) < CHARGE_3A_CC_CURRENT_THRESHOLD)) {
				magic_current = pre_magic_current;
			} else {
				if (capacity < CHARGE_STATE_CHANGE_SOC1) {
					magic_current = MAGIC_CHARGE_3A_CC_CURRENT1;
				} else {
					magic_current = MAGIC_CHARGE_3A_CC_CURRENT3;
				}
				current_threshold = CURRENT_LEVEL1;
			}
		} else if (fgcurrent > CHARGE_2A_CC_CURRENT_THRESHOLD) {
			if (((current_threshold == CURRENT_LEVEL1)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_3A_CC_CURRENT_THRESHOLD))
				|| ((current_threshold == CURRENT_LEVEL3)
				&& ((fgcurrent - current_rise_hys) < CHARGE_2A_CC_CURRENT_THRESHOLD))) {
				magic_current = pre_magic_current;
			} else {
				if (capacity < CHARGE_STATE_CHANGE_SOC2) {
					magic_current = MAGIC_CHARGE_2A_CC_CURRENT1;
				} else {
					magic_current = MAGIC_CHARGE_2A_CC_CURRENT2;
				}
				current_threshold = CURRENT_LEVEL2;
			}
		} else if (fgcurrent > CHARGE_1A_CC_CURRENT_THRESHOLD) {
			if (((current_threshold == CURRENT_LEVEL2)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_2A_CC_CURRENT_THRESHOLD))
				|| ((current_threshold == CURRENT_LEVEL4)
				&& ((fgcurrent - current_rise_hys) < CHARGE_1A_CC_CURRENT_THRESHOLD))) {
				magic_current = pre_magic_current;
			} else {
				if (capacity < CHARGE_STATE_CHANGE_SOC3) {
					magic_current = MAGIC_CHARGE_1A_CC_CURRENT1;
				} else {
					magic_current = MAGIC_CHARGE_1A_CC_CURRENT2;
				}
				current_threshold = CURRENT_LEVEL3;
			}
		} else if ((fgcurrent <= CHARGE_1A_CC_CURRENT_THRESHOLD) && (fgcurrent > 10)) {
			if ((current_threshold == CURRENT_LEVEL3)
				&& ((fgcurrent + current_fall_hys) >= CHARGE_1A_CC_CURRENT_THRESHOLD)) {
				magic_current = pre_magic_current;
			} else {
				if (pinfo && pinfo->chr_type == POWER_SUPPLY_TYPE_USB) {
					magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
				} else {
					magic_current = MAGIC_CHARGE_END_CV_CURRENT;
				}
				current_threshold = CURRENT_LEVEL4;
			}
		} else {
			magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
			current_threshold = CURRENT_LEVEL5;
		}
		pre_magic_current = magic_current;
	} else {
		if (pinfo) {
			batt_charging_source = wt_get_charge_source(pinfo);
		}
		pr_err("%s: batt_charging_source=%d\n", __func__, batt_charging_source);
		switch (batt_charging_source) {
			case SEC_BATTERY_CABLE_TA:
			case SEC_BATTERY_CABLE_USB_CDP:
				if (capacity < CHARGE_STATE_CHANGE_SOC3) {
					magic_current = MAGIC_CHARGE_1A_CC_CURRENT1;
				} else if (capacity < CHARGE_STATE_CHANGE_SOC4) {
					magic_current = MAGIC_CHARGE_1A_CC_CURRENT2;
				} else {
					magic_current = MAGIC_CHARGE_END_CV_CURRENT;
				}
				break;
			case SEC_BATTERY_CABLE_PDIC_APDO:
				if (!pinfo->disable_quick_charge) {
					pr_err("%s: Invalid: logic error\n", __func__);
					magic_current = MAGIC_PPS_CHARGE_3A_CC_CURRENT1;
				} else {
					if (capacity < CHARGE_STATE_CHANGE_SOC2) {
						magic_current = MAGIC_CHARGE_2A_CC_CURRENT4;
					} else if (capacity < CHARGE_STATE_CHANGE_SOC3) {
						magic_current = MAGIC_CHARGE_1A_CC_CURRENT1;
					} else {
						magic_current = MAGIC_CHARGE_1A_CC_CURRENT2;
					}
				}
				break;
			case SEC_BATTERY_CABLE_9V_TA:
			case SEC_BATTERY_CABLE_PDIC:
				if (pinfo->disable_quick_charge) {
					if (capacity < CHARGE_STATE_CHANGE_SOC2) {
						magic_current = MAGIC_CHARGE_2A_CC_CURRENT4;
					} else if (capacity < CHARGE_STATE_CHANGE_SOC3) {
						magic_current = MAGIC_CHARGE_1A_CC_CURRENT1;
					} else {
						magic_current = MAGIC_CHARGE_1A_CC_CURRENT2;
					}
				} else {
					if (capacity < CHARGE_STATE_CHANGE_SOC1) {
						magic_current = MAGIC_CHARGE_3A_CC_CURRENT1;
					} else if (capacity < CHARGE_STATE_CHANGE_SOC2) {
						magic_current = MAGIC_CHARGE_2A_CC_CURRENT1;
					} else if (capacity < CHARGE_STATE_CHANGE_SOC3) {
						magic_current = MAGIC_CHARGE_2A_CC_CURRENT4;
					} else if (capacity < CHARGE_STATE_CHANGE_SOC4) {
						magic_current = MAGIC_CHARGE_1A_CC_CURRENT1;
					} else {
						magic_current = MAGIC_CHARGE_1A_CC_CURRENT2;
					}
				}
				break;
			case SEC_BATTERY_CABLE_USB:
				magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
				break;
			default:
				magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
				break;
		}

		init_avg_current(magic_current);
		pre_magic_current = 0;
		current_threshold = 0;
	}
	pr_err("%s222: current_threshold=%d, pre_magic_current=%d\n",
		__func__, current_threshold, pre_magic_current);
	return magic_current;
}

static int  wt_get_current_now(struct mtk_battery_manager *bm)
{
	int curr_now = 0;
	if (bm->gm1 != NULL)
		if(!bm->gm1->bat_plug_out)
			curr_now += bm_update_psy_property(bm->gm1, CURRENT_NOW);
	if (bm->gm2 != NULL)
		if(!bm->gm2->bat_plug_out)
			curr_now += bm_update_psy_property(bm->gm2, CURRENT_NOW);

	curr_now = curr_now * 100;

	return curr_now;
}

static int wt_get_batt_cycle_cnt(struct mtk_battery_manager *bm)
{
	int batt_cycle_cnt = 0;
	int qmax = 0, cycle = 0;

	if (bm->gm1 != NULL) {
		if(!bm->gm1->bat_plug_out) {
			cycle += (bm->gm1->bat_cycle + 1) *
				bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		}
	}

	if (bm->gm2 != NULL) {
		if(!bm->gm2->bat_plug_out) {
			cycle += (bm->gm2->bat_cycle + 1) *
				bm_update_psy_property(bm->gm2, QMAX_DESIGN);
			qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);
		}
	}

	if (qmax != 0)
		batt_cycle_cnt = cycle / qmax;
	pr_err("[%s] batt_cycle_cnt=%d\n", __func__, batt_cycle_cnt);
	return batt_cycle_cnt;
}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
static int wt_get_batt_full_maximum_offset(struct mtk_charger *pinfo)
{
	int soc_maximum_offset = 0;

	if (pinfo == NULL)
		return -1;

	if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_80_OFFCHARGING) {
		soc_maximum_offset =
			pinfo->batt_full_capacity - POWER_SUPPLY_CAPACITY_80_OPTION;
	}

	return soc_maximum_offset / 2;
}
#endif
static int wt_get_battery_remain_mah(struct mtk_battery_manager *gm,
						struct mtk_charger *pinfo, int soc)
{
	int remain_ui = 0;
	int capacity = 0;
	int remain_mah = 0;
	int deadsoc_coefficient = DEADSOC_COEFFICIENT1;
	int bat_cycle = 0;
	int charge_full_capacity = CHARGE_FULL_SOC;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	int soc_maximum_offset = 0;
#endif

	if (gm == NULL)
		return -1;

	if (pinfo == NULL)
		return -1;

	capacity = soc;
	if (capacity < 0) {
		return -1;
	}
	bat_cycle = wt_get_batt_cycle_cnt(gm);

	if (bat_cycle < 199) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT1;
	} else if (bat_cycle < 249) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT2;
	} else if (bat_cycle < 299) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT3;
	} else if (bat_cycle < 1000) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT4;
	} else {
		deadsoc_coefficient = DEADSOC_COEFFICIENT5;
	}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
		soc_maximum_offset = wt_get_batt_full_maximum_offset(pinfo);
		if (soc_maximum_offset < 0)
			return -1;
		charge_full_capacity =
			CHARGE_80_SOC + soc_maximum_offset * CHARGE_SOC_OFFSET;
	}
#endif
	if (charge_full_capacity > CHARGE_FULL_SOC) {
		charge_full_capacity = CHARGE_FULL_SOC;
	}

	if (capacity >= charge_full_capacity) {
		capacity = charge_full_capacity;
	}

	pr_err("%s: charge_full_capacity=%d\n", __func__, charge_full_capacity);
	remain_ui = charge_full_capacity - capacity;

	remain_mah = DESIGNED_CAPACITY * deadsoc_coefficient * remain_ui / 100 / 100;
	return remain_mah;
}

static int wt_get_slow_update_th(int wt_initial_time_interval,
					int time_to_full_update_th, int capacity)
{
	int time_to_full_update_th_new = WT_INTERMAL_TIME_NORMAL;
	int wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;

	if (capacity <= 70) {
		if (wt_initial_time_interval < 900) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;
		} else if (wt_initial_time_interval < 1500) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH1;
		} else {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH2;
		}
	} else if (capacity <= 80) {
		if (wt_initial_time_interval < 480) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;
		} else if (wt_initial_time_interval < 900) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH1;
		} else if (wt_initial_time_interval < 1200) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH2;
		} else {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH3;
		}
	} else if (capacity < 95) {
		if (wt_initial_time_interval < 120) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;
		} else if (wt_initial_time_interval < 300) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH1;
		} else {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH3;
		}
	} else {
		wt_time_to_full_update_max = WT_INTERMAL_TIME_HIGH3;
	}

	if (time_to_full_update_th <= wt_time_to_full_update_max) {
		time_to_full_update_th_new = time_to_full_update_th + WT_INTERMAL_TIME_STEP;
	} else {
		time_to_full_update_th_new = wt_time_to_full_update_max + WT_INTERMAL_TIME_STEP;
	}
	pr_err("%s: time_th=%d, time_th_new=%d, wt_time_max=%d\n",
		__func__, time_to_full_update_th,
		time_to_full_update_th_new,	wt_time_to_full_update_max);
	return time_to_full_update_th_new;

}

static int wt_check_slow_critical_update_th(int ui_raw_time_diff,
					int ui_time_to_full, int capacity, int critical_soc)
{
	int time_to_full_update_th = -1;

	if (capacity >= critical_soc) {
		if (ui_time_to_full < 300) {
			if (ui_raw_time_diff >= 2000) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH9;
			} else if (ui_raw_time_diff >= 1500) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH8;
			} else if (ui_raw_time_diff >= 1000) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH7;
			} else if (ui_raw_time_diff >= 600) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH6;
			} else if (ui_raw_time_diff >= 300) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH5;
			} else	if (ui_raw_time_diff >= 100) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH2;
			}
		} else if (ui_time_to_full < 660) {
			if (ui_raw_time_diff >= 2000) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH8;
			} else if (ui_raw_time_diff >= 1500) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH7;
			} else if (ui_raw_time_diff >= 1000) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH6;
			} else if (ui_raw_time_diff >= 300) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH5;
			} else	if (ui_raw_time_diff >= 100) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH2;
			}
		} else if (ui_time_to_full < 1000) {
			if (ui_raw_time_diff >= 2000) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH7;
			} else if (ui_raw_time_diff >= 1500) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH6;
			} else if (ui_raw_time_diff >= 600) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH5;
			} else	if (ui_raw_time_diff >= 100) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH2;
			}
		}
	}

	pr_err("%s: time_th=%d\n", __func__, time_to_full_update_th);
	return time_to_full_update_th;
}

static int wt_get_quick_update_th(int wt_initial_time_interval,
					int time_to_full_update_th, int capacity)
{
	int time_to_full_update_th_new = WT_INTERMAL_TIME_NORMAL;
	int wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;

	if (capacity <= 70) {
		if (wt_initial_time_interval < 900) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;
		} else {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_LOW1;
		}
	} else if (capacity <= 80) {
		if (wt_initial_time_interval < 480) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;
		} else {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_LOW1;
		}
	} else if (capacity < 95) {
		if (wt_initial_time_interval < 120) {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_NORMAL;
		} else {
			wt_time_to_full_update_max = WT_INTERMAL_TIME_LOW1;
		}
	} else {
		wt_time_to_full_update_max = WT_INTERMAL_TIME_LOW1;
	}

	if (time_to_full_update_th >= wt_time_to_full_update_max) {
		time_to_full_update_th_new = time_to_full_update_th - WT_INTERMAL_TIME_STEP;
	} else {
		time_to_full_update_th_new = wt_time_to_full_update_max - WT_INTERMAL_TIME_STEP;
	}

	pr_err("%s: time_th=%d, time_th_new=%d, wt_time_max=%d\n",
		__func__, time_to_full_update_th,
		time_to_full_update_th_new,	wt_time_to_full_update_max);
	return time_to_full_update_th_new;

}

static int wt_check_quick_critical_offset(int ui_raw_time_diff,
					int raw_time_to_full, int capacity, int critical_soc)
{
	int wt_time_to_full_offset = -1;

	if (capacity >= critical_soc) {
		if (raw_time_to_full < 300) {
			if (ui_raw_time_diff >= 600) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH6;
			} else if (ui_raw_time_diff >= 400) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH5;
			} else if (ui_raw_time_diff >= 200) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH4;
			} else if (ui_raw_time_diff >= 120) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH2;
			}
		} else if (raw_time_to_full < 660) {
			if (ui_raw_time_diff >= 600) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH5;
			} else if (ui_raw_time_diff >= 300) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH4;
			}
		} else if (raw_time_to_full < 1000) {
			if (ui_raw_time_diff >= 600) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH4;
			} else if (ui_raw_time_diff >= 300) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH3;
			}
		}
	}

	pr_err("%s: time_offset=%d\n", __func__, wt_time_to_full_offset);
	return wt_time_to_full_offset;

}

static int wt_check_min_remain_time(struct mtk_charger *pinfo,
					int ui_time_to_full, int capacity)
{
	int batt_charging_source = 0;
	int time_to_full_update_th = -1;
	int ui_time_to_full_min1 = 0;
	int ui_time_to_full_min2 = 0;
	int ui_time_to_full_min3 = 0;
	int ui_time_to_full_min4 = 0;
	int ui_time_to_full_min5 = 0;
	int ui_time_to_full_min6 = 0;
	int ui_time_to_full_min7 = 0;
	int ui_time_to_full_min8 = 0;
	bool is_protection_mode = false;
	bool disable_quick_charge = false;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	int soc_maximum_offset = 0;
#endif
	int capacity_threshold1 = 60;
	int capacity_threshold2 = 70;
	int capacity_threshold3 = 80;
	int capacity_threshold4 = 90;
	int capacity_threshold5 = 95;
	int capacity_threshold6 = 97;
	int capacity_threshold7 = 98;
	int capacity_threshold8 = 99;

	if (pinfo == NULL)
		return -1;

	disable_quick_charge = pinfo->disable_quick_charge;

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
		is_protection_mode = true;
		soc_maximum_offset = wt_get_batt_full_maximum_offset(pinfo);
		if (soc_maximum_offset < 0) {
			soc_maximum_offset = 0;
		}
		capacity_threshold1 = 40 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold2 = 50 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold3 = 60 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold4 = 70 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold5 = 75 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold6 = 77 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold7 = 78 + soc_maximum_offset * CHARGE_SOC_OFFSET;
		capacity_threshold8 = 79 + soc_maximum_offset * CHARGE_SOC_OFFSET;

		if (capacity_threshold5 > 95) {
			return -1;
		}
	}
#endif

	if (pinfo) {
		batt_charging_source = wt_get_charge_source(pinfo);
	}

	if ((disable_quick_charge)
		&& ((batt_charging_source == SEC_BATTERY_CABLE_PDIC_APDO)
		|| (batt_charging_source == SEC_BATTERY_CABLE_9V_TA)
		|| (batt_charging_source == SEC_BATTERY_CABLE_PDIC))) {
		if (is_protection_mode) {
			ui_time_to_full_min1 = 4200;
			ui_time_to_full_min2 = 3300;
			ui_time_to_full_min3 = 2160;
			ui_time_to_full_min4 = 1080;
			ui_time_to_full_min5 = 420;
			ui_time_to_full_min6 = 300;
			ui_time_to_full_min7 = 180;
			ui_time_to_full_min8 = 120;
		} else {
			ui_time_to_full_min1 = 4200;
			ui_time_to_full_min2 = 3300;
			ui_time_to_full_min3 = 2160;
			ui_time_to_full_min4 = 1200;
			ui_time_to_full_min5 = 540;
			ui_time_to_full_min6 = 360;
			ui_time_to_full_min7 = 240;
			ui_time_to_full_min8 = 180;
		}
	} else if (batt_charging_source == SEC_BATTERY_CABLE_PDIC_APDO) {
		if (is_protection_mode) {
			ui_time_to_full_min1 = 2160;
			ui_time_to_full_min2 = 1380;
			ui_time_to_full_min3 = 900;
			ui_time_to_full_min4 = 480;
			ui_time_to_full_min5 = 300;
			ui_time_to_full_min6 = 240;
			ui_time_to_full_min7 = 180;
			ui_time_to_full_min8 = 120;
		} else {
			ui_time_to_full_min1 = 2160;
			ui_time_to_full_min2 = 1740;
			ui_time_to_full_min3 = 1320;
			ui_time_to_full_min4 = 900;
			ui_time_to_full_min5 = 540;
			ui_time_to_full_min6 = 360;
			ui_time_to_full_min7 = 240;
			ui_time_to_full_min8 = 180;
		}
	} else {
		if ((batt_charging_source == SEC_BATTERY_CABLE_9V_TA)
			|| (batt_charging_source == SEC_BATTERY_CABLE_PDIC)) {
			if (is_protection_mode) {
				ui_time_to_full_min1 = 2760;
				ui_time_to_full_min2 = 2160;
				ui_time_to_full_min3 = 1500;
				ui_time_to_full_min4 = 660;
				ui_time_to_full_min5 = 360;
				ui_time_to_full_min6 = 240;
				ui_time_to_full_min7 = 180;
				ui_time_to_full_min8 = 120;
			} else {
				ui_time_to_full_min1 = 2760;
				ui_time_to_full_min2 = 2160;
				ui_time_to_full_min3 = 1560;
				ui_time_to_full_min4 = 900;
				ui_time_to_full_min5 = 540;
				ui_time_to_full_min6 = 360;
				ui_time_to_full_min7 = 240;
				ui_time_to_full_min8 = 180;
			}
		} else {
			ui_time_to_full_min1 = 4800;
			ui_time_to_full_min2 = 3900;
			ui_time_to_full_min3 = 2460;
			ui_time_to_full_min4 = 1380;
			ui_time_to_full_min5 = 660;
			ui_time_to_full_min6 = 420;
			ui_time_to_full_min7 = 300;
			ui_time_to_full_min8 = 180;
		}
	}
	pr_err("%s: ui_time_to_full_min1=%d, ui_time_to_full_min2=%d, ui_time_to_full_min3=%d, ui_time_to_full_min4=%d, ui_time_to_full_min5=%d\n",
		__func__, ui_time_to_full_min1, ui_time_to_full_min2,
		ui_time_to_full_min3, ui_time_to_full_min4,
		ui_time_to_full_min5);

	if ((ui_time_to_full_min1 == 0) || (ui_time_to_full_min2 == 0)
		|| (ui_time_to_full_min3 == 0) || (ui_time_to_full_min4 == 0)
		|| (ui_time_to_full_min5 == 0)) {
		return -1;
	}

	if (((capacity <= capacity_threshold1) && (ui_time_to_full <= ui_time_to_full_min1))
		|| ((capacity <= capacity_threshold2) && (ui_time_to_full <= ui_time_to_full_min2))
		|| ((capacity <= capacity_threshold3) && (ui_time_to_full <= ui_time_to_full_min3))
		|| ((capacity < capacity_threshold4) && (ui_time_to_full <= ui_time_to_full_min4))
		|| ((capacity < capacity_threshold5) && (ui_time_to_full <= ui_time_to_full_min5))
		|| ((capacity < capacity_threshold6) && (ui_time_to_full <= ui_time_to_full_min6))
		|| ((capacity < capacity_threshold7) && (ui_time_to_full <= ui_time_to_full_min7))
		|| ((capacity < capacity_threshold8) && (ui_time_to_full <= ui_time_to_full_min8))) {
		time_to_full_update_th = WT_INTERMAL_TIME_MAX;
	}
	return time_to_full_update_th;
}

static int wt_check_calculate_time_state(struct battery_data *data,
				struct mtk_charger *pinfo, int fgcurrent)
{
#if defined (CONFIG_W2_CHARGER_PRIVATE)
	int wt_calculate_time_state = CALCULATE_NONE_STATE;
	static int pre_bat_status = POWER_SUPPLY_STATUS_UNKNOWN;

	if (POWER_SUPPLY_STATUS_CHARGING == data->bat_status) {
		if ((pinfo->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) &&
			(pre_bat_status != POWER_SUPPLY_STATUS_CHARGING)) {
			wt_calculate_time_state = CALCULATE_INIT_STATE;
		} else {
			if (pinfo->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
				wt_calculate_time_state = CALCULATE_INVALID_STATE;
			else
				wt_calculate_time_state = CALCULATE_CHARGING_STATE;
		}
		if (pre_bat_status != POWER_SUPPLY_STATUS_CHARGING
			&& (fgcurrent > 10)) {
			init_avg_current(fgcurrent);
		}
	} else {
		if (POWER_SUPPLY_STATUS_FULL == data->bat_status) {
			wt_calculate_time_state = CALCULATE_FULL_STATE;
		} else {
			wt_calculate_time_state = CALCULATE_PLUG_OUT_STATE;
		}
		init_avg_current(-1);
	}
#else
	int wt_calculate_time_state = CALCULATE_NONE_STATE;
	static int pre_bat_status = POWER_SUPPLY_STATUS_UNKNOWN;
	static int pre_pd_type = MTK_PD_CONNECT_NONE;

	if (POWER_SUPPLY_STATUS_CHARGING == data->bat_status) {
		if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			if ((pre_bat_status != POWER_SUPPLY_STATUS_CHARGING)
				|| (pre_pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO)) {
				wt_calculate_time_state = CALCULATE_INIT_STATE;
			} else {
				wt_calculate_time_state = CALCULATE_CHARGING_STATE;
			}
		} else {
			if ((pinfo->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) &&
				(pre_bat_status != POWER_SUPPLY_STATUS_CHARGING)) {
				wt_calculate_time_state = CALCULATE_INIT_STATE;
			} else {
				wt_calculate_time_state = CALCULATE_CHARGING_STATE;
			}
		}
		if (pre_bat_status != POWER_SUPPLY_STATUS_CHARGING
			&& (fgcurrent > 10)) {
			init_avg_current(fgcurrent);
		}
	} else {
		if (POWER_SUPPLY_STATUS_FULL == data->bat_status) {
			wt_calculate_time_state = CALCULATE_FULL_STATE;
		} else {
			wt_calculate_time_state = CALCULATE_PLUG_OUT_STATE;
		}
		init_avg_current(-1);
	}
	pre_pd_type = pinfo->ta_type;
#endif
	pre_bat_status = data->bat_status;
	pr_err("%s: wt_calculate_time_state=%d\n", __func__, wt_calculate_time_state);
	return wt_calculate_time_state;
}

static int wt_recheck_calculate_time_state(int wt_initial_time_interval,
				int fgcurrent, int soc)
{
	int wt_calculate_time_state = CALCULATE_CHARGING_STATE;
	int capacity = soc;

	//no charging current
	if ((fgcurrent <= 10
		&& (wt_initial_time_interval > UPDATE_TO_FULL_INTERVAL_S))
		|| (capacity < 0)) {
		wt_calculate_time_state = CALCULATE_INVALID_STATE;
		init_avg_current(-1);
	}

	pr_err("%s: wt_calculate_time_state=%d\n", __func__, wt_calculate_time_state);
	return wt_calculate_time_state;
}

static int wt_recheck_afc_calculate_time_state(struct mtk_charger *pinfo,
				int wt_recheck_afc_start_time)
{
	int batt_charging_source = 0;
	bool is_recheck_afc = false;
	int real_time = 0;
	int wt_check_afc_time_interval = 0;

	if (pinfo == NULL)
		return -1;

	batt_charging_source = wt_get_charge_source(pinfo);

	if ((batt_charging_source != SEC_BATTERY_CABLE_9V_TA)
		&& (batt_charging_source != SEC_BATTERY_CABLE_TA)) {
		return 0;
	}

	if (batt_charging_source == SEC_BATTERY_CABLE_TA) {
		real_time = fulltime_get_sys_time();
		if (real_time >= wt_recheck_afc_start_time) {
			wt_check_afc_time_interval = real_time - wt_recheck_afc_start_time;
		}

		if (wt_check_afc_time_interval >= RECHECK_DCP_INTERVAL_S) {
			return 0;
		}
	}

	if (batt_charging_source == SEC_BATTERY_CABLE_9V_TA) {
		is_recheck_afc = true;
	} else {
		is_recheck_afc = false;
	}

	pr_err("%s: is_recheck_afc=%d\n", __func__, is_recheck_afc);
	if (is_recheck_afc) {
		return CALCULATE_INIT_STATE;
	}

	return -1;
}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
static int wt_check_protection_calculate_time_state(struct mtk_charger *pinfo)
{
	static int old_batt_mode = 0;
	int batt_mode = 0;
	bool is_mode_changed = false;

	if (pinfo == NULL)
		return -1;

	batt_mode = pinfo->batt_full_capacity;
	if (batt_mode != old_batt_mode) {
		if (((batt_mode > POWER_SUPPLY_CAPACITY_100)
			&& (batt_mode <= POWER_SUPPLY_CAPACITY_80_OFFCHARGING))
			&& ((old_batt_mode > POWER_SUPPLY_CAPACITY_100)
			&& (old_batt_mode <= POWER_SUPPLY_CAPACITY_80_OFFCHARGING))) {
			is_mode_changed = false;
		} else {
			is_mode_changed = true;
		}
	}

	old_batt_mode = batt_mode;
	pr_err("%s: is_mode_changed=%d\n", __func__, is_mode_changed);
	if (is_mode_changed) {
		return CALCULATE_INIT_STATE;
	}

	return -1;
}
#endif

static int wt_check_hv_disable_calculate_time_state(struct mtk_charger *pinfo)
{
	static bool old_batt_charge_mode = false;
	int batt_charge_mode = pinfo->disable_quick_charge;
	bool is_mode_changed = false;

	if (pinfo == NULL)
		return -1;

	if (batt_charge_mode ^ old_batt_charge_mode) {
		is_mode_changed = true;
	} else {
		is_mode_changed = false;
	}

	old_batt_charge_mode = batt_charge_mode;
	pr_err("%s: is_mode_changed=%d\n", __func__, is_mode_changed);
	if (is_mode_changed) {
		return CALCULATE_INIT_STATE;
	}

	return -1;
}

static int get_time_to_charge_full(struct battery_data *data)
{
	int magic_current, real_time;
	int time_to_charge_full = 0xff;
	int capacity = data->bat_capacity;
	int fgcurrent = 0;
	int remain_mah = 0;
	static struct mtk_battery_manager *bm;
	struct mtk_charger *pinfo;
	struct power_supply *psy;
	static int pre_real_time = 0, pre_magic_current = 0, pre_remain_mah = 0;
	static bool magic_current_changflg = true;
	static int wt_initial_time_interval = 0;
	static int wt_recheck_afc_start_time = 0;
	static int pre_charge_plug_time = 0;
	int wt_calculate_time_state = CALCULATE_NONE_STATE;
	static int ui_time_to_full = 0;
	static int old_ui_time_to_full = 0;
	static int raw_time_to_full = 0;
	//static int old_raw_time_to_full = 0;
	static int initial_time_to_full = 0;
	static bool is_initial_flag = true;
	int wt_time_now = 0;
	static int wt_time_old = 0;
	int wt_time_interval = 0;
	static int wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
	static int time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
	int time_critical_update_th = WT_INTERMAL_TIME_NORMAL;
	int wt_time_critical_offset = WT_INTERMAL_TIME_NORMAL;
	static bool is_time_need_update = false;
	static int wt_compensation_state = 0;
	static int ui_raw_time_diff = 0;
	//static int old_ui_raw_time_diff = 0;
	static bool is_need_recheck_afc = true;
	static bool is_first_check_afc = true;
	int wt_recheck_afc = 0;
	int critical_soc = 0;
	int soc_maximum_offset = 0;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL) {
		pr_err("[%s]psy is not rdy\n", __func__);
		return -1;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		pr_err("[%s]mtk_gauge is not rdy\n", __func__);
		return -1;
	}

	if (bm == NULL)
		bm = get_mtk_battery_manager();

	if (bm == NULL) {
		pr_err("[%s]bm is not rdy\n", __func__);
		return -1;
	}

	fgcurrent = wt_get_current_now(bm);
	//pinfo->ta_type= adapter_dev_get_property(pinfo->select_adapter, CAP_TYPE);

	pr_err("%s: capacity=%d, fgcurrent=%d,bat_cycle=%d,status=%d\n",
		__func__, capacity, fgcurrent, wt_get_batt_cycle_cnt(bm), data->bat_status);

	wt_calculate_time_state = wt_check_calculate_time_state(data, pinfo,
								fgcurrent);
	if (wt_calculate_time_state == CALCULATE_CHARGING_STATE) {
		wt_calculate_time_state =
			wt_recheck_calculate_time_state(wt_initial_time_interval,
			fgcurrent, capacity);
	}

	remain_mah = wt_get_battery_remain_mah(bm, pinfo, capacity);
	if (remain_mah < 0) {
		pr_err("%s: Error: The remaining capacity is invalid\n", __func__);
		remain_mah = 0;
		wt_calculate_time_state = CALCULATE_INVALID_STATE;
	}

	pr_err("%s: is_need_recheck_afc=%d\n", __func__, is_need_recheck_afc);
	if (is_need_recheck_afc
		&& ((wt_calculate_time_state == CALCULATE_INIT_STATE)
		|| (wt_calculate_time_state == CALCULATE_CHARGING_STATE))) {
		if (is_first_check_afc) {
			wt_recheck_afc_start_time = fulltime_get_sys_time();
			is_first_check_afc = false;
		}
		wt_recheck_afc = wt_recheck_afc_calculate_time_state(pinfo,
			wt_recheck_afc_start_time);
		if (wt_recheck_afc > 0) {
			wt_calculate_time_state = CALCULATE_INIT_STATE;
			is_need_recheck_afc = false;
		} else if (wt_recheck_afc == 0) {
			is_need_recheck_afc = false;
		}
		pr_err("%s: recheck afc: wt_calculate_time_state=%d\n",
			__func__, wt_calculate_time_state);
	}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	if ((wt_calculate_time_state == CALCULATE_INIT_STATE)
		|| (wt_calculate_time_state == CALCULATE_CHARGING_STATE)) {
		if (wt_check_protection_calculate_time_state(pinfo) > 0) {
			wt_calculate_time_state = CALCULATE_INIT_STATE;
		}
		pr_err("%s: check protection: wt_calculate_time_state=%d\n",
			__func__, wt_calculate_time_state);
	}
#endif

	if ((wt_calculate_time_state == CALCULATE_INIT_STATE)
		|| (wt_calculate_time_state == CALCULATE_CHARGING_STATE)) {
		if (wt_check_hv_disable_calculate_time_state(pinfo) > 0) {
			wt_calculate_time_state = CALCULATE_INIT_STATE;
		}
		pr_err("%s: check hv_disable: wt_calculate_time_state=%d\n",
			__func__, wt_calculate_time_state);
	}

	switch (wt_calculate_time_state) {
		case CALCULATE_INIT_STATE:
			wt_initial_time_interval = 0;
			is_initial_flag = true;
			raw_time_to_full = 0;
			ui_time_to_full = 0;
			old_ui_time_to_full = ui_time_to_full;
			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
			time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
			wt_time_now = 0;
			wt_time_old = 0;
			is_time_need_update = false;
			break;
		case CALCULATE_FULL_STATE:
			time_to_charge_full = 0;
			wt_initial_time_interval = 0;
			is_initial_flag = false;
			break;
		case CALCULATE_PLUG_OUT_STATE:
			time_to_charge_full = -1;
			wt_initial_time_interval = 0;
			is_initial_flag = false;
			is_need_recheck_afc = true;
			is_first_check_afc = true;
			wt_recheck_afc_start_time = 0;
			break;
		case CALCULATE_INVALID_STATE:
			time_to_charge_full = -1;
			is_initial_flag = true;
			break;
		default:
			break;
	}

	if ((wt_calculate_time_state == CALCULATE_FULL_STATE)
		|| (wt_calculate_time_state == CALCULATE_PLUG_OUT_STATE)
		|| (wt_calculate_time_state == CALCULATE_INVALID_STATE)) {
		raw_time_to_full = time_to_charge_full;
		ui_time_to_full = time_to_charge_full;
		old_ui_time_to_full = ui_time_to_full;
		wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
		time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
		wt_time_now = 0;
		wt_time_old = 0;
		is_time_need_update = false;
		return time_to_charge_full;
	}

	fgcurrent = calculate_avg_current(fgcurrent);
	pr_err("%s: avg_current=%d\n", __func__, fgcurrent);

	if (is_initial_flag) {
		pre_charge_plug_time = fulltime_get_sys_time();
	}

	real_time = fulltime_get_sys_time();
	if (pre_charge_plug_time > real_time) {
		pre_charge_plug_time = real_time;
	}
	wt_initial_time_interval = real_time - pre_charge_plug_time;

	if (wt_get_charge_source(pinfo) == SEC_BATTERY_CABLE_PDIC_APDO
		&& (!pinfo->disable_quick_charge)) {
		magic_current = select_apdo_magic_current(fgcurrent, capacity, wt_initial_time_interval);
	}
	else {
		magic_current = select_basic_magic_current(fgcurrent, capacity, pinfo, wt_initial_time_interval);
	}

	if ((pre_magic_current == magic_current) && (magic_current_changflg)) {
		magic_current_changflg = false;
		pre_real_time = fulltime_get_sys_time();
	} else if ((pre_magic_current != magic_current) || (pre_remain_mah != remain_mah)) {
		magic_current_changflg = true;
	}
	pr_err("%s:magic_current=%d,%d,%d,chr_type=%d,real_time=%d,%d,%d,pd_type=%d\n", __func__,
		pre_magic_current, magic_current, magic_current_changflg, pinfo->chr_type,
		real_time, pre_real_time, pre_charge_plug_time, pinfo->ta_type);

	pre_magic_current = magic_current;
	if (magic_current != 0) {
		time_to_charge_full = remain_mah * 3600 / magic_current; //second
	} else {
		time_to_charge_full = -1;
		return time_to_charge_full;
	}

	if ((time_to_charge_full > (real_time - pre_real_time))
		&& (time_to_charge_full > 0)
		&& (pre_remain_mah == remain_mah)
		&& (magic_current_changflg == false)
		&& (real_time - pre_real_time > 60)
		&& (pre_real_time > 0)) {
		time_to_charge_full = remain_mah * 3600 / magic_current - (real_time - pre_real_time);
	}
	pre_remain_mah = remain_mah;

	pr_err("%s: is_initial_flag=%d,wt_initial_time_interval=%d\n", __func__, is_initial_flag, wt_initial_time_interval);
	if (is_initial_flag && (wt_initial_time_interval < UPDATE_TO_FULL_INTERVAL_S)
		&& (magic_current != 0)) {
		initial_time_to_full = remain_mah * 3600 / magic_current;
		raw_time_to_full = initial_time_to_full;
		ui_time_to_full = initial_time_to_full;
		old_ui_time_to_full = initial_time_to_full;
		is_initial_flag = false;
		wt_time_now = fulltime_get_sys_time();
		wt_time_old = wt_time_now;
	} else {
		if (magic_current != 0) {
			raw_time_to_full = remain_mah * 3600 / magic_current; //second
			wt_time_now = fulltime_get_sys_time();
		} else {
			raw_time_to_full = -1;
			ui_time_to_full = -1;
			time_to_charge_full = ui_time_to_full;
			old_ui_time_to_full = ui_time_to_full;
			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
			time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
			wt_time_now = 0;
			wt_time_old = 0;
			is_time_need_update = false;
			return time_to_charge_full;
		}
	}

	if ((ui_time_to_full >= 0) && (raw_time_to_full >= 0)) {
	if (wt_time_now >= wt_time_old) {
		wt_time_interval = wt_time_now - wt_time_old;
	} else {
		wt_time_interval = -1;
		pr_err("%s: Invalid. The time reduces\n", __func__);
	}

	//old_ui_raw_time_diff = raw_time_to_full - old_ui_time_to_full;

	if (ui_time_to_full == raw_time_to_full) {
		ui_raw_time_diff = 0;
		wt_compensation_state = COMPENSATION_LEVEL_REDUCE_NORMAL;
	} else if (ui_time_to_full < raw_time_to_full) {
		ui_raw_time_diff = raw_time_to_full - ui_time_to_full;
		wt_compensation_state = COMPENSATION_LEVEL_REDUCE_SLOW;
	} else if (ui_time_to_full > raw_time_to_full) {
		ui_raw_time_diff = ui_time_to_full - raw_time_to_full;
		wt_compensation_state = COMPENSATION_LEVEL_REDUCE_QUICK;
	}

	pr_err("%s: ui_time=%d, raw_time=%d, compensation_state=%d\n",
		__func__, ui_time_to_full, raw_time_to_full, wt_compensation_state);

	switch (wt_compensation_state) {
		case COMPENSATION_LEVEL_REDUCE_NORMAL:
			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
			time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
			break;
		case COMPENSATION_LEVEL_REDUCE_SLOW:
			if (time_to_full_update_th == WT_INTERMAL_TIME_MAX) {
				time_to_full_update_th = WT_INTERMAL_TIME_HIGH2;
			}

			time_to_full_update_th = wt_get_slow_update_th(wt_initial_time_interval,
				time_to_full_update_th, capacity);

			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;

			critical_soc = 95;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
			if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
				critical_soc = 75;
				soc_maximum_offset = wt_get_batt_full_maximum_offset(pinfo);
				if (soc_maximum_offset > 0) {
					critical_soc += soc_maximum_offset * CHARGE_SOC_OFFSET;
				}
				if (critical_soc > 95) {
					critical_soc = 95;
				}
			}
#endif
			if (capacity >= critical_soc) {
				time_critical_update_th = wt_check_slow_critical_update_th(ui_raw_time_diff,
				ui_time_to_full, capacity, critical_soc);
				if (time_critical_update_th > 0) {
					time_to_full_update_th = time_critical_update_th;
				}
			}

			if (wt_check_min_remain_time(pinfo, ui_time_to_full, capacity) > 0) {
				time_to_full_update_th = WT_INTERMAL_TIME_MAX;
			}

			break;
		case COMPENSATION_LEVEL_REDUCE_QUICK:
			if (time_to_full_update_th == WT_INTERMAL_TIME_MAX) {
				time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
			}

			time_to_full_update_th = wt_get_quick_update_th(wt_initial_time_interval,
				time_to_full_update_th, capacity);

			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;

			if (time_to_full_update_th <= WT_INTERMAL_TIME_LOW2) {
				wt_time_to_full_offset = WT_INTERMAL_TIME_HIGH2;
			}

			critical_soc = 97;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
			if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
				critical_soc = 76;
				soc_maximum_offset = wt_get_batt_full_maximum_offset(pinfo);
				if (soc_maximum_offset > 0) {
					critical_soc += soc_maximum_offset * CHARGE_SOC_OFFSET;
				}
				if (critical_soc > 97) {
					critical_soc = 97;
				}
			}
#endif
			if (capacity >= critical_soc) {
				wt_time_critical_offset = wt_check_quick_critical_offset(ui_raw_time_diff,
				raw_time_to_full, capacity, critical_soc);
				if (wt_time_critical_offset > 0) {
					wt_time_to_full_offset = wt_time_critical_offset;
				}
			}

			break;
		default:
			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
			time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
			break;
	}

	if ((time_to_full_update_th < WT_INTERMAL_TIME_MAX) && (wt_time_interval >= time_to_full_update_th)) {
		is_time_need_update = true;
	} else {
		is_time_need_update = false;
	}

	pr_err("%s: time_interval=%d, time_th=%d, wt_time_offset=%d, is_update=%d\n",
		__func__, wt_time_interval, time_to_full_update_th,
		wt_time_to_full_offset, is_time_need_update);

	if (is_time_need_update) {
		if (ui_time_to_full >= (wt_time_to_full_offset + WT_INTERMAL_TIME_NORMAL)) {
			ui_time_to_full = ui_time_to_full - wt_time_to_full_offset;
		} else {
			ui_time_to_full = WT_INTERMAL_TIME_NORMAL;
		}
		is_time_need_update = false;
		wt_time_old = wt_time_now;
	}

	if (raw_time_to_full == 0) {
		ui_time_to_full = 0;
		wt_time_old = wt_time_now;
		is_time_need_update = false;
	}

	//old_raw_time_to_full = raw_time_to_full;

	pr_err("%s: ui_time=%d, old_ui_time=%d\n",
		__func__, ui_time_to_full, old_ui_time_to_full);

	//wt_time_interval < 0, using old method when get time error. This situation should not occur, under normal circumstances
	if (wt_time_interval >= 0) {
		if (ui_time_to_full <= old_ui_time_to_full) {
			time_to_charge_full = ui_time_to_full;
			old_ui_time_to_full = ui_time_to_full;
		} else {
			pr_err("%s: Invalid. The remaining duration increases\n", __func__);
			//time_to_charge_full = old_ui_time_to_full;
		}
	} else {
			raw_time_to_full = -1;
			ui_time_to_full = -1;
			old_ui_time_to_full = ui_time_to_full;
			wt_time_to_full_offset = WT_INTERMAL_TIME_NORMAL;
			time_to_full_update_th = WT_INTERMAL_TIME_NORMAL;
			wt_time_now = 0;
			wt_time_old = 0;
			is_time_need_update = false;
	}
	}

	return time_to_charge_full;
}
#endif

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static void wt_update_battery_health(struct battery_data *bs_data)
{
	struct mtk_charger *pinfo = NULL;
	struct power_supply *charger_psy = NULL;

	if (bs_data == NULL) {
		pr_err("[%s] bs_data == NULL\n", __func__);
		return;
	}
	bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD;

	charger_psy = power_supply_get_by_name("mtk-master-charger");
	if (charger_psy == NULL) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(charger_psy);
	if (pinfo == NULL) {
		pr_err("[%s]charge info is not rdy\n", __func__);
	} else {
		if(pinfo->notify_code & CHG_BAT_OT_STATUS) {
			bs_data->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else if(pinfo->notify_code & CHG_BAT_LT_STATUS) {
			bs_data->bat_health = POWER_SUPPLY_HEALTH_COLD;
		} else {
			bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		}
	}

	pr_err("[%s] bat_health=%d\n", __func__, bs_data->bat_health);
}
#endif

static int battery_get_chg_status(void)
{
	struct power_supply *psy = power_supply_get_by_name("mtk-master-charger");
	struct mtk_charger *pinfo = NULL;

	if (psy == NULL) {
		pr_err("[%s] psy == NULL\n", __func__);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		pr_err("[%s]pinfo is not rdy\n", __func__);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
		if (wt_batt_full_capacity_check_for_cp() < 0) {
			pinfo->batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}
#endif
	pr_err("%s status:%d\n", __func__, pinfo->batt_status);
	return pinfo->batt_status;
}

static int bs_psy_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0, qmax = 0, cycle = 0;
	int curr_now = 0;
	int curr_avg = 0;
#if !defined (CONFIG_N28_CHARGER_PRIVATE) && !defined (CONFIG_W2_CHARGER_PRIVATE)
	int remain_ui = 0, remain_mah = 0;
#endif
	int time_to_full = 0;
	int q_max_uah = 0;
	int volt_now = 0;
	int count = 0;
	int temp = 0;
	struct mtk_battery_manager *bm;
	struct battery_data *bs_data;

	bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);
	bs_data = &bm->bs_data;

	/* gauge_get_property should check return value */
	/* to avoid i2c suspend but query by other module */
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		//val->intval = bs_data->bat_status;
		val->intval = battery_get_chg_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
		wt_update_battery_health(bs_data);
#endif
		val->intval = bs_data->bat_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bs_data->bat_present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = bs_data->bat_technology;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT: //sum(cycle * qmax) / sum(qmax)
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				cycle += (bm->gm1->bat_cycle + 1) *
					bm_update_psy_property(bm->gm1, QMAX_DESIGN);
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				cycle += (bm->gm2->bat_cycle + 1) *
					bm_update_psy_property(bm->gm2, QMAX_DESIGN);
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);
			}
		if (qmax != 0)
			val->intval = cycle / qmax;
		break;
	case POWER_SUPPLY_PROP_CAPACITY: //sum(uisoc)
		/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
		/* 6= ATE_factory_boot */
		if (bm->bootmode == 1 || bm->bootmode == 4
			|| bm->bootmode == 5 || bm->bootmode == 6) {
			val->intval = 75;
			break;
		}
#if defined (CONFIG_W2_CHARGER_PRIVATE)
		if ((bm->bootmode == 2) && (bs_data->bat_capacity == -1)) {
			bs_data->bat_capacity = 75;
			val->intval = bs_data->bat_capacity;
			pr_err("%s: RECOVERY!!!\n", __func__);
			break;
		}
#endif

		if (bm->gm1->fixed_uisoc != 0xffff)
			val->intval = bm->gm1->fixed_uisoc;
		else
			val->intval = bs_data->bat_capacity;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:

		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				curr_now += bm_update_psy_property(bm->gm1, CURRENT_NOW);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				curr_now += bm_update_psy_property(bm->gm2, CURRENT_NOW);

		val->intval = curr_now * 100;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:

		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				curr_avg += bm_update_psy_property(bm->gm1, CURRENT_AVG);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				curr_avg += bm_update_psy_property(bm->gm2, CURRENT_AVG);

		val->intval = curr_avg * 100;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:

		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);

		val->intval = qmax * 100;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:

		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);

		val->intval = bs_data->bat_capacity * qmax;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:

		count = 0;
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				volt_now += bm_update_psy_property(bm->gm1, VOLTAGE_NOW);
				count += 1;
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				volt_now += bm_update_psy_property(bm->gm2, VOLTAGE_NOW);
				count += 1;
			}
		if (count != 0) {
			val->intval = volt_now / count * 1000;
			batt_vol_value = val->intval;
		}
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_TEMP:

		count = 0;
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				temp += bm_update_psy_property(bm->gm1, TEMP);
				count += 1;
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				temp += bm_update_psy_property(bm->gm2, TEMP);
				count += 1;
			}
		if (count != 0) {
			val->intval = temp / count * 10;
			batt_temp_value = val->intval;
		}
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = check_cap_level(bs_data->bat_capacity);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
		time_to_full = get_time_to_charge_full(bs_data);

		pr_err("time_to_full: %d\n", time_to_full);
		val->intval = time_to_full;
#else
		/* full or unknown must return 0 */
		ret = check_cap_level(bs_data->bat_capacity);
		if ((ret == POWER_SUPPLY_CAPACITY_LEVEL_FULL) ||
			(ret == POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN)) {
			val->intval = 0;
			break;
		}

		remain_ui = 100 - bs_data->bat_capacity;
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				curr_avg += bm_update_psy_property(bm->gm1, CURRENT_AVG);
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				curr_avg += bm_update_psy_property(bm->gm2, CURRENT_AVG);
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);
			}

		remain_mah = remain_ui * qmax / 10;
		if (curr_avg != 0)
			time_to_full = remain_mah * 3600 / curr_avg / 10;

		val->intval = abs(time_to_full);
#endif
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (check_cap_level(bs_data->bat_capacity) ==
			POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN) {
			val->intval = 0;
			break;
		}

		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out)
				qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);

		q_max_uah = qmax * 100;
		if (q_max_uah <= 100000) {
			pr_debug("%s gm_no:%d, q_max:%d q_max_uah:%d\n",
				__func__, bm->gm_no, qmax, q_max_uah);
			q_max_uah = 100001;
		}
		val->intval = q_max_uah;

		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (IS_ERR_OR_NULL(bs_data->chg_psy)) {
			bs_data->chg_psy = devm_power_supply_get_by_phandle(
				bm->dev, "charger");
			pr_err("%s retry to get chg_psy\n", __func__);
		}
		if (IS_ERR_OR_NULL(bs_data->chg_psy)) {
			pr_err("%s Couldn't get chg_psy\n", __func__);
			ret = 4350;
		} else {
			ret = power_supply_get_property(bs_data->chg_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
			if (ret < 0) {
				pr_err("get CV property fail\n");
				ret = 4350;
			}
		}
		break;


	default:
		ret = -EINVAL;
		break;
		}

	pr_err("%s psp:%d ret:%d val:%d", __func__, psp, ret, val->intval);

	return ret;
}

static int bs_psy_set_property(struct power_supply *psy,
	enum power_supply_property psp,
	const union power_supply_propval *val)
{
	int ret = 0;
	struct mtk_battery_manager *bm;
	int count = 0;

	bm = (struct mtk_battery_manager *)power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (val->intval > 0)
			bm_send_cmd(bm, MANAGER_DYNAMIC_CV, val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bm->gm1 != NULL)
			if(!bm->gm1->bat_plug_out) {
				battery_set_property(bm->gm1, BAT_PROP_TEMPERATURE, val->intval);
				count += 1;
				pr_err("%s gm1 count:%d val:%d", __func__, count, val->intval);
			}
		if (bm->gm2 != NULL)
			if(!bm->gm2->bat_plug_out) {
				battery_set_property(bm->gm2, BAT_PROP_TEMPERATURE, val->intval);
				count += 1;
				pr_err("%s gm2 count:%d val:%d", __func__, count, val->intval);
			}
		if (count != 0) {
			batt_temp_value = val->intval / count * 10;
		}
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
		}

	pr_err("%s psp:%d ret:%d val:%d",
		__func__, psp, ret, val->intval);

	return ret;
}

static void mtk_battery_external_power_changed(struct power_supply *psy)
{
	struct mtk_battery_manager *bm;
	struct battery_data *bs_data;
	union power_supply_propval online = {0}, status = {0}, vbat0 = {0};
	union power_supply_propval prop_type = {0};
	int cur_chr_type = 0, old_vbat0 = 0;

	struct power_supply *chg_psy = NULL;
	struct power_supply *dv2_chg_psy = NULL;
	int ret = 0;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	struct mtk_charger *pinfo = NULL;
	struct power_supply *bat_psy = NULL;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (IS_ERR_OR_NULL(bat_psy)) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]charge info is not rdy\n", __func__);
		return;
	}
#endif

	bm = psy->drv_data;
	bs_data = &bm->bs_data;
	chg_psy = bs_data->chg_psy;

	if (bm->gm1->is_probe_done == false) {
		pr_err("[%s] gm_no:%d battery probe is not rdy:%d\n",
			__func__, bm->gm_no, bm->gm1->is_probe_done);
		return;
	}

	if (bm->gm_no==2) {
		if (bm->gm2->is_probe_done == false) {
			pr_err("[%s] gm_no:%d battery probe is not rdy:%d\n",
				__func__, bm->gm_no, bm->gm2->is_probe_done);
			return;
		}
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(bm->dev,
						       "charger");
		pr_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		ret |= power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);

		ret |= power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);

		ret |= power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);

		if (ret < 0)
			pr_debug("%s ret: %d\n", __func__, ret);

		if (!online.intval) {
			bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
			bs_data->old_bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
				bs_data->bat_status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
				bs_data->old_bat_status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;

				dv2_chg_psy = power_supply_get_by_name("mtk-mst-div-chg");
				if (!IS_ERR_OR_NULL(dv2_chg_psy)) {
					ret = power_supply_get_property(dv2_chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &online);
					if (online.intval) {
						bs_data->bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
						bs_data->old_bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
						status.intval =
							POWER_SUPPLY_STATUS_CHARGING;
					}
					power_supply_put(dv2_chg_psy);
				}
			} else {
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
					if ((status.intval == POWER_SUPPLY_STATUS_FULL)
						&& pinfo->is_chg_done
						&& (pinfo->batt_soc_rechg == 1)
						&& (pinfo->batt_full_capacity == 1)
						&& (bs_data->bat_status == POWER_SUPPLY_STATUS_FULL)) {
						bs_data->bat_status = POWER_SUPPLY_STATUS_FULL;
						bs_data->old_bat_status = POWER_SUPPLY_STATUS_FULL;
					} else {
						bs_data->bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
						bs_data->old_bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
					}
#else
					bs_data->bat_status =
						POWER_SUPPLY_STATUS_CHARGING;
					bs_data->old_bat_status =
						POWER_SUPPLY_STATUS_CHARGING;
#endif
				bs_data->bat_status =
					POWER_SUPPLY_STATUS_CHARGING;
				bs_data->old_bat_status =
					POWER_SUPPLY_STATUS_CHARGING;
			}
			bm_send_cmd(bm, MANAGER_SW_BAT_CYCLE_ACCU, 0);
		}

		//bs_data->old_bat_status = bs_data->bat_status;

		if (status.intval == POWER_SUPPLY_STATUS_FULL
			&& bm->b_EOC != true) {
			pr_err("POWER_SUPPLY_STATUS_FULL, EOC\n");
			gauge_get_int_property(bm->gm1, GAUGE_PROP_BAT_EOC);
			bm_send_cmd(bm, MANAGER_NOTIFY_CHR_FULL, 0);
			pr_err("GAUGE_PROP_BAT_EOC done\n");
			bm->b_EOC = true;
		} else{
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
			if ((status.intval != POWER_SUPPLY_STATUS_FULL)
				|| (pinfo->batt_soc_rechg != 1)
				|| (pinfo->batt_full_capacity != 1)
				|| (!pinfo->is_chg_done)) {
				bm->b_EOC = false;
			}
#else
			bm->b_EOC = false;
#endif
		}

		battery_update(bm);

		/* check charger type */
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop_type);

		/* plug in out */
		cur_chr_type = prop_type.intval;

		if (cur_chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			if (bm->chr_type != POWER_SUPPLY_TYPE_UNKNOWN)
				pr_err("%s chr plug out\n", __func__);
		} else {
			if (bm->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
				bm_send_cmd(bm, MANAGER_WAKE_UP_ALGO, FG_INTR_CHARGER_IN);
		}

		if (bm->gm1->vbat0_flag != vbat0.intval) {
			old_vbat0 = bm->gm1->vbat0_flag;
			bm->gm1->vbat0_flag = vbat0.intval;
			if (bm->gm_no == 2)
				bm->gm2->vbat0_flag = vbat0.intval;
			bm_send_cmd(bm, MANAGER_WAKE_UP_ALGO, FG_INTR_NAG_C_DLTV);
			pr_err("fuelgauge NAFG for calibration,vbat0[o:%d n:%d]\n",
				old_vbat0, vbat0.intval);
		}
	}

	pr_err("%s event, name:%s online:%d, status:%d, EOC:%d, cur_chr_type:%d old:%d, vbat0:[o:%d n:%d]\n",
		__func__, psy->desc->name, online.intval, status.intval,
		bm->b_EOC, cur_chr_type, bm->chr_type,
		old_vbat0, vbat0.intval);

	bm->chr_type = cur_chr_type;
}

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
static ssize_t stop_charge_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo;
	struct power_supply *psy;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL) {
		pr_err("[%s]psy is not rdy\n", __func__);
		return -1;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		pr_err("[%s]mtk_gauge is not rdy\n", __func__);
		return -1;
	}else{
		charger_manager_disable_charging_new(pinfo, 1);
	}
	return sprintf(buf, "chr=0\n");
}
static DEVICE_ATTR_RO(stop_charge);

static ssize_t start_charge_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo;
	struct power_supply *psy;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL) {
		pr_err("[%s]psy is not rdy\n", __func__);
		return -1;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		pr_err("[%s]mtk_gauge is not rdy\n", __func__);
		return -1;
	}else{
		charger_manager_disable_charging_new(pinfo, 0);
	}
	return sprintf(buf, "chr=1\n");
}
static DEVICE_ATTR_RO(start_charge);

static ssize_t batt_current_ua_now_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_current(pinfo) * 1000;
	}
	return sprintf(buf, "%d\n",ret);
}
static DEVICE_ATTR_RO(batt_current_ua_now);

static ssize_t batt_temp_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]show_batt_temp is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_temperature(pinfo) * 10;
		if(ret == 0) {
			pr_err("[%s]get batt_temp null from chg intf\n", __func__);
			ret = batt_temp_value;
		}
	}
	pr_err("[%s]get batt_temp:%d,%d\n", __func__, batt_temp_value, ret);
	return sprintf(buf, "%d\n",ret);
}
static ssize_t batt_temp_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	signed int temp = 0;
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;
	int ret = 0;
	union power_supply_propval batt_temp = {0};

	if (kstrtoint(buf, 10, &temp) == 0) {
		bat_psy = power_supply_get_by_name("mtk-master-charger");
		if (bat_psy == NULL)
			return -ENODEV;

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
		if (pinfo == NULL) {
			pr_err("[%s]store_batt_temp is not rdy\n", __func__);
			return -1;
		} else {
			batt_temp.intval = temp;
			ret = set_battery_temperature(pinfo, batt_temp);
			if(ret < 0) {
				pr_err("[%s]set batt_temp null from chg intf\n", __func__);
			}
		}
		pr_err("[%s]set batt_temp:%d,%d\n", __func__, batt_temp_value, batt_temp.intval);
	}
	return size;
}
static DEVICE_ATTR(batt_temp, 0664, batt_temp_show, batt_temp_store);

static ssize_t show_batt_type(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", str_batt_type);
}

static ssize_t store_batt_type(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int i = 0;

	if (buf != NULL && size != 0) {
		pr_err("[%s] buf is %s\n", __func__, buf);
		memset(str_batt_type, 0, 64);
		for (i = 0; i < size; ++i) {
			str_batt_type[i] = buf[i];
		}
		str_batt_type[i+1] = '\0';
		pr_err("str_batt_type:%s\n", str_batt_type);
	}

	return size;
}
static DEVICE_ATTR(batt_type, 0664, show_batt_type, store_batt_type);

static ssize_t hv_charger_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0, value = 0;
	struct chg_alg_device *alg;
	int i;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]hv_charger_status is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_charger_type(pinfo);
	}
	if(ret != POWER_SUPPLY_TYPE_UNKNOWN) {
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = pinfo->alg[i];
			if (alg == NULL)
				continue;
#ifdef CONFIG_AFC_CHARGER
#if defined (CONFIG_W2_CHARGER_PRIVATE)
			if ((adapter_is_support_pd_pps(pinfo)) || afc_get_is_connect(pinfo) ||
				(pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30))
				ret = AFC_9V_OR_15W;
			else
				ret = NORMAL_TA;
#else
			if (adapter_is_support_pd_pps(pinfo))
				ret = SFC_25W;
			else if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || afc_get_is_connect(pinfo))
				ret = AFC_9V_OR_15W;
			else
				ret = NORMAL_TA;
#endif
#else
			if (chg_alg_is_algo_running(alg)) {
				ret = AFC_9V_OR_15W;
				if (adapter_is_support_pd_pps(pinfo))
					ret = SFC_25W;
			}
			else
				ret = NORMAL_TA;
#endif
		value |= ret;
		pr_err("[%s]:%d,%d,%d\n", __func__, i, ret, value);

		}
	}
	if (pinfo->disable_quick_charge) {
		value = NORMAL_TA;
	}
	return sprintf(buf, "%d\n",value);
}
static DEVICE_ATTR_RO(hv_charger_status);

static ssize_t new_charge_type_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_current(pinfo);
	}
	pr_err("[%s] ret=%d\n", __func__, ret);
	if(ret > 1000)
		return sprintf(buf, "Fast\n");
	else
		return sprintf(buf, "Slow\n");
}
static DEVICE_ATTR_RO(new_charge_type);

static ssize_t resistance_id_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	static struct mtk_battery_manager *bm;

	if (bm == NULL)
		bm = get_mtk_battery_manager();
	return sprintf(buf, "%d\n", bm->gm1->battery_id);
}
static DEVICE_ATTR_RO(resistance_id);

static ssize_t batt_vol_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_voltage(pinfo) * 1000;
		if(ret == 0) {
			pr_err("[%s]get batt_vol null from chg intf\n", __func__);
			ret = batt_vol_value;
		}
	}
	return sprintf(buf, "%d\n",ret);
}
static DEVICE_ATTR_RO(batt_vol);

static ssize_t show_set_battery_cycle(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	return sprintf(buf, "%d\n", temp_cycle);
}

static ssize_t store_set_battery_cycle(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		temp_cycle = temp;
	}
	pr_err("[%s] temp_cycle=%d\n", __func__, temp_cycle);

	return size;
}
static DEVICE_ATTR(set_battery_cycle, 0664, show_set_battery_cycle,
		   store_set_battery_cycle);

static ssize_t battery_cycle_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	static struct mtk_battery_manager *bm;
	int batt_cycle_cnt = 0;
	int qmax = 0, cycle = 0;

	if (bm == NULL)
		bm = get_mtk_battery_manager();

	if (bm->gm1 != NULL)
		if(!bm->gm1->bat_plug_out) {
			cycle += (bm->gm1->bat_cycle + 1) *
				bm_update_psy_property(bm->gm1, QMAX_DESIGN);
			qmax += bm_update_psy_property(bm->gm1, QMAX_DESIGN);
		}
	if (bm->gm2 != NULL)
		if(!bm->gm2->bat_plug_out) {
			cycle += (bm->gm2->bat_cycle + 1) *
				bm_update_psy_property(bm->gm2, QMAX_DESIGN);
			qmax += bm_update_psy_property(bm->gm2, QMAX_DESIGN);
		}
	if (qmax != 0)
		batt_cycle_cnt = cycle / qmax;
	pr_err("[%s] batt_cycle_cnt=%d\n", __func__, batt_cycle_cnt);

	return sprintf(buf, "%d\n", batt_cycle_cnt);
}
static DEVICE_ATTR_RO(battery_cycle);

static bool is_pd_adapter(struct mtk_charger *info){
	if (info->ta_type == MTK_PD_CONNECT_PE_READY_SNK
		|| info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30
		|| info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		return true;
	}
	return false;
}

static ssize_t online_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0, noline_type = 0, usb_type = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]online show is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_charger_type(pinfo);
		usb_type = get_usb_type(pinfo);
	}
	if (ret == POWER_SUPPLY_TYPE_UNKNOWN)
		noline_type = NO_ADAPTER_TYPE;
	else if ((ret == POWER_SUPPLY_TYPE_USB)
		&& (usb_type == POWER_SUPPLY_USB_TYPE_DCP)
		&& (!is_pd_adapter(pinfo)))
		noline_type = NO_IMPLEMENT;
	else if (((ret == POWER_SUPPLY_TYPE_USB)
		&& (usb_type == POWER_SUPPLY_USB_TYPE_SDP))
		|| (ret == POWER_SUPPLY_TYPE_USB_CDP))
		noline_type = USB_CDP_TYPE;
	else
		noline_type = AC_ADAPTER_TYPE;
	pr_err("[%s] %d, %d, %d\n", __func__, ret, usb_type, noline_type);

	return sprintf(buf, "%d\n",noline_type);
}
static DEVICE_ATTR_RO(online);

#if defined (CONFIG_N28_CHARGER_PRIVATE)
extern int is_aw35615_pdic;
extern void aw_retry_source_cap(int cur);
#endif
void iphone_limit_api(bool val)
{
	if(val) {
#if defined (CONFIG_N28_CHARGER_PRIVATE)
		if (is_aw35615_pdic)
			aw_retry_source_cap(0);
		else
			pd_dpm_send_source_caps_0a(true);
#else
		pd_dpm_send_source_caps_0a(true);
#endif
	} else {
#if defined (CONFIG_N28_CHARGER_PRIVATE)
		if (is_aw35615_pdic)
			aw_retry_source_cap(500);
		else
			pd_dpm_send_source_caps_0a(true);
#else
		pd_dpm_send_source_caps_0a(false);
#endif
	}
}

static ssize_t batt_slate_mode_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	pr_err("batt_slate_mode = %d", batt_slate_mode);
	return sprintf(buf, "%d\n", batt_slate_mode);
}
static ssize_t batt_slate_mode_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = 0;

	if (buf != NULL && size != 0) {
		ret = kstrtoul(buf, 10, &val);
		if (ret)
			pr_err("slate_mode store err\n");
		ret = val;
		batt_slate_mode = val;

		if (batt_slate_mode == 3) {
			iphone_limit_api(true);
		} else if (batt_slate_mode == 0) {
			iphone_limit_api(false);
		}

		set_batt_slate_mode(&ret);
		pr_err("%s = %d,val=%d\n", __func__, batt_slate_mode, (int)val);
	}
	return size;
}
static DEVICE_ATTR_RW(batt_slate_mode);

static ssize_t batt_current_event_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *info;
	int secbat = SEC_BAT_CURRENT_EVENT_NONE;
	int tmp = 25;
	struct battery_data *bs_data;
	union power_supply_propval online;
	struct power_supply *chg_psy = NULL;
	static struct mtk_battery_manager *bm;

	if (bm == NULL)
		bm = get_mtk_battery_manager();
	bs_data = &bm->bs_data;
	chg_psy = bs_data->chg_psy;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL)
		return -ENODEV;

	info = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (info == NULL) {
		pr_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(bm->dev,
						       "charger");
		pr_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	}

	power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_ONLINE, &online);
	if(!online.intval)
		goto out;

	if((info->ta_type == MTK_PD_CONNECT_PE_READY_SNK) ||
		(info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) ||
		(info->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) ||
		(afc_get_is_connect(info)))
		secbat |= SEC_BAT_CURRENT_EVENT_FAST;

	if (batt_slate_mode)
		secbat |= SEC_BAT_CURRENT_EVENT_SLATE;

	if (bs_data->bat_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
		secbat |= SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE;

	tmp = force_get_tbat(bm->gm1, true);

	if (tmp < 10)
		secbat |= SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING;
	else if (tmp > 45)
		secbat |= SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING;

	if (info->disable_quick_charge) {
		secbat |= SEC_BAT_CURRENT_EVENT_HV_DISABLE;
	}
out:
	pr_err("%s: secbat = %d, tmp = %d, STATUS = %d, ONLINE = %d\n",
		__func__, secbat, tmp, bs_data->bat_status, online.intval);

	return sprintf(buf, "%d\n",secbat);
}
static DEVICE_ATTR_RO(batt_current_event);

static ssize_t batt_misc_event_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0, usb_type = 0;
	u32 batt_misc_event = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_charger_type(pinfo);
		usb_type = get_usb_type(pinfo);
	}
	if ((ret == POWER_SUPPLY_TYPE_USB && usb_type == POWER_SUPPLY_USB_TYPE_DCP)
		&& (!is_pd_adapter(pinfo)))
		batt_misc_event |= 0x4;
	else
		batt_misc_event |= 0x0;

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	if (pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
		if (wt_batt_full_capacity_check_for_cp() < 0) {
			batt_misc_event |= 0x01000000;
		}
	}
#endif

	pr_err("%s: batt_misc_event=%d, chg_type=%d, usb_type=%d\n",
		__func__, batt_misc_event, ret, usb_type);

	return sprintf(buf, "%d\n",batt_misc_event);
}
static DEVICE_ATTR_RO(batt_misc_event);

static ssize_t direct_charging_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *info;
	int dcstatus = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	info = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (info == NULL) {
		pr_err("[%s] is not rdy\n", __func__);
		return -1;
	}

	if (adapter_is_support_pd_pps(info))
		dcstatus = 1;

	pr_err("%s: dcstatus=%d\n", __func__, dcstatus);

	return sprintf(buf, "%d\n",dcstatus);
}
static DEVICE_ATTR_RO(direct_charging_status);

static ssize_t show_batt_charging_source(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;
	struct power_supply *chg_psy = NULL;
	static struct mtk_battery_manager *bm;
	struct battery_data *bs_data = NULL;
	union power_supply_propval online = {0, };

	if (bm == NULL)
		bm = get_mtk_battery_manager();
	bs_data = &bm->bs_data;
	chg_psy = bs_data->chg_psy;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if(bat_psy == NULL) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(bm->dev, "charger");
		pr_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_ONLINE, &online);
	}

	if (online.intval == 0) {
		pinfo->batt_charging_source = SEC_BATTERY_CABLE_NONE;
	} else {
		if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_PDIC_APDO;
		} else if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_PDIC;
#ifdef CONFIG_AFC_CHARGER
		} else if (afc_get_is_connect(pinfo)) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_9V_TA;
#endif
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_USB;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_USB_CDP;
		}  else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_TA;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
		} else {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
		}
	}
	pr_err("%s: usb_online=%d, batt_charging_source=%d\n",
		__func__, online.intval, pinfo->batt_charging_source);

	return sprintf(buf, "%d\n", pinfo->batt_charging_source);
}

static ssize_t store_batt_charging_source(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if(bat_psy == NULL) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	ret = kstrtoint(buf, 10, &val);
	if(ret < 0)
		return -EINVAL;

	pinfo->batt_charging_source = val;

	return count;
}
static DEVICE_ATTR(batt_charging_source, 0664, show_batt_charging_source,
		   store_batt_charging_source);

static ssize_t show_charging_type(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;
	struct power_supply *chg_psy = NULL;
	static struct mtk_battery_manager *bm;
	struct battery_data *bs_data = NULL;
	union power_supply_propval online = {0, };
	int batt_charging_type = SEC_BATTERY_CABLE_NONE;
	int i = 0;

	if (bm == NULL)
		bm = get_mtk_battery_manager();
	bs_data = &bm->bs_data;
	chg_psy = bs_data->chg_psy;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if(bat_psy == NULL) {
		pr_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		pr_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(bm->dev, "charger");
		pr_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_ONLINE, &online);
	}

	if (online.intval == 0) {
		batt_charging_type = SEC_BATTERY_CABLE_NONE;
	} else {
		if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			batt_charging_type = SEC_BATTERY_CABLE_PDIC_APDO;
		} else if (pinfo->ta_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) {
			batt_charging_type = SEC_BATTERY_CABLE_PDIC;
#ifdef CONFIG_AFC_CHARGER
		} else if (afc_get_is_connect(pinfo)) {
			batt_charging_type = SEC_BATTERY_CABLE_9V_TA;
#endif
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
			batt_charging_type = SEC_BATTERY_CABLE_USB;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
			batt_charging_type = SEC_BATTERY_CABLE_USB_CDP;
		}  else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
			batt_charging_type = SEC_BATTERY_CABLE_TA;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
			batt_charging_type = SEC_BATTERY_CABLE_UNKNOWN;
		} else {
			batt_charging_type = SEC_BATTERY_CABLE_UNKNOWN;
		}
	}

	pr_err("%s: usb_online=%d, batt_charging_type=%d\n",
		__func__, online.intval, batt_charging_type);

	for (i = 0; i < ARRAY_SIZE(wt_ta_type); i++) {
		if(batt_charging_type == wt_ta_type[i].charging_type) {
			return scnprintf(buf, PROP_SIZE_LEN, "%s\n", wt_ta_type[i].ta_type);
		}
	}
	return -ENODATA;
}
static DEVICE_ATTR(charging_type, 0444, show_charging_type, NULL);
#endif

void bm_battery_service_init(struct mtk_battery_manager *bm)
{
	struct battery_data *bs_data;

	bs_data = &bm->bs_data;
	bs_data->psd.name = "battery";
	bs_data->psd.type = POWER_SUPPLY_TYPE_BATTERY;
	bs_data->psd.properties = battery_props;
	bs_data->psd.num_properties = ARRAY_SIZE(battery_props);
	bs_data->psd.get_property = bs_psy_get_property;
	bs_data->psd.set_property = bs_psy_set_property;
	bs_data->psd.external_power_changed =
		mtk_battery_external_power_changed;
	bs_data->psy_cfg.drv_data = bm;

	bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING,
	bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD,
	bs_data->bat_present = 1,
	bs_data->bat_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	bs_data->bat_capacity = -1,
	bs_data->bat_batt_vol = 0,
	bs_data->bat_batt_temp = 0,
	bs_data->old_bat_status = POWER_SUPPLY_STATUS_DISCHARGING,

	bm->bs_data.psy =
	power_supply_register(
		bm->dev, &bm->bs_data.psd, &bm->bs_data.psy_cfg);
	if (IS_ERR(bm->bs_data.psy))
		pr_err("[BAT_probe] power_supply_register Battery Fail !!\n");

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_stop_charge);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_start_charge);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_current_ua_now);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_temp);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_type);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_hv_charger_status);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_new_charge_type);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_resistance_id);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_vol);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_set_battery_cycle);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_battery_cycle);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_online);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_slate_mode);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_current_event);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_misc_event);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_direct_charging_status);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_batt_charging_source);
	device_create_file(&bm->bs_data.psy->dev, &dev_attr_charging_type);
#endif

	bm->gm1->fixed_uisoc = 0xffff;
	if (bm->gm_no == 2)
		bm->gm2->fixed_uisoc = 0xffff;

	mtk_battery_external_power_changed(bm->bs_data.psy);
}

void mtk_bm_send_to_user(struct mtk_battery_manager *bm, u32 pid,
	int seq, struct afw_header *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size = reply_msg->data_len + AFW_MSG_HEADER_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret = -1;

	if (bm == NULL)
		return;

	//pr_err("[%s]id:%d cmd:%d datalen:%d\n", __func__,
	//	reply_msg->instance_id, reply_msg->cmd, reply_msg->data_len);

	if (pid == 0) {
		pr_err("[%s]=>pid is 0 , id:%d cmd:%d\n", __func__,
			reply_msg->instance_id, reply_msg->cmd);
		return;
	}


	reply_msg->identity = AFW_MAGIC;

	if (in_interrupt())
		skb = alloc_skb(len, GFP_ATOMIC);
	else
		skb = alloc_skb(len, GFP_KERNEL);

	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	if (bm->mtk_bm_sk != NULL) {
		ret = netlink_unicast
			(bm->mtk_bm_sk, skb, pid, MSG_DONTWAIT);
		//pr_err("[%s]netlink_unicast , id:%d cmd:%d\n",
		//	__func__, reply_msg->instance_id, reply_msg->cmd);
	} else
		pr_err("[%s]bm->mtk_bm_sk is  NULL\n", __func__);
	if (ret < 0) {
		pr_err("[%s]send failed ret=%d pid=%d\n", __func__, ret, pid);
		return;
	}
}

void mtk_bm_handler(struct mtk_battery *gm,
	int seq, struct afw_header *reply_msg)
{
	static struct mtk_battery_manager *bm;

	//pr_err("[%s]id:%d =>cmd:%d subcmd:%d %d hash %d\n", __func__,
	//	gm->id, reply_msg->cmd, reply_msg->subcmd, reply_msg->subcmd_para1, reply_msg->hash);

	bm = gm->bm;
	if (bm == NULL)
		bm = get_mtk_battery_manager();

	if (bm != NULL) {
		if (bm->gm1 == gm) {
			reply_msg->instance_id = gm->id;
			mtk_bm_send_to_user(bm, bm->fgd_pid,
				seq, reply_msg);
		} else if (bm->gm2 == gm) {
			reply_msg->instance_id = gm->id;
			mtk_bm_send_to_user(bm, bm->fgd_pid,
				seq, reply_msg);
		} else
			pr_err("[%s]gm is incorrect !n", __func__);
	} else
		pr_err("[%s]bm is incorrect !n", __func__);
}

static void fg_cmd_check(struct afw_header *msg)
{
	while (msg->subcmd == 0 &&
		msg->subcmd_para1 != AFW_MSG_HEADER_LEN) {
		pr_err("fuel gauge version error cmd:%d %d\n",
			msg->cmd,
			msg->subcmd);
		msleep(10000);
		break;
	}
}

static void mtk_battery_manager_handler(struct mtk_battery_manager *bm, void *nl_data,
	struct afw_header *ret_msg, int seq)
{
	struct afw_header *msg;
	struct mtk_battery *gm;
	bool send = true;

	if (bm == NULL) {
		bm = get_mtk_battery_manager();
		if (bm == NULL)
			return;
	}
	msg = nl_data;
//	ret_msg->nl_cmd = msg->nl_cmd;
	ret_msg->cmd = msg->cmd;
	ret_msg->instance_id = msg->instance_id;
	ret_msg->hash = msg->hash;
	ret_msg->datatype = msg->datatype;


	//pr_err("[%s] gm id:%d cmd:%d type:%d hash:%d\n",
	//	__func__, msg->instance_id, msg->cmd, msg->datatype, msg->hash);

	//msleep(3000);
	//mdelay(3000);
	if (msg->instance_id == 0)
		gm = bm->gm1;
	else if (msg->instance_id == 1)
		gm = bm->gm2;
	else {
		pr_err("[%s] can not find gm, id:%d cmd:%d\n", __func__, msg->instance_id, msg->cmd);
		return;
	}

	switch (msg->cmd) {

	case AFW_CMD_PRINT_LOG:
	case FG_DAEMON_CMD_PRINT_LOG:
	{
		fg_cmd_check(msg);
		pr_err("[%sd]%s", gm->gauge->name,&msg->data[0]);

	}
	break;
	case AFW_CMD_SET_PID:
	//case FG_DAEMON_CMD_SET_DAEMON_PID:
	{
		unsigned int ino = bm->gm_no;

		fg_cmd_check(msg);
		/* check is daemon killed case*/
		if (bm->fgd_pid == 0) {
			memcpy(&bm->fgd_pid, &msg->data[0],
				sizeof(bm->fgd_pid));
			pr_err("[K]FG_DAEMON_CMD_SET_DAEMON_PID = %d(first launch) ino:%d\n",
				bm->fgd_pid, ino);
		} else {
			memcpy(&bm->fgd_pid, &msg->data[0],
				sizeof(bm->fgd_pid));

		/* WY_FIX: */
			pr_err("[K]FG_DAEMON_CMD_SET_DAEMON_PID=%d,kill daemon:%d init_flag:%d (re-launch) ino:%d\n",
				bm->fgd_pid,
				gm->Bat_EC_ctrl.debug_kill_daemontest,
				gm->init_flag,
				ino);
			if (gm->Bat_EC_ctrl.debug_kill_daemontest != 1 &&
				gm->init_flag == 1)
				gm->fg_cust_data.dod_init_sel = 14;
			else
				gm->fg_cust_data.dod_init_sel = 0;
		}
		ret_msg->data_len += sizeof(ino);
		memcpy(ret_msg->data, &ino, sizeof(ino));
	}
	break;
	default:
	{
		if (msg->instance_id == 0) {
			if (bm->gm1 != NULL && bm->gm1->netlink_handler != NULL) {
				bm->gm1->netlink_handler(bm->gm1, nl_data, ret_msg);
			} else {
				pr_err("[%s]gm1 netlink_handler is NULL\n", __func__);
				send = false;
			}
		} else if (msg->instance_id == 1) {
			if (bm->gm2 != NULL && bm->gm2->netlink_handler != NULL) {
				bm->gm2->netlink_handler(bm->gm2, nl_data, ret_msg);
			} else {
				pr_err("[%s]gm2 netlink_handler is NULL\n", __func__);
				send = false;
			}
		} else {
			pr_err("[%s]gm instance id is not supported:%d\n", __func__, msg->instance_id);
			send = false;
		}
	}
	break;
	}

	if (send == true)
		mtk_bm_send_to_user(bm, bm->fgd_pid, seq, ret_msg);

}

void mtk_bm_netlink_handler(struct sk_buff *skb)
{
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct afw_header *fgd_msg, *fgd_ret_msg;
	int size = 0;
	static struct mtk_battery_manager *bm;

	if (bm == NULL)
		bm = get_mtk_battery_manager();

	nlh = (struct nlmsghdr *)skb->data;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	fgd_msg = (struct afw_header *)data;

	if (fgd_msg->identity != AFW_MAGIC) {
		pr_err("[%s]not correct MTKFG netlink packet!%d\n",
			__func__, fgd_msg->identity);
		return;
	}

	size = fgd_msg->ret_data_len + AFW_MSG_HEADER_LEN;

	if (size > (PAGE_SIZE << 1))
		fgd_ret_msg = vmalloc(size);
	else {
		if (in_interrupt())
			fgd_ret_msg = kmalloc(size, GFP_ATOMIC);
		else
			fgd_ret_msg = kmalloc(size, GFP_KERNEL);
	}

	if (fgd_ret_msg == NULL) {
		if (size > PAGE_SIZE)
			fgd_ret_msg = vmalloc(size);

		if (fgd_ret_msg == NULL)
			return;
	}

	memset(fgd_ret_msg, 0, size);

	mtk_battery_manager_handler(bm, data, fgd_ret_msg, seq);

	kvfree(fgd_ret_msg);

#ifdef WY_FIX
	if (fgd_msg->instance_id == 0) {
		if (bm->gm1 != NULL && bm->gm1->netlink_handler != NULL) {
			bm->gm1->netlink_handler(bm->gm1, data, fgd_ret_msg);
			mtk_bm_send_to_user(bm, pid, seq, fgd_ret_msg);
		} else
			pr_err("[%s]gm1 netlink_handler is NULL\n", __func__);
	} else if (fgd_msg->instance_id == 1) {
		if (bm->gm2 != NULL && bm->gm2->netlink_handler != NULL) {
			bm->gm2->netlink_handler(bm->gm2, data, fgd_ret_msg);
			mtk_bm_send_to_user(bm, pid, seq, fgd_ret_msg);
		} else
			pr_err("[%s]gm2 netlink_handler is NULL\n", __func__);
	} else {
		pr_err("[%s]gm instance id is not supported:%d\n", __func__, fgd_msg->instance_id);
	}
#endif

}

static int mtk_bm_create_netlink(struct platform_device *pdev)
{
	struct mtk_battery_manager *bm = platform_get_drvdata(pdev);
	struct netlink_kernel_cfg cfg = {
		.input = mtk_bm_netlink_handler,
	};

	bm->mtk_bm_sk =
		netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);

	if (bm->mtk_bm_sk == NULL) {
		pr_err("netlink_kernel_create error\n");
		return -EIO;
	}

	pr_err("[%s]netlink_kernel_create protol= %d\n",
		__func__, NETLINK_FGD);

	return 0;
}

void bm_custom_init_from_header(struct mtk_battery_manager *bm)
{
	bm->vsys_det_voltage1  = VSYS_DET_VOLTAGE1;
	bm->vsys_det_voltage2  = VSYS_DET_VOLTAGE2;
}

void bm_custom_init_from_dts(struct platform_device *pdev, struct mtk_battery_manager *bm)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	ret = device_property_read_u32(dev, "disable-quick-shutdown", &bm->disable_quick_shutdown);
	if (ret)
		pr_debug("%s: disable-quick-shutdown get fail\n", __func__);

	ret = device_property_read_u32(dev, "vsys-det-voltage1", &bm->vsys_det_voltage1);
	if (ret)
		pr_debug("%s: vsys-det-voltage1 get fail\n", __func__);

	ret = device_property_read_u32(dev, "vsys-det-voltage2", &bm->vsys_det_voltage2);
	if (ret)
		pr_debug("%s: vsys-det-voltage2 get fail\n", __func__);

	pr_debug("%s: %d %d %d n", __func__,
		bm->disable_quick_shutdown, bm->vsys_det_voltage1, bm->vsys_det_voltage2);
}

static int mtk_bm_probe(struct platform_device *pdev)
{
	struct mtk_battery_manager *bm;
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	int ret = 0;

	pr_err("[%s] 20231205-1\n", __func__);
	bm = devm_kzalloc(&pdev->dev, sizeof(*bm), GFP_KERNEL);
	if (!bm)
		return -ENOMEM;

	platform_set_drvdata(pdev, bm);
	bm->dev = &pdev->dev;

	bm_custom_init_from_header(bm);
	bm_custom_init_from_dts(pdev, bm);

	psy = devm_power_supply_get_by_phandle(&pdev->dev,
								 "gauge1");

	if (psy == NULL || IS_ERR(psy)) {
		pr_err("[%s]can not get gauge1 psy\n", __func__);
	} else {
		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge != NULL) {
			bm->gm1 = gauge->gm;
			bm->gm1->id = 0;
			bm->gm1->bm = bm;
			bm->gm1->netlink_send = mtk_bm_handler;
		} else
			pr_err("[%s]gauge1 is not rdy\n", __func__);
	}
	psy = devm_power_supply_get_by_phandle(&pdev->dev,
								 "gauge2");

	if (psy == NULL || IS_ERR(psy)) {
		pr_err("[%s]can not get gauge2 psy\n", __func__);
	} else {
		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge != NULL) {
			bm->gm2 = gauge->gm;
			bm->gm2->id = 1;
			bm->gm2->bm = bm;
			bm->gm2->netlink_send = mtk_bm_handler;
		} else
			pr_err("[%s]gauge2 is not rdy\n", __func__);
	}

	if (bm->gm1 != NULL) {
		if (bm->gm2 != NULL) {
			pr_err("[%s]dual gauges is enabled\n", __func__);
			bm->gm_no = 2;
		} else {
			pr_err("[%s]single gauge is enabled\n", __func__);
			bm->gm_no = 1;
		}
	} else
		pr_err("[%s]gauge configuration is incorrect\n", __func__);

	if (bm->gm1 == NULL && bm->gm2 == NULL) {
		pr_err("[%s]disable gauge because can not find gm!\n", __func__);
		return 0;
	}


	bm->chan_vsys = devm_iio_channel_get(
		&pdev->dev, "vsys");
	if (IS_ERR(bm->chan_vsys)) {
		ret = PTR_ERR(bm->chan_vsys);
		pr_err("vsys auxadc get fail, ret=%d\n", ret);
	}

	bm_check_bootmode(&pdev->dev, bm);

	init_waitqueue_head(&bm->wait_que);


	bm->bm_wakelock = wakeup_source_register(NULL, "battery_manager_wakelock");
	spin_lock_init(&bm->slock);

#ifdef CONFIG_PM
	bm->pm_notifier.notifier_call = bm_pm_event;
	ret = register_pm_notifier(&bm->pm_notifier);
	if (ret) {
		pr_err("%s failed to register system pm notify\n", __func__);
		unregister_pm_notifier(&bm->pm_notifier);
	}
#endif /* CONFIG_PM */

#ifdef BM_USE_HRTIMER
	battery_manager_thread_hrtimer_init(bm);
#endif
#ifdef BM_USE_ALARM_TIMER
	battery_manager_thread_alarm_init(bm);
#endif

	kthread_run(battery_manager_routine_thread, bm, "battery_manager_thread");

	bm->bs_data.chg_psy = devm_power_supply_get_by_phandle(&pdev->dev, "charger");
	if (IS_ERR_OR_NULL(bm->bs_data.chg_psy))
		pr_err("[%s]Fail to get chg_psy!\n", __func__);

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	init_avg_current(-1);
#endif

	bm_battery_service_init(bm);
	mtk_bm_create_netlink(pdev);

	mtk_power_misc_init(bm, &bm->sdc);

	bm->sdc.sdc_wakelock = wakeup_source_register(NULL, "battery_manager_sdc_wakelock");
	spin_lock_init(&bm->sdc.slock);

	return 0;
}

static int mtk_bm_remove(struct platform_device *pdev)
{
	return 0;
}

static void mtk_bm_shutdown(struct platform_device *pdev)
{
}

static int __maybe_unused mtk_bm_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mtk_bm_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_bm_pm_ops, mtk_bm_suspend, mtk_bm_resume);

static const struct of_device_id __maybe_unused mtk_bm_of_match[] = {
	{ .compatible = "mediatek,battery manager", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_bm_of_match);

static struct platform_driver mtk_battery_manager_driver = {
	.probe = mtk_bm_probe,
	.remove = mtk_bm_remove,
	.shutdown = mtk_bm_shutdown,
	.driver = {
		.name = "mtk_battery_manager",
		.pm = &mtk_bm_pm_ops,
		.of_match_table = mtk_bm_of_match,
	},
};
module_platform_driver(mtk_battery_manager_driver);

MODULE_AUTHOR("Wy Chuang<Wy.Chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Battery Manager");
MODULE_LICENSE("GPL");
