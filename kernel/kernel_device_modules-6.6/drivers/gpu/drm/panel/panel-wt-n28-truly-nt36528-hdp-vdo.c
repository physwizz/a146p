/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "include/panel-tianma-nt36672e-vdo-120hz-hfp.h"
#endif

#if defined(CONFIG_RT4831A_I2C)
#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
#endif

#include "../../../misc/mediatek/gate_ic/lcm_cust_common.h"
// extern u8 rec_esd;	//ESD recover flag global value

struct truly {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
    struct gpio_desc *pm_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define truly_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		truly_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define truly_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		truly_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct truly *panel_to_truly(struct drm_panel *panel)
{
	return container_of(panel, struct truly, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int truly_dcs_read(struct truly *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void truly_panel_get_data(struct truly *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = truly_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void truly_dcs_write(struct truly *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static int last_brightness = 0;	//the last backlight level values
u8 esd_bl1[] = {0x00, 0x00};

static void truly_panel_init(struct truly *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 10 * 1000);
	usleep_range(1 * 1000, 1 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%snt36528 1074-1066-vfp\n", __func__);

	truly_dcs_write_seq_static(ctx,0xFF,0x20);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0x05,0xAB);
	//{0x07,0xC8);//default=C8h,VGH=VGHO=15V //IC default design VGH=VGHO,VGL=VGLO
	truly_dcs_write_seq_static(ctx,0x08,0x64);//VGL=VGLO=-10V
	truly_dcs_write_seq_static(ctx,0x0D,0x80);
	truly_dcs_write_seq_static(ctx,0x1F,0x33);//230424 update Gate CTRL
	truly_dcs_write_seq_static(ctx,0x6D,0x22);//230425 update SD CTRL 1
	truly_dcs_write_seq_static(ctx,0x80,0x64);
	truly_dcs_write_seq_static(ctx,0x94,0x80);//VOP[8]//修正C0->80
	truly_dcs_write_seq_static(ctx,0x95,0x13);//GVDDP=5.6V //20230518 update
	truly_dcs_write_seq_static(ctx,0x96,0xFF);//GVDDN=-5.4V //20230518 update

	truly_dcs_write_seq_static(ctx,0xFF,0x23);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0x11,0x00);
	truly_dcs_write_seq_static(ctx,0x12,0xB4);
	truly_dcs_write_seq_static(ctx,0x15,0xE9);
	truly_dcs_write_seq_static(ctx,0x16,0x14);
	truly_dcs_write_seq_static(ctx,0x00,0x60);//PWM set 11bit
	truly_dcs_write_seq_static(ctx,0x07,0x00);
	truly_dcs_write_seq_static(ctx,0x09,0xA4);//PWM set 20khz

	truly_dcs_write_seq_static(ctx,0xFF,0x24);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0xF1,0x2B);//reduce peak overshoot of bias current
	truly_dcs_write_seq_static(ctx,0x00,0x26);
	truly_dcs_write_seq_static(ctx,0x01,0x26);
	truly_dcs_write_seq_static(ctx,0x02,0x05);
	truly_dcs_write_seq_static(ctx,0x03,0x05);
	truly_dcs_write_seq_static(ctx,0x04,0x26);
	truly_dcs_write_seq_static(ctx,0x05,0x26);
	truly_dcs_write_seq_static(ctx,0x06,0x0F);
	truly_dcs_write_seq_static(ctx,0x07,0x0F);
	truly_dcs_write_seq_static(ctx,0x08,0x0E);
	truly_dcs_write_seq_static(ctx,0x09,0x0E);
	truly_dcs_write_seq_static(ctx,0x0A,0x0D);
	truly_dcs_write_seq_static(ctx,0x0B,0x0D);
	truly_dcs_write_seq_static(ctx,0x0C,0x0C);
	truly_dcs_write_seq_static(ctx,0x0D,0x0C);
	truly_dcs_write_seq_static(ctx,0x0E,0x04);
	truly_dcs_write_seq_static(ctx,0x0F,0x04);
	truly_dcs_write_seq_static(ctx,0x10,0x03);
	truly_dcs_write_seq_static(ctx,0x11,0x03);
	truly_dcs_write_seq_static(ctx,0x12,0x03);
	truly_dcs_write_seq_static(ctx,0x13,0x03);
	truly_dcs_write_seq_static(ctx,0x14,0x03);
	truly_dcs_write_seq_static(ctx,0x15,0x03);
	truly_dcs_write_seq_static(ctx,0x16,0x26);
	truly_dcs_write_seq_static(ctx,0x17,0x26);
	truly_dcs_write_seq_static(ctx,0x18,0x05);
	truly_dcs_write_seq_static(ctx,0x19,0x05);
	truly_dcs_write_seq_static(ctx,0x1A,0x26);
	truly_dcs_write_seq_static(ctx,0x1B,0x26);
	truly_dcs_write_seq_static(ctx,0x1C,0x0F);
	truly_dcs_write_seq_static(ctx,0x1D,0x0F);
	truly_dcs_write_seq_static(ctx,0x1E,0x0E);
	truly_dcs_write_seq_static(ctx,0x1F,0x0E);
	truly_dcs_write_seq_static(ctx,0x20,0x0D);
	truly_dcs_write_seq_static(ctx,0x21,0x0D);
	truly_dcs_write_seq_static(ctx,0x22,0x0C);
	truly_dcs_write_seq_static(ctx,0x23,0x0C);
	truly_dcs_write_seq_static(ctx,0x24,0x04);
	truly_dcs_write_seq_static(ctx,0x25,0x04);
	truly_dcs_write_seq_static(ctx,0x26,0x03);
	truly_dcs_write_seq_static(ctx,0x27,0x03);
	truly_dcs_write_seq_static(ctx,0x28,0x03);
	truly_dcs_write_seq_static(ctx,0x29,0x03);
	truly_dcs_write_seq_static(ctx,0x2A,0x03);
	truly_dcs_write_seq_static(ctx,0x2B,0x03);
	truly_dcs_write_seq_static(ctx,0x2F,0x07);
	truly_dcs_write_seq_static(ctx,0x30,0x30);
	truly_dcs_write_seq_static(ctx,0x37,0x55);
	truly_dcs_write_seq_static(ctx,0x3A,0x8C);
	truly_dcs_write_seq_static(ctx,0x3B,0x8C);
	truly_dcs_write_seq_static(ctx,0x3D,0x92);
	truly_dcs_write_seq_static(ctx,0x55,0x84);
	truly_dcs_write_seq_static(ctx,0x56,0x04);
	truly_dcs_write_seq_static(ctx,0x5E,0x00, 0x08);
	truly_dcs_write_seq_static(ctx,0x59,0x50);
	truly_dcs_write_seq_static(ctx,0x5A,0xC5);//230704
	truly_dcs_write_seq_static(ctx,0x5B,0xAE);//230704 update GCK_falling
	truly_dcs_write_seq_static(ctx,0x5C,0x9F);//230425 update GCKR_EQT1
	truly_dcs_write_seq_static(ctx,0x5D,0x0F);//230425 update GCKF_EQT1
	truly_dcs_write_seq_static(ctx,0x92,0xD3);
	truly_dcs_write_seq_static(ctx,0x93,0xB5, 0x00);//VFP=181
	truly_dcs_write_seq_static(ctx,0x94,0x0C);//VBP+VSA=12
	truly_dcs_write_seq_static(ctx,0xDC,0xCE);
	truly_dcs_write_seq_static(ctx,0xE0,0xCE);
	truly_dcs_write_seq_static(ctx,0xE2,0xCE);
	truly_dcs_write_seq_static(ctx,0xE4,0xCE);
	truly_dcs_write_seq_static(ctx,0xE6,0xCE);
	truly_dcs_write_seq_static(ctx,0xEA,0xCE);
	truly_dcs_write_seq_static(ctx,0xEE,0xCE);
	truly_dcs_write_seq_static(ctx,0xF0,0xCE);

	truly_dcs_write_seq_static(ctx,0xFF,0x25);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0x02,0x00, 0x40, 0x00, 0x40);
	truly_dcs_write_seq_static(ctx,0x13,0x02);
	truly_dcs_write_seq_static(ctx,0x14,0x7F);

	truly_dcs_write_seq_static(ctx,0xFF,0x26);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0x00,0x80);
	truly_dcs_write_seq_static(ctx,0x0A,0x05);
	truly_dcs_write_seq_static(ctx,0x19,0x0B);
	truly_dcs_write_seq_static(ctx,0x1A,0x4D);
	truly_dcs_write_seq_static(ctx,0x1B,0x0A);
	truly_dcs_write_seq_static(ctx,0x1C,0xC2);
	truly_dcs_write_seq_static(ctx,0x1E,0xD3);
	truly_dcs_write_seq_static(ctx,0x1F,0xD3);
	truly_dcs_write_seq_static(ctx,0x2A,0x0B);
	truly_dcs_write_seq_static(ctx,0x2B,0x4D);
	truly_dcs_write_seq_static(ctx,0xCB,0x0A);
	truly_dcs_write_seq_static(ctx,0xD6,0x9C);
	truly_dcs_write_seq_static(ctx,0xD7,0x9E);
	truly_dcs_write_seq_static(ctx,0xD8,0xA0);
	truly_dcs_write_seq_static(ctx,0xD9,0xA2);
	truly_dcs_write_seq_static(ctx,0xDA,0xA4);
	truly_dcs_write_seq_static(ctx,0xDB,0xA6);
	truly_dcs_write_seq_static(ctx,0xDC,0xA8);
	truly_dcs_write_seq_static(ctx,0xDD,0xAA);

	truly_dcs_write_seq_static(ctx,0xFF,0x27);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0x78,0x86);
	truly_dcs_write_seq_static(ctx,0x79,0x05);
	truly_dcs_write_seq_static(ctx,0x7A,0x04);
	truly_dcs_write_seq_static(ctx,0x7C,0x78);
	truly_dcs_write_seq_static(ctx,0x7D,0x8C);
	truly_dcs_write_seq_static(ctx,0x83,0x08);
	truly_dcs_write_seq_static(ctx,0x84,0x30);
	truly_dcs_write_seq_static(ctx,0x93,0x60);
	truly_dcs_write_seq_static(ctx,0x96,0x19);
	truly_dcs_write_seq_static(ctx,0x98,0x88);
	truly_dcs_write_seq_static(ctx,0xA9,0x28);
	truly_dcs_write_seq_static(ctx,0xB4,0x10);
	truly_dcs_write_seq_static(ctx,0xC4,0x78);
	truly_dcs_write_seq_static(ctx,0xC5,0x8C);
	truly_dcs_write_seq_static(ctx,0xE4,0x00);
	truly_dcs_write_seq_static(ctx,0xE5,0x9D);
	truly_dcs_write_seq_static(ctx,0x58,0xAC);
	truly_dcs_write_seq_static(ctx,0x59,0x00, 0x06);
	truly_dcs_write_seq_static(ctx,0x5A,0x00, 0x01);
	truly_dcs_write_seq_static(ctx,0x5D,0x00, 0x01);
	truly_dcs_write_seq_static(ctx,0x5E,0x00, 0xB4);
	truly_dcs_write_seq_static(ctx,0x5F,0x01);
	truly_dcs_write_seq_static(ctx,0x60,0x06, 0x60);
	truly_dcs_write_seq_static(ctx,0xC0,0x08);
	truly_dcs_write_seq_static(ctx,0xEF,0x08);
	truly_dcs_write_seq_static(ctx,0xF4,0x05);

	truly_dcs_write_seq_static(ctx,0xFF,0x2A);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0xA2,0x33);
	truly_dcs_write_seq_static(ctx,0xA4,0x03);
	truly_dcs_write_seq_static(ctx,0x23,0x08);
	truly_dcs_write_seq_static(ctx,0x28,0xB5);
	truly_dcs_write_seq_static(ctx,0x2D,0xB5);
	truly_dcs_write_seq_static(ctx,0x99,0x95);
	truly_dcs_write_seq_static(ctx,0x9A,0x0B);
	truly_dcs_write_seq_static(ctx,0xAC,0x00);
	truly_dcs_write_seq_static(ctx,0xAD,0x3C);
	truly_dcs_write_seq_static(ctx,0xAE,0x80);
	truly_dcs_write_seq_static(ctx,0xAF,0x43);
	truly_dcs_write_seq_static(ctx,0xB0,0x18);
	truly_dcs_write_seq_static(ctx,0xB1,0x90);
	truly_dcs_write_seq_static(ctx,0xB3,0x66);

	truly_dcs_write_seq_static(ctx,0xFF,0x10);
	truly_dcs_write_seq_static(ctx,0xFB,0x01);
	truly_dcs_write_seq_static(ctx,0x68,0x03, 0x01);//dimming 16/60s
	truly_dcs_write_seq_static(ctx,0x35,0x00);//TE
	truly_dcs_write_seq_static(ctx,0x53,0x2C);
	truly_dcs_write_seq_static(ctx,0x55,0x00);
	truly_dcs_write_seq_static(ctx, 0x11);
	msleep(100);
	/* Display On*/
	truly_dcs_write_seq_static(ctx, 0x29);
    msleep(20);

	// if(1 == rec_esd) {
		// pr_info("recover_esd_set_backlight\n");
		// esd_bl1[0] = (last_brightness & 0xFF0) >> 4;
		// esd_bl1[1] = (last_brightness & 0xF);
		// truly_dcs_write_seq(ctx, 0x51, esd_bl1[0], esd_bl1[1]);
	// }

	pr_info("%s-\n", __func__);
}

static int truly_disable(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

extern bool n28_gesture_status;
static int truly_unprepare(struct drm_panel *panel)
{

	struct truly *ctx = panel_to_truly(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;
	//panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[FTS] tpd_focaltech_notifier_callback in truly-unprepare\n ");

	if (n28_gesture_status == 1) {
		printk("[NVT-lcm]truly_unprepare n28_gesture_status = 1\n ");
		truly_dcs_write_seq_static(ctx,0x28,0x00);
		msleep(20);
		truly_dcs_write_seq_static(ctx,0x10,0x00);
		msleep(120);
	} else {
		printk("[NVT-lcm]truly_unprepare n28_gesture_status = 0\n ");

		truly_dcs_write_seq_static(ctx,0x28,0x00);
		msleep(20);
		truly_dcs_write_seq_static(ctx,0x10,0x00);
		msleep(120);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);
	}
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int truly_prepare(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	//ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->pm_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	//usleep_range(5000, 5001);

	// end
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	msleep(2);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0x14);
	_lcm_i2c_write_bytes(AVEE_REG, 0x14);
	msleep(2);
	truly_panel_init(ctx);

	//add for TP resume
	//panel_notifier_call_chain(PANEL_PREPARE, NULL);
	//printk("[FTS] tpd_focaltech_notifier_callback in truly_prepare\n ");

	ret = ctx->error;
	if (ret < 0)
		truly_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	truly_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int truly_enable(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 103492,
	.hdisplay = 720,
	.hsync_start = 720 + 180,
	.hsync_end = 720 + 180 + 12,
	.htotal = 720 + 180 + 12 + 50,
	.vdisplay = 1600,
	.vsync_start = 1600 + 181,
	.vsync_end = 1600 + 181 + 2,
	.vtotal = 1600 + 181 + 2 + 10,
};

static struct mtk_panel_params ext_params = {
	.pll_clk = 330,
	//.vfp_low_power = VFP_45HZ,
	.physical_width_um = 70380,
	.physical_height_um = 156240,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	/*
	.lcm_esd_check_table[1] = {
		.cmd = 0x0E, .count = 1, .para_list[0] = 0x80,
	},
	*/
	.data_rate = 660,
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int truly_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	unsigned int bl_lvl;
	char bl_tb0[] = {0x51, 0x0f, 0xff};

	bl_lvl = wingtech_bright_to_bl(level,1023,52,2047,48);
	pr_info("%s backlight: level = %d,bl_lvl=%d\n", __func__, level, bl_lvl);

	bl_tb0[1] = (bl_lvl & 0xF00) >> 8;
	bl_tb0[2] = (bl_lvl & 0xFF);
	pr_info("%s backlight: bl_tb0[1] = %x,bl_tb0[2] = %x\n", __func__, bl_tb0[1], bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	last_brightness = bl_lvl;
	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct truly *ctx = panel_to_truly(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = truly_setbacklight_cmdq,
	//.ext_param_set = mtk_panel_ext_param_set,
	//.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,
	.ata_check = panel_ata_check,
};

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int truly_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 156;
	return 1;
}

static const struct drm_panel_funcs truly_drm_funcs = {
	.disable = truly_disable,
	.unprepare = truly_unprepare,
	.prepare = truly_prepare,
	.enable = truly_enable,
	.get_modes = truly_get_modes,
};

static int truly_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct truly *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct truly), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	//ctx->pm_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	//if (IS_ERR(ctx->pm_gpio)) {
	//	dev_info(dev, "cannot get pm_gpio %ld\n",
	//		 PTR_ERR(ctx->pm_gpio));
	//	return PTR_ERR(ctx->pm_gpio);
	//}
	//devm_gpiod_put(dev, ctx->pm_gpio);
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
			 PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &truly_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &truly_drm_funcs;

	drm_panel_add(&ctx->panel);


	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- wt,truly,nt36528,hdp,vdo,60hz\n", __func__);

	return ret;
}

static void truly_remove(struct mipi_dsi_device *dsi)
{
	struct truly *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}

static void truly_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("%s++\n", __func__);

	//n28_gesture_status = 0;
	//pr_info("optimize shutdown sequence n28_gesture_status is 0 %s++\n", __func__);
}

static const struct of_device_id truly_of_match[] = {
	{
	    .compatible = "wt,n28_nt36528_dsi_vdo_hdp_truly_truly",
	},
	{}
};

MODULE_DEVICE_TABLE(of, truly_of_match);

static struct mipi_dsi_driver truly_driver = {
	.probe = truly_probe,
	.remove = truly_remove,
	.shutdown = truly_panel_shutdown,
	.driver = {
		.name = "n28_nt36528_dsi_vdo_hdp_truly_truly",
		.owner = THIS_MODULE,
		.of_match_table = truly_of_match,
	},
};

static int __init truly_drv_init(void)
{
	int ret = 0;

	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	ret = mipi_dsi_driver_register(&truly_driver);
	if (ret < 0)
		pr_notice("%s, Failed to register boe driver: %d\n", __func__, ret);

	mtk_panel_unlock();
	pr_notice("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit truly_drv_exit(void)
{
	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	mipi_dsi_driver_unregister(&truly_driver);
	mtk_panel_unlock();
	pr_notice("%s-\n", __func__);
}
module_init(truly_drv_init);
module_exit(truly_drv_exit);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt truly nt36528 vdo Panel Driver");
MODULE_LICENSE("GPL v2");

