/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include <linux/atomic.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power_supply.h>
#include "mtk_smartcharging.h"
#include "afc_charger_intf.h"

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

#define SC_TAG "smartcharging"

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
#ifndef CONFIG_WT_COMPILE_FACTORY_VERSION
#define ONEUI_6P1_CHG_PROTECION_ENABLE
#endif

//#define WT_OPTIMIZE_USING_HYSTERESIS
//#define WT_OPTIMIZE_USING_UI_TIME
#endif

#define CHARGING_THERMAL_ENABLE
//#define CHARGING_15W_THERMAL_ENABLE

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

struct mtk_charger;
struct charger_data;
#define BATTERY_FOR_TESTING_CV 4100000
#define BATTERY_CV 4350000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
#define VBUS_OVP_VOLTAGE 15000000 /* 15V */
/* dual battery */
#define V_CS_BATTERY_CV 4350 /* mV */
#define AC_CS_NORMAL_CC 2000 /* mV */
#define AC_CS_FAST_CC 2000 /* mV */
#define CS_CC_MIN 100 /* mA */
#define V_BATT_EXTRA_DIFF 300 /* 265 mV */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#define FIX_PDO_CHARGER_CURRENT			2800000
#define FIX_PDO_INPUT_CURRENT			1600000
#define APDO_CHARGER_T2_TO_T3_CURRENT		5200000
#define APDO_CHARGER_T3_TO_T4_CURRENT		3800000
#define FIX_PDO_COMMON_INPUT_CURRENT       1100000
#define CHARGING_PROTECT_INPUT_CURRENT     1200000
#define PROP_SIZE_LEN 20
/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)
#define CHG_DPDM_OV_STATUS	(1 << 7)

/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

#define MAX_ALG_NO 10

#define RESET_BOOT_VOLT_TIME 50
enum current_level_enum {
	CURRENT_LEVEL_NONE = 0,
	CURRENT_LEVEL1,
	CURRENT_LEVEL2,
	CURRENT_LEVEL3,
	CURRENT_LEVEL4,
	CURRENT_LEVEL5,
	CURRENT_LEVEL6,
	CURRENT_LEVEL7,
	CURRENT_LEVEL_MAX
};

enum calculate_time_state_enum {
	CALCULATE_NONE_STATE = 0,
	CALCULATE_INIT_STATE,
	CALCULATE_CHARGING_STATE,
	CALCULATE_FULL_STATE,
	CALCULATE_PLUG_OUT_STATE,
	CALCULATE_INVALID_STATE,
	CALCULATE_STATEL_MAX
};

enum compensation_state_enum {
	COMPENSATION_LEVEL_REDUCE_NORMAL = 0,
	COMPENSATION_LEVEL_REDUCE_QUICK,
	COMPENSATION_LEVEL_REDUCE_SLOW,
	COMPENSATION_LEVEL_MAX
};


enum mtk_pd_connect_type {
	MTK_PD_CONNECT_NONE,
	MTK_PD_CONNECT_HARD_RESET,
	MTK_PD_CONNECT_SOFT_RESET,
	MTK_PD_CONNECT_PE_READY_SNK,
	MTK_PD_CONNECT_PE_READY_SNK_PD30,
	MTK_PD_CONNECT_PE_READY_SNK_APDO,
	MTK_PD_CONNECT_TYPEC_ONLY_SNK,
};

enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

enum DUAL_CHG_STAT {
	BOTH_EOC,
	STILL_CHG,
};

enum ADC_SOURCE {
	NULL_HANDLE,
	FROM_CHG_IC,
	FROM_CS_ADC,
};

enum TA_STATE {
	TA_INIT_FAIL,
	TA_CHECKING,
	TA_NOT_SUPPORT,
	TA_NOT_READY,
	TA_READY,
	TA_PD_PPS_READY,
};

enum adapter_protocol_state {
	FIRST_HANDSHAKE,
	RUN_ON_PD,
	RUN_ON_UFCS,
};

enum TA_CAP_STATE {
	APDO_TA,
	WO_APDO_TA,
	STD_TA,
	ONLY_APDO_TA,
};

enum chg_dev_notifier_events {
	EVENT_FULL,
	EVENT_RECHARGE,
	EVENT_DISCHARGE,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};

struct wt_charging_type {
	int charging_type;
	char *ta_type;
};

/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV	4240000
#define JEITA_TEMP_T3_TO_T4_CV	4240000
#define JEITA_TEMP_T2_TO_T3_CV	4340000
#define JEITA_TEMP_T1_TO_T2_CV	4240000
#define JEITA_TEMP_T0_TO_T1_CV	4040000
#define JEITA_TEMP_BELOW_T0_CV	4040000
#define JEITA_TEMP_ABOVE_T4_CC	0
#define JEITA_TEMP_T3_TO_T4_CC	1500000
#define JEITA_TEMP_T2_TO_T3_CC	1500000
#define JEITA_TEMP_T1_TO_T2_CC	1400000
#define JEITA_TEMP_T0_TO_T1_CC	486000
#define JEITA_TEMP_BELOW_T0_CC	0
#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES 0

#ifdef CHARGING_THERMAL_ENABLE
#ifdef CONFIG_W2_CHARGER_PRIVATE
//common charge
//lcm off
#define AP_TEMP_T3_THRES  46
#define AP_TEMP_T3_THRES_MINUS_X_DEGREE 45
#define AP_TEMP_T2_THRES  43
#define AP_TEMP_T2_THRES_MINUS_X_DEGREE 42
#define AP_TEMP_T1_THRES  38
#define AP_TEMP_T1_THRES_MINUS_X_DEGREE 37
#define AP_TEMP_T0_THRES  36
#define AP_TEMP_T0_THRES_MINUS_X_DEGREE 35
//lcm on
#define AP_TEMP_LCMON_T4 45
#define AP_TEMP_LCMON_T4_ANTI_SHAKE 43
#define AP_TEMP_LCMON_T3 43
#define AP_TEMP_LCMON_T3_ANTI_SHAKE 41
#define AP_TEMP_LCMON_T2 42
#define AP_TEMP_LCMON_T2_ANTI_SHAKE 40
#define AP_TEMP_LCMON_T1 41
#define AP_TEMP_LCMON_T1_ANTI_SHAKE 39
#define AP_TEMP_LCMON_T0 39
#define AP_TEMP_LCMON_T0_ANTI_SHAKE 37

#elif defined(CONFIG_N28_CHARGER_PRIVATE)
//common charge
//lcm off
#define AP_TEMP_T3_THRES  46
#define AP_TEMP_T3_THRES_MINUS_X_DEGREE 45
#define AP_TEMP_T2_THRES  42
#define AP_TEMP_T2_THRES_MINUS_X_DEGREE 41
#define AP_TEMP_T1_THRES  38
#define AP_TEMP_T1_THRES_MINUS_X_DEGREE 37
#define AP_TEMP_T0_THRES  36
#define AP_TEMP_T0_THRES_MINUS_X_DEGREE 35
//lcm on
#define AP_TEMP_LCMON_T4 45
#define AP_TEMP_LCMON_T4_ANTI_SHAKE 43
#define AP_TEMP_LCMON_T3 43
#define AP_TEMP_LCMON_T3_ANTI_SHAKE 41
#define AP_TEMP_LCMON_T2 42
#define AP_TEMP_LCMON_T2_ANTI_SHAKE 40
#define AP_TEMP_LCMON_T1 41
#define AP_TEMP_LCMON_T1_ANTI_SHAKE 39
#define AP_TEMP_LCMON_T0 39
#define AP_TEMP_LCMON_T0_ANTI_SHAKE 37
//fast charge
//lcm off
#define AP_TEMP_T4_CP_THRES  48
#define AP_TEMP_T4_CP_THRES_MINUS_X_DEGREE 47
#define AP_TEMP_T3_CP_THRES  45
#define AP_TEMP_T3_CP_THRES_MINUS_X_DEGREE 44
#define AP_TEMP_T2_CP_THRES  39
#define AP_TEMP_T2_CP_THRES_MINUS_X_DEGREE 38
#define AP_TEMP_T1_CP_THRES  37
#define AP_TEMP_T1_CP_THRES_MINUS_X_DEGREE 36
#define AP_TEMP_T0_CP_THRES  35
#define AP_TEMP_T0_CP_THRES_MINUS_X_DEGREE 34

//#else
//other project
//#define AP_TEMP_THRES_LCMON 44
//#define AP_TEMP_THRES_MINUS_X_DEGREE_LCMON 43
//#define AP_TEMP_CP_THRES_LCMON 44
//#define AP_TEMP_CP_THRES_MINUS_X_DEGREE_LCMON 43
#endif
#endif

#ifdef CHARGING_15W_THERMAL_ENABLE
#define THERMAL_CURRENT_LCMOFF_T4     500000
#define THERMAL_CURRENT_LCMOFF_T3     1500000
#define THERMAL_CURRENT_LCMOFF_T2     2500000
#define THERMAL_CURRENT_LCMOFF_T1     3500000
#define THERMAL_CURRENT_LCMOFF_T0     4000000

#define THERMAL_15W_CURRENT_LCMOFF_T4 1000000
#define THERMAL_15W_CURRENT_LCMOFF_T3 1500000
#define THERMAL_15W_CURRENT_LCMOFF_T2 1500000
#define THERMAL_15W_CURRENT_LCMOFF_T1 1500000
#define THERMAL_15W_CURRENT_LCMOFF_T0 2000000
#endif

/* yuanjian.wt add for AFC */
/* AFC */
#define AFC_ICHG_LEAVE_THRESHOLD  1000000 /* uA */
#define AFC_START_BATTERY_SOC	  0
#define AFC_STOP_BATTERY_SOC	  85
#define AFC_PRE_INPUT_CURRENT     500000 /* uA */
#define AFC_CHARGER_INPUT_CURRENT 1670000 /* uA */
#define AFC_CHARGER_CURRENT 3000000
#define AFC_MIN_CHARGER_VOLTAGE   4200000
#define AFC_MAX_CHARGER_VOLTAGE   9000000
#define AFC_COMMON_ICL_CURR_MAX 1800000
#define CHG_AFC_COMMON_CURR_MAX 2500000


/*batt_charging_source*/
#define SEC_BATTERY_CABLE_UNKNOWN                0
#define SEC_BATTERY_CABLE_NONE                   1
#define SEC_BATTERY_CABLE_PREPARE_TA             2
#define SEC_BATTERY_CABLE_TA                     3
#define SEC_BATTERY_CABLE_USB                    4
#define SEC_BATTERY_CABLE_USB_CDP                5
#define SEC_BATTERY_CABLE_9V_TA                  6
#define SEC_BATTERY_CABLE_9V_ERR                 7
#define SEC_BATTERY_CABLE_9V_UNKNOWN             8
#define SEC_BATTERY_CABLE_12V_TA                 9
#define SEC_BATTERY_CABLE_WIRELESS               10
#define SEC_BATTERY_CABLE_HV_WIRELESS            11
#define SEC_BATTERY_CABLE_PMA_WIRELESS           12
#define SEC_BATTERY_CABLE_WIRELESS_PACK          13
#define SEC_BATTERY_CABLE_WIRELESS_HV_PACK       14
#define SEC_BATTERY_CABLE_WIRELESS_STAND         15
#define SEC_BATTERY_CABLE_WIRELESS_HV_STAND      16
#define SEC_BATTERY_CABLE_QC20                   17
#define SEC_BATTERY_CABLE_QC30                   18
#define SEC_BATTERY_CABLE_PDIC                   19
#define SEC_BATTERY_CABLE_UARTOFF                20
#define SEC_BATTERY_CABLE_OTG                    21
#define SEC_BATTERY_CABLE_LAN_HUB                22
#define SEC_BATTERY_CABLE_POWER_SHARING          23
#define SEC_BATTERY_CABLE_HMT_CONNECTED          24
#define SEC_BATTERY_CABLE_HMT_CHARGE             25
#define SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT        26
#define SEC_BATTERY_CABLE_WIRELESS_VEHICLE       27
#define SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE    28
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV    29
#define SEC_BATTERY_CABLE_TIMEOUT                30
#define SEC_BATTERY_CABLE_SMART_OTG              31
#define SEC_BATTERY_CABLE_SMART_NOTG             32
#define SEC_BATTERY_CABLE_WIRELESS_TX            33
#define SEC_BATTERY_CABLE_HV_WIRELESS_20         34
#define SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT   35
#define SEC_BATTERY_CABLE_WIRELESS_FAKE          36
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_20    37
#define SEC_BATTERY_CABLE_PDIC_APDO              38
#define SEC_BATTERY_CABLE_POGO                   39
#define SEC_BATTERY_CABLE_POGO_9V                40
#define SEC_BATTERY_CABLE_FPDO_DC                41
#define SEC_BATTERY_CABLE_MAX                    42

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4,
	TEMP_MAX,
};

// gujiayin.wt 20240910 add SDP/DCP/CDP/HVDCP in  88 mode
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
enum real_charge_type {
	REAL_TYPE_UNKNOWN = 0,
	REAL_TYPE_BATTERY,
	REAL_TYPE_UPS,
	REAL_TYPE_MAINS,
	REAL_TYPE_USB,			/* Standard Downstream Port */
	REAL_TYPE_USB_DCP,		/* Dedicated Charging Port */
	REAL_TYPE_USB_CDP,		/* Charging Downstream Port */
	REAL_TYPE_USB_ACA,		/* Accessory Charger Adapters */
	REAL_TYPE_USB_TYPE_C,		/* Type C Port */
	REAL_TYPE_USB_PD,		/* Power Delivery Port */
	REAL_TYPE_USB_PD_DRP,		/* PD Dual Role Port */
	REAL_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	REAL_TYPE_WIRELESS,		/* Wireless */
	REAL_TYPE_USB_HVDCP,		/* HVDCP */
	REAL_TYPE_USB_AFC,		/* AFC */
	REAL_TYPE_USB_FLOAT,		/* Float */
};
#endif

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
enum {
	POWER_SUPPLY_NO_ONEUI_CHG = 0,
	POWER_SUPPLY_CAPACITY_100,
	POWER_SUPPLY_CAPACITY_80_HIGHSOC,
	POWER_SUPPLY_CAPACITY_80_SLEEP,
	POWER_SUPPLY_CAPACITY_80_OPTION,
	POWER_SUPPLY_CAPACITY_80_OFFCHARGING,
	POWER_SUPPLY_CAPACITY_85_OPTION,
	POWER_SUPPLY_CAPACITY_85_OFFCHARGING,
	POWER_SUPPLY_CAPACITY_90_OPTION,
	POWER_SUPPLY_CAPACITY_90_OFFCHARGING,
	POWER_SUPPLY_CAPACITY_95_OPTION,
	POWER_SUPPLY_CAPACITY_95_OFFCHARGING,
};

static const char * const POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT[] = {
	[POWER_SUPPLY_NO_ONEUI_CHG]		= "null",
	[POWER_SUPPLY_CAPACITY_100]		= "100",
	[POWER_SUPPLY_CAPACITY_80_HIGHSOC]	= "80 HIGHSOC",
	[POWER_SUPPLY_CAPACITY_80_SLEEP]	= "80 SLEEP",
	[POWER_SUPPLY_CAPACITY_80_OPTION]	= "80 OPTION",
	[POWER_SUPPLY_CAPACITY_80_OFFCHARGING]	= "80",
	[POWER_SUPPLY_CAPACITY_85_OPTION]	= "85 OPTION",
	[POWER_SUPPLY_CAPACITY_85_OFFCHARGING]	= "85",
	[POWER_SUPPLY_CAPACITY_90_OPTION]	= "90 OPTION",
	[POWER_SUPPLY_CAPACITY_90_OFFCHARGING]	= "90",
	[POWER_SUPPLY_CAPACITY_95_OPTION]	= "95 OPTION",
	[POWER_SUPPLY_CAPACITY_95_OFFCHARGING]	= "95",
};
#endif


struct info_notifier_block {
	struct notifier_block nb;
	struct mtk_charger *info;
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	int cc;
	bool charging;
	bool error_recovery_flag;
};

struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*do_dvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_dvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_hvdvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_hvdvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;
	int vbus_sw_ovp_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int charging_host_charger_current;

	int pe2_charger_input_current;
	int pe2_charger_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int jeita_temp_above_t4_cc;
	int jeita_temp_t3_to_t4_cc;
	int jeita_temp_t2_to_t3_cc;
	int jeita_temp_t1_to_t2_cc;
	int jeita_temp_t0_to_t1_cc;
	int jeita_temp_below_t0_cc;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;

#ifdef CHARGING_THERMAL_ENABLE
	//common charge
	//lcm off
	int ap_temp_t3_thres;
	int ap_temp_t3_thres_minus_x_degree;
	int ap_temp_t2_thres;
	int ap_temp_t2_thres_minus_x_degree;
	int ap_temp_t1_thres;
	int ap_temp_t1_thres_minus_x_degree;
	int ap_temp_t0_thres;
	int ap_temp_t0_thres_minus_x_degree;
	//lcm on
	int ap_temp_lcmon_t4;
	int ap_temp_lcmon_t4_anti_shake;
	int ap_temp_lcmon_t3;
	int ap_temp_lcmon_t3_anti_shake;
	int ap_temp_lcmon_t2;
	int ap_temp_lcmon_t2_anti_shake;
	int ap_temp_lcmon_t1;
	int ap_temp_lcmon_t1_anti_shake;
	int ap_temp_lcmon_t0;
	int ap_temp_lcmon_t0_anti_shake;

	int lcd_on_ibat[TEMP_MAX];
	int lcd_off_ibat[TEMP_MAX];

#ifdef CONFIG_N28_CHARGER_PRIVATE
	//fast charge
	//lcm off
	int ap_temp_t4_cp_thres;
	int ap_temp_t4_cp_thres_minus_x_degree;
	int ap_temp_t3_cp_thres;
	int ap_temp_t3_cp_thres_minus_x_degree;
	int ap_temp_t2_cp_thres;
	int ap_temp_t2_cp_thres_minus_x_degree;
	int ap_temp_t1_cp_thres;
	int ap_temp_t1_cp_thres_minus_x_degree;
	int ap_temp_t0_cp_thres;
	int ap_temp_t0_cp_thres_minus_x_degree;
	int lcd_off_cp_ibat[TEMP_MAX];
#endif
	//int ap_temp_thres_lcmon;
	//int ap_temp_thres_minus_x_degree_lcmon;
	//int ap_temp_cp_thres_lcmon;
	//int ap_temp_cp_thres_minus_x_degree_lcmon;
#endif

	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;

	/*yuanjian.wt, start 20191123, add for AFC charge*/
	/*AFC*/
	int afc_start_battery_soc;
	int afc_stop_battery_soc;
	int afc_ichg_level_threshold;
	int afc_pre_input_current;
	int afc_charger_input_current;
	int afc_charger_current;
	int afc_common_charger_input_curr;
	int afc_common_charger_curr;
	int afc_min_charger_voltage;
	int afc_max_charger_voltage;
	/*yuanjian.wt, End 20191123, add for AFC charge*/

};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	bool thermal_throttle_record;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

enum chg_data_idx_enum {
	CHG1_SETTING,
	CHG2_SETTING,
	DVCHG1_SETTING,
	DVCHG2_SETTING,
	HVDVCHG1_SETTING,
	HVDVCHG2_SETTING,
	CHGS_SETTING_MAX,
};

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;
	struct charger_device *dvchg1_dev;
	struct notifier_block dvchg1_nb;
	struct charger_device *dvchg2_dev;
	struct notifier_block dvchg2_nb;
	struct charger_device *hvdvchg1_dev;
	struct notifier_block hvdvchg1_nb;
	struct charger_device *hvdvchg2_dev;
	struct notifier_block hvdvchg2_nb;
	struct charger_device *bkbstchg_dev;
	struct notifier_block bkbstchg_nb;
	struct charger_device *cschg1_dev;
	struct notifier_block cschg1_nb;
	struct charger_device *cschg2_dev;
	struct notifier_block cschg2_nb;


	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	struct power_supply_desc psy_desc3;
	struct power_supply_config psy_cfg3;
	struct power_supply *psy3;
#endif

	struct power_supply_desc psy_dvchg_desc1;
	struct power_supply_config psy_dvchg_cfg1;
	struct power_supply *psy_dvchg1;

	struct power_supply_desc psy_dvchg_desc2;
	struct power_supply_config psy_dvchg_cfg2;
	struct power_supply *psy_dvchg2;

	struct power_supply_desc psy_hvdvchg_desc1;
	struct power_supply_config psy_hvdvchg_cfg1;
	struct power_supply *psy_hvdvchg1;

	struct power_supply_desc psy_hvdvchg_desc2;
	struct power_supply_config psy_hvdvchg_cfg2;
	struct power_supply *psy_hvdvchg2;

/* S96818AA3 gujiayin.wt,modify,2024/08/21 add psy node start*/
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
	struct power_supply_desc otg_desc;
	struct power_supply_config otg_cfg;
	struct power_supply *otg_psy;
#endif
/* S96818AA3 gujiayin.wt,modify,2024/08/21 add psy node end*/

	struct power_supply  *chg_psy;
	struct power_supply  *bc12_psy;
	struct power_supply  *bat_psy;
	struct power_supply  *bat2_psy;
	struct power_supply  *bat_manager_psy;
	struct adapter_device *select_adapter;
	struct adapter_device *pd_adapter;
	struct adapter_device *adapter_dev[MAX_TA_IDX];
	struct notifier_block *nb_addr;
	struct info_notifier_block ta_nb[MAX_TA_IDX];
	struct adapter_device *ufcs_adapter;
	struct mutex pd_lock;
	struct mutex ufcs_lock;
	struct mutex ta_lock;

#ifdef CONFIG_AFC_CHARGER
	/*AFC*/
	bool enable_afc;
	struct afc_dev afc;
#endif

	u32 bootmode;
	u32 boottype;

	int ta_status[MAX_TA_IDX];
	int select_adapter_idx;
	int ta_hardreset;
	int chr_type;
	int usb_type;
	int usb_state;
	int adapter_priority;
	int ta_type;

	struct mutex cable_out_lock;
	int cable_out_cnt;

	/* system lock */
	spinlock_t slock;
	struct wakeup_source *charger_wakelock;
	struct mutex charger_lock;

	/* thread related */
	wait_queue_head_t  wait_que;
	bool charger_thread_timeout;
	unsigned int polling_interval;
	bool charger_thread_polling;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec64 endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int log_level;
	bool usb_unlimited;
	bool charger_unlimited;
	bool disable_charger;
	bool disable_aicl;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	int safety_timer_cmd;
	bool vbusov_stat;
	bool dpdmov_stat;
	bool lst_dpdmov_stat;
	bool is_chg_done;
	/* ATM */
	bool atm_enabled;

	const char *algorithm_name;
	const char *curr_select_name;
	struct mtk_charger_algorithm algo;

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec64 charging_begin_time;

	/* vbat monitor, 6pin bat */
	bool batpro_done;
	bool enable_vbat_mon;
	bool enable_vbat_mon_bak;
	int old_cv;
	bool stop_6pin_re_en;
	int vbat0_flag;

	/*yuanjian.wt, start 20191123, add for AFC charge*/
	/*AFC*/
	int afc_start_battery_soc;
	int afc_stop_battery_soc;
	int afc_ichg_level_threshold;
	int afc_pre_input_current;
	int afc_charger_input_current;
	int afc_charger_current;
	int afc_min_charger_voltage;
	int afc_max_charger_voltage;
	/*yuanjian.wt, End 20191123, add for AFC charge*/

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	struct delayed_work psy_update_dwork;
	struct delayed_work late_init_work;
#endif
	atomic_t batt_full_discharge;
	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	int lst_rnd_alg_idx;
	bool alg_new_arbitration;
	bool alg_unchangeable;
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* water detection */
	bool water_detected;
	bool record_water_detected;

	bool enable_dynamic_mivr;

	/* fast charging algo support indicator */
	bool enable_fast_charging_indicator;
	unsigned int fast_charging_indicator;

	/* diasable meta current limit for testing */
	unsigned int enable_meta_current_limit;

	/* set current selector parallel mode */
	int cs_heatlim;
	unsigned int cs_para_mode;
	int cs_gpio_index;
	bool cs_hw_disable;
	int dual_chg_stat;
	int cs_cc_now;
	int comp_resist;
	struct smartcharging sc;
	bool cs_with_gauge;

	/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_scd_pid;
	struct scd_cmd_param_t_1 sc_data;

	/*charger IC charging status*/
	bool is_charging;
	bool is_cs_chg_done;

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	bool afc_flag;
	bool pd_flag;
	int typec_cc_orientation;
	int real_charge_type;
	int shipmode;
	struct notifier_block charger_nb;
	int batt_charging_source;
#endif
#if defined (CHARGING_THERMAL_ENABLE) || defined (CHARGING_15W_THERMAL_ENABLE)
	struct notifier_block fb_notif;
	bool lcmoff;
#endif
#if defined (CHARGING_THERMAL_ENABLE)
	int ap_temp;
	struct sw_jeita_data ap_thermal_data;
#endif
#if defined (CHARGING_15W_THERMAL_ENABLE)
	int old_thermal_charging_current_limit;
	atomic_t thermal_current_update;
#endif

	ktime_t uevent_time_check;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	int batt_full_capacity;
	int batt_soc_rechg;
#endif

#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
	bool disable_quick_charge;
#endif
	bool force_disable_pp[CHG2_SETTING + 1];
	bool enable_pp[CHG2_SETTING + 1];
	struct mutex pp_lock[CHG2_SETTING + 1];
	int cmd_pp;

	/* enable boot volt*/
	bool enable_boot_volt;
	bool reset_boot_volt_times;

	/* adapter switch control */
	int protocol_state;
	int ta_capability;
	int wait_times;
	bool is_soc_100_in_charging;
	bool is_basic_discharge;
	int batt_status;
	bool is_enable_term;
};

static inline int mtk_chg_alg_notify_call(struct mtk_charger *info,
					  enum chg_alg_notifier_events evt,
					  int value)
{
	int i;
	struct chg_alg_notify notify = {
		.evt = evt,
		.value = value,
	};

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i])
			chg_alg_notifier_call(info->alg[i], &notify);
	}
	return 0;
}

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int set_battery_temperature(struct mtk_charger *info, union power_supply_propval batt_temp);
extern int get_battery_current(struct mtk_charger *info);
extern int get_cs_side_battery_current(struct mtk_charger *info, int *ibat);
extern int get_cs_side_battery_voltage(struct mtk_charger *info, int *vbat);
extern int get_chg_output_vbat(struct mtk_charger *info, int *vbat);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibat(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int get_usb_type(struct mtk_charger *info);
extern int disable_hw_ovp(struct mtk_charger *info, int en);
extern bool is_charger_exist(struct mtk_charger *info);
extern int get_charger_temperature(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_charging_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_input_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_zcv(struct mtk_charger *info,
	struct charger_device *chg);
extern void _wake_up_charger(struct mtk_charger *info);
extern int mtk_adapter_switch_control(struct mtk_charger *info);
extern int mtk_selected_adapter_ready(struct mtk_charger *info);
extern int mtk_adapter_protocol_init(struct mtk_charger *info);
extern void mtk_check_ta_status(struct mtk_charger *info);
/* functions for other */
extern int mtk_chg_enable_vbus_ovp(bool enable);
#if defined (CONFIG_N28_CHARGER_PRIVATE) || defined (CONFIG_W2_CHARGER_PRIVATE)
extern int adapter_is_support_pd_pps(struct mtk_charger *info);
extern bool batt_store_mode;
#endif
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
int wt_batt_full_capacity_check_for_cp(void);
#endif

#define ONLINE(idx, attach)		((idx & 0xf) << 4 | (attach & 0xf))
#define ONLINE_GET_IDX(online)		((online >> 4) & 0xf)
#define ONLINE_GET_ATTACH(online)	(online & 0xf)

#endif /* __MTK_CHARGER_H */
