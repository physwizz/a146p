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

struct xinxian {
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

#define xinxian_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		xinxian_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define xinxian_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		xinxian_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct xinxian *panel_to_xinxian(struct drm_panel *panel)
{
	return container_of(panel, struct xinxian, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int xinxian_dcs_read(struct xinxian *ctx, u8 cmd, void *data, size_t len)
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

static void xinxian_panel_get_data(struct xinxian *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = xinxian_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void xinxian_dcs_write(struct xinxian *ctx, const void *data, size_t len)
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
u8 esd_bl6[] = {0x00, 0x00};

static void xinxian_panel_init(struct xinxian *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12 * 1000, 12 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%sicnl9916c 1074-1066-vfp\n", __func__);

	xinxian_dcs_write_seq_static(ctx,0xF0,0x99,0x16,0x0C);
	xinxian_dcs_write_seq_static(ctx,0xC0,0x40,0x93,0xFF,0xFF,0xFF,0x3F,0xFF,0x00,0xFF,0x00,0xCC,0xB1,0x23,0x45,0x67,0x89,0xAD,0xFF,0xFF,0xF0);
	xinxian_dcs_write_seq_static(ctx,0xC1,0x00,0x20,0x20,0xBE,0x04,0x6E,0x74,0x04,0x40,0x06,0x22,0x70,0x30,0x20,0x07,0x11,0x84,0x4C,0x00,0x93,0x13,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	xinxian_dcs_write_seq_static(ctx,0xC2,0x00);
	xinxian_dcs_write_seq_static(ctx,0xC3,0x06,0x00,0xFF,0x00,0xFF,0x5C,0x10,0x01);
	xinxian_dcs_write_seq_static(ctx,0xC4,0x04,0x31,0xB8,0x40,0x00,0xBC,0x00,0x00,0x00,0x00,0x00,0xF0);
	xinxian_dcs_write_seq_static(ctx,0xC5,0x03,0x21,0x96,0xC8,0x3E,0x00,0x09,0x01,0x14,0x04,0x0F,0x18,0xC6,0x03,0x64,0xFF,0x01,0x04,0x18,0x22,0x45,0x14,0x38);
	xinxian_dcs_write_seq_static(ctx,0xC6,0x72,0x24,0x13,0x2B,0x2B,0x28,0x3F,0x02,0x16,0x96,0x00,0x01);
	xinxian_dcs_write_seq_static(ctx,0xCA,0x27,0x40,0x04,0x19,0x46,0x94,0x41,0x8F,0x22,0x33,0x52,0x64,0x3E,0x34,0x5A,0x6E,0x46,0x3C,0x11,0x00,0x01,0x01,0x08,0x01,0x06,0x00,0x05,0x00,0x0A,0x04);
	xinxian_dcs_write_seq_static(ctx,0xB2,0x70,0x11,0x09,0x10,0x96,0x36,0x0B,0x07,0x55,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0xD6,0xA5,0xD6,0xA5,0x00,0x00,0x00);
	xinxian_dcs_write_seq_static(ctx,0xB3,0xB5,0x07,0x02,0x0F,0x81,0xD6,0x00,0x00,0xA5,0x08,0x00);
	xinxian_dcs_write_seq_static(ctx,0xB4,0x00,0x11,0x15,0x11,0x15,0x26,0x26,0x91,0xA2,0x33,0x44,0x00,0x26,0x00,0x55,0x3C,0x02,0x08,0x20,0x30,0x00);
	xinxian_dcs_write_seq_static(ctx,0xB5,0x00,0x00,0x32,0x32,0x00,0x00,0x33,0x00,0x00,0x00,0x33,0x0D,0x0F,0x11,0x13,0x15,0x17,0x09,0x07,0x05,0x00,0x00,0xFF,0xFF,0xFC,0x32,0x20,0x00,0x00,0x00,0x00,0x00);
	xinxian_dcs_write_seq_static(ctx,0xB6,0x00,0x00,0x32,0x32,0x00,0x00,0x33,0x00,0x00,0x00,0x33,0x0C,0x0E,0x10,0x12,0x14,0x16,0x08,0x06,0x04,0x00,0x00,0xFF,0xFF,0xFC,0x32,0x20,0x00,0x00,0x00,0x00,0x00);
	xinxian_dcs_write_seq_static(ctx,0xB7,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A);
	xinxian_dcs_write_seq_static(ctx,0xBB,0x01,0x02,0x03,0x0A,0x04,0x13,0x14,0x00);
	xinxian_dcs_write_seq_static(ctx,0xBC,0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0xF0,0x0B,0x33,0x54,0x5B,0x33,0x33,0x00,0x5A,0x46,0x52,0x3E);
	xinxian_dcs_write_seq_static(ctx,0xBD,0xA1,0x0A,0x52,0xA6,0x8F,0x04,0xC7,0xC1,0x06,0x03,0x2E,0x15,0x01);
	xinxian_dcs_write_seq_static(ctx,0xBF,0x0C,0x19,0x0C,0x19,0x00,0x11,0x04,0x18,0x50);
	xinxian_dcs_write_seq_static(ctx,0xC7,0x76,0x54,0x32,0x22,0x34,0x56,0x77,0x77,0x30,0x76,0x54,0x32,0x22,0x34,0x56,0x77,0x77,0x10,0x31,0x00,0x01,0xFF,0xFF,0x00,0x82,0x91,0x40);
	xinxian_dcs_write_seq_static(ctx,0x80,0xFF,0xFA,0xF3,0xEC,0xE7,0xE3,0xDF,0xDB,0xD7,0xCC,0xC3,0xBB,0xB5,0xAF,0xA8,0x9E,0x94,0x8A,0x7E,0x7D,0x72,0x68,0x5D,0x50,0x40,0x2E,0x29,0x1A,0x16,0x13,0x0E,0x0B);
	xinxian_dcs_write_seq_static(ctx,0x81,0xFF,0xFA,0xF3,0xEC,0xE7,0xE3,0xDF,0xDB,0xD7,0xCC,0xC3,0xBB,0xB5,0xAF,0xA8,0x9E,0x94,0x8A,0x7E,0x7D,0x72,0x68,0x5D,0x50,0x40,0x2E,0x29,0x1A,0x16,0x13,0x0E,0x0B);
	xinxian_dcs_write_seq_static(ctx,0x82,0xFF,0xFA,0xF3,0xEC,0xE7,0xE3,0xDF,0xDB,0xD7,0xCC,0xC3,0xBB,0xB5,0xAF,0xA8,0x9E,0x94,0x8A,0x7E,0x7D,0x72,0x68,0x5D,0x50,0x40,0x2E,0x29,0x1A,0x16,0x13,0x0E,0x0B);
	xinxian_dcs_write_seq_static(ctx,0x83,0x03,0x08,0x05,0x02,0x02,0x08,0x05,0x02,0x02,0x08,0x05,0x02,0x02,0x06,0x03,0x01,0x00,0x06,0x03,0x01,0x01,0x06,0x03,0x01,0x01);
	xinxian_dcs_write_seq_static(ctx,0x84,0x11,0xD2,0x42,0x59,0x2E,0x44,0x24,0xCD,0xEE,0x11,0xD2,0x42,0x59,0x2E,0x44,0x24,0xCD,0xEE,0x11,0xD2,0x42,0x59,0x2E,0x44,0x24,0xCD,0xEE);
	xinxian_dcs_write_seq_static(ctx,0x85,0xFB,0xF6,0xEE,0xE7,0xE3,0xDE,0xDA,0xD6,0xD2,0xC7,0xBE,0xB6,0xAF,0xA8,0xA1,0x97,0x8B,0x80,0x74,0x73,0x67,0x5D,0x52,0x44,0x37,0x2B,0x1B,0x10,0x0F,0x0D,0x0B,0x08);
	xinxian_dcs_write_seq_static(ctx,0x86,0xFB,0xF6,0xEE,0xE7,0xE3,0xDE,0xDA,0xD6,0xD2,0xC7,0xBE,0xB6,0xAF,0xA8,0xA1,0x97,0x8B,0x80,0x74,0x73,0x67,0x5D,0x52,0x44,0x37,0x2B,0x1B,0x10,0x0F,0x0D,0x0B,0x08);
	xinxian_dcs_write_seq_static(ctx,0x87,0xFB,0xF6,0xEE,0xE7,0xE3,0xDE,0xDA,0xD6,0xD2,0xC7,0xBE,0xB6,0xAF,0xA8,0xA1,0x97,0x8B,0x80,0x74,0x73,0x67,0x5D,0x52,0x44,0x37,0x2B,0x1B,0x10,0x0F,0x0D,0x0B,0x08);
	xinxian_dcs_write_seq_static(ctx,0x8B,0x1F,0x7B,0x81,0xE8,0x55,0xD7,0x67,0x06,0x64,0x1F,0x7B,0x81,0xE8,0x55,0xD7,0x67,0x06,0x64,0x1F,0x7B,0x81,0xE8,0x55,0xD7,0x67,0x06,0x64);
	xinxian_dcs_write_seq_static(ctx,0xC8,0x46,0x00,0x08,0xF6);
	xinxian_dcs_write_seq_static(ctx,0xE0,0x0C,0x00,0xB0,0x10,0x00,0x0A,0x8C);
	xinxian_dcs_write_seq_static(ctx,0x35,0x00,0x00);
	xinxian_dcs_write_seq_static(ctx,0x53,0x2C,0x00);
	xinxian_dcs_write_seq_static(ctx,0x11);
	msleep(120);
	/* Display On*/
	xinxian_dcs_write_seq_static(ctx,0x29);
	msleep(20);
	xinxian_dcs_write_seq_static(ctx,0x6D,0x02,0x00);
	xinxian_dcs_write_seq_static(ctx,0xBD,0xA1,0x0A,0x52,0xAE);
	xinxian_dcs_write_seq_static(ctx,0xF0,0x00,0x00,0x00);

	// if(1 == rec_esd) {
		// pr_info("recover_esd_set_backlight\n");
		// esd_bl6[0] = (last_brightness & 0xF00) >> 8;
		// esd_bl6[1] = (last_brightness & 0xFF);
		// xinxian_dcs_write_seq(ctx, 0x51, esd_bl6[0], esd_bl6[1]);
	// }

	pr_info("%s-\n", __func__);
}

static int xinxian_disable(struct drm_panel *panel)
{
	struct xinxian *ctx = panel_to_xinxian(panel);

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
static int xinxian_unprepare(struct drm_panel *panel)
{

	struct xinxian *ctx = panel_to_xinxian(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;
	//panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[FTS] tpd_focaltech_notifier_callback in xinxian-unprepare\n ");

	if (n28_gesture_status == 1) {
		printk("[FTS-lcm]xinxian_unprepare n28_gesture_status = 1\n ");
		xinxian_dcs_write_seq_static(ctx,0X6d,0x25,0x00);
		xinxian_dcs_write_seq_static(ctx,0X28,0x00,0x00);
		msleep(20);
		xinxian_dcs_write_seq_static(ctx,0X10,0x00,0x00);
		msleep(120);
	} else {
		printk("[FTS-lcm]xinxian_unprepare n28_gesture_status = 0\n ");

		xinxian_dcs_write_seq_static(ctx,0X6d,0x25,0x00);
		xinxian_dcs_write_seq_static(ctx,0X28,0x00,0x00);
		msleep(20);
		xinxian_dcs_write_seq_static(ctx,0X10,0x00,0x00);
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

static int xinxian_prepare(struct drm_panel *panel)
{
	struct xinxian *ctx = panel_to_xinxian(panel);
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

	msleep(8);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0x14);
	_lcm_i2c_write_bytes(AVEE_REG, 0x14);
	msleep(2);
	xinxian_panel_init(ctx);

	//add for TP resume
	//panel_notifier_call_chain(PANEL_PREPARE, NULL);
	//printk("[FTS] tpd_focaltech_notifier_callback in xinxian_prepare\n ");

	ret = ctx->error;
	if (ret < 0)
		xinxian_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	xinxian_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int xinxian_enable(struct drm_panel *panel)
{
	struct xinxian *ctx = panel_to_xinxian(panel);

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
	.clock = 104082,
	.hdisplay = 720,
	.hsync_start = 720 + 116,
	.hsync_end = 720 + 116 + 4,
	.htotal = 720 + 116 + 4 + 110,
	.vdisplay = 1600,
	.vsync_start = 1600 + 190,
	.vsync_end = 1600 + 190 + 4,
	.vtotal = 1600 + 190 + 4 + 32,
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

static int xinxian_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	unsigned int bl_lvl;
	char bl_tb0[] = {0x51, 0x0f, 0xff};

	bl_lvl = wingtech_bright_to_bl(level,1023,52,3685,48);
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
	struct xinxian *ctx = panel_to_xinxian(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = xinxian_setbacklight_cmdq,
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

static int xinxian_get_modes(struct drm_panel *panel,
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

static const struct drm_panel_funcs xinxian_drm_funcs = {
	.disable = xinxian_disable,
	.unprepare = xinxian_unprepare,
	.prepare = xinxian_prepare,
	.enable = xinxian_enable,
	.get_modes = xinxian_get_modes,
};

static int xinxian_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct xinxian *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct xinxian), GFP_KERNEL);
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
	drm_panel_init(&ctx->panel, dev, &xinxian_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &xinxian_drm_funcs;

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

	pr_info("%s- wt,xinxian,icnl9916c,hdp,vdo,60hz\n", __func__);

	return ret;
}

static void xinxian_remove(struct mipi_dsi_device *dsi)
{
	struct xinxian *ctx = mipi_dsi_get_drvdata(dsi);
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

static void xinxian_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("%s++\n", __func__);

	//fts_gestrue_status = 0;
	//pr_info("optimize shutdown sequence fts_gestrue_status is 0 %s++\n", __func__);
}

static const struct of_device_id xinxian_of_match[] = {
	{
	    .compatible = "wt,n28_icnl9916c_dsi_vdo_hdp_xinxian_hkc",
	},
	{}
};

MODULE_DEVICE_TABLE(of, xinxian_of_match);

static struct mipi_dsi_driver xinxian_driver = {
	.probe = xinxian_probe,
	.remove = xinxian_remove,
	.shutdown = xinxian_panel_shutdown,
	.driver = {
		.name = "n28_icnl9916c_dsi_vdo_hdp_xinxian_hkc",
		.owner = THIS_MODULE,
		.of_match_table = xinxian_of_match,
	},
};

static int __init xinxian_drv_init(void)
{
	int ret = 0;

	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	ret = mipi_dsi_driver_register(&xinxian_driver);
	if (ret < 0)
		pr_notice("%s, Failed to register boe driver: %d\n", __func__, ret);

	mtk_panel_unlock();
	pr_notice("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit xinxian_drv_exit(void)
{
	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	mipi_dsi_driver_unregister(&xinxian_driver);
	mtk_panel_unlock();
	pr_notice("%s-\n", __func__);
}
module_init(xinxian_drv_init);
module_exit(xinxian_drv_exit);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt xinxian icnl9916c vdo Panel Driver");
MODULE_LICENSE("GPL v2");

