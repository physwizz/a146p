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

struct dsbj {
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

#define dsbj_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		dsbj_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define dsbj_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		dsbj_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct dsbj *panel_to_dsbj(struct drm_panel *panel)
{
	return container_of(panel, struct dsbj, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int dsbj_dcs_read(struct dsbj *ctx, u8 cmd, void *data, size_t len)
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

static void dsbj_panel_get_data(struct dsbj *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = dsbj_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void dsbj_dcs_write(struct dsbj *ctx, const void *data, size_t len)
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
u8 esd_bl7[] = {0x00, 0x00};

static void dsbj_panel_init(struct dsbj *ctx)
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

	dsbj_dcs_write_seq_static(ctx,0xF0,0x99,0x16,0x0C);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x00,0x20,0x20,0xBE,0x04,0x6E,0x74,0x04,0x40,0x06,
	0x22,0x70,0x35,0x20,0x07,0x11,0x84,0x4C,0x00,0x93);
	dsbj_dcs_write_seq_static(ctx,0xC3,0x06,0x00,0xFF,0x00,0xFF,0x5C);
	dsbj_dcs_write_seq_static(ctx,0xC4,0x04,0x31,0xB8,0x40,0x00,0xBC,0x00,0x00,0x00,0x00,
	0x00,0xF0);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x03,0x21,0x96,0xC8,0x3E,0x00,0x04,0x01,0x14,0x04,
	0x19,0x0B,0xC6,0x03,0x64,0xFF,0x01,0x04,0x18,0x22,0x45,0x14,0x38);
	dsbj_dcs_write_seq_static(ctx,0xC6,0x89,0x24,0x17,0x2B,0x2B,0x28,0x3F,0x03,0x16,0xA5,
	0x00,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xB2,0x03,0x02,0x97,0x98,0x99,0x99,0x8B,0x01,0x49,0xB3,
	0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0x00,0x00,0x00,0x55,0x55,0x00,
	0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xB3,0xF2,0x01,0x02,0x09,0x81,0xAC,0x05,0x02,0xB3,0x04,
	0x80,0x00,0xB3,0xAC,0xB3,0xF2,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xB5,0x00,0x04,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0xC1,
	0x00,0x2A,0x1A,0x18,0x16,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x28,0x60,0xFF,0xFC,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xB6,0x00,0x05,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0xC1,
	0x00,0x2B,0x1B,0x19,0x17,0x15,0x13,0x11,0x0F,0x0D,0x0B,0x29,0x60,0xFF,0xFC,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xB7,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,
	0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A);
	dsbj_dcs_write_seq_static(ctx,0xBC,0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0xF0,0x0B,0x13,
	0x5C,0x5B,0x33,0x33,0x00,0x64,0x3C,0x5E,0x36);
	dsbj_dcs_write_seq_static(ctx,0xBF,0x0C,0x19,0x0C,0x19,0x00,0x11,0x04,0x18,0x50);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x40,0x93,0xFF,0xFF,0xFF,0x3F,0xFF,0x00,0xFF,0x00,
	0xCC,0xB1,0x23,0x45,0x67,0x89,0xAD,0xFF,0xFF,0xF0);
	dsbj_dcs_write_seq_static(ctx,0xC7,0x76,0x54,0x32,0x22,0x23,0x45,0x67,0x76,0x30,0x76,
	0x54,0x32,0x22,0x23,0x45,0x67,0x76,0x30,0x31,0x00,0x01,0xFF,0xFF,0x40,0x6E,0x6E,0x40,
	0x00,0x00,0x00,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0x80,0xFF,0xF7,0xEC,0xE3,0xDD,0xD7,0xD3,0xCF,0xCB,0xC0,
	0xB7,0xAF,0xA8,0xA2,0x9C,0x92,0x8A,0x82,0x7A,0x79,0x70,0x67,0x5D,0x51,0x43,0x30,0x26,
	0x1B,0x18,0x14,0x11,0x0D);
	dsbj_dcs_write_seq_static(ctx,0x81,0xFF,0xF7,0xEC,0xE3,0xDD,0xD7,0xD3,0xCF,0xCB,0xC0,
	0xB7,0xAF,0xA8,0xA2,0x9C,0x92,0x8A,0x82,0x7A,0x79,0x70,0x67,0x5D,0x51,0x43,0x30,0x26,
	0x1B,0x18,0x14,0x11,0x0D);
	dsbj_dcs_write_seq_static(ctx,0x82,0xFF,0xF7,0xEC,0xE3,0xDD,0xD7,0xD3,0xCF,0xCB,0xC0,
	0xB7,0xAF,0xA8,0xA2,0x9C,0x92,0x8A,0x82,0x7A,0x79,0x70,0x67,0x5D,0x51,0x43,0x30,0x26,
	0x1B,0x18,0x14,0x11,0x0D);
	dsbj_dcs_write_seq_static(ctx,0x83,0x01,0x0A,0x05,0x01,0x00,0x0A,0x05,0x01,0x00,0x0A,
	0x05,0x01,0x00,0x0A,0x06,0x02,0x00,0x0A,0x06,0x02,0x00,0x0A,0x06,0x02,0x00);
	dsbj_dcs_write_seq_static(ctx,0x84,0x13,0x70,0xC7,0x9A,0x16,0x97,0xEE,0x72,0x30,0x13,
	0x70,0xC7,0x9A,0x16,0x97,0xEE,0x72,0x30,0x13,0x70,0xC7,0x9A,0x16,0x97,0xEE,0x72,0x30);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x00);
	dsbj_dcs_write_seq_static(ctx,0xC8,0x46,0x00,0x88,0xF8);
	dsbj_dcs_write_seq_static(ctx,0xCA,0x25,0x40,0x00,0x19,0x46,0x94,0x41,0x8F,0x44,0x44,
	0x68,0x68,0x40,0x40,0x6E,0x6E,0x40,0x40,0x33,0x00,0x01,0x01,0x0F,0x0B,0x02,0x02,0x05,
	0x00,0x00,0x04);
	dsbj_dcs_write_seq_static(ctx,0xE0,0x0C,0x00,0xB0,0x10,0x00,0x0A,0x8C);
	dsbj_dcs_write_seq_static(ctx,0xBD,0xA1,0x0A,0x52,0xA6);
	dsbj_dcs_write_seq_static(ctx,0x35,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0x51,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0x53,0x2C,0x00);
	dsbj_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	/* Display On*/
	dsbj_dcs_write_seq_static(ctx, 0x29);
	msleep(20);
	dsbj_dcs_write_seq_static(ctx,0x6D,0x02,0x00);
	dsbj_dcs_write_seq_static(ctx,0xBD,0xA1,0x0A,0x52,0xAE);
	dsbj_dcs_write_seq_static(ctx,0xF0,0x00,0x00,0x00);
	msleep(1);

	// if(1 == rec_esd) {
		// pr_info("recover_esd_set_backlight\n");
		// esd_bl7[0] = (last_brightness & 0xF00) >> 8;
		// esd_bl7[1] = (last_brightness & 0xFF);
		// dsbj_dcs_write_seq(ctx, 0x51, esd_bl7[0], esd_bl7[1]);
	// }

	pr_info("%s-\n", __func__);
}

static int dsbj_disable(struct drm_panel *panel)
{
	struct dsbj *ctx = panel_to_dsbj(panel);

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
static int dsbj_unprepare(struct drm_panel *panel)
{

	struct dsbj *ctx = panel_to_dsbj(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;
	//panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[FTS] tpd_focaltech_notifier_callback in dsbj-unprepare\n ");


	if (n28_gesture_status== 1) {
		printk("[CTS-lcm]dsbj_unprepare n28_gesture_status= 1\n ");
		dsbj_dcs_write_seq_static(ctx,0X6d,0x25,0x00);
		dsbj_dcs_write_seq_static(ctx,0X28,0x00,0x00);
		msleep(20);
		dsbj_dcs_write_seq_static(ctx,0X10,0x00,0x00);
		msleep(120);
	} else {
		printk("[CTS-lcm]dsbj_unprepare n28_gesture_status= 0\n ");

		dsbj_dcs_write_seq_static(ctx,0X6d,0x25,0x00);
		dsbj_dcs_write_seq_static(ctx,0X28,0x00,0x00);
		msleep(20);
		dsbj_dcs_write_seq_static(ctx,0X10,0x00,0x00);
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

static int dsbj_prepare(struct drm_panel *panel)
{
	struct dsbj *ctx = panel_to_dsbj(panel);
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
	dsbj_panel_init(ctx);

	//add for TP resume
	//panel_notifier_call_chain(PANEL_PREPARE, NULL);
	//printk("[FTS] tpd_focaltech_notifier_callback in dsbj_prepare\n ");

	ret = ctx->error;
	if (ret < 0)
		dsbj_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	dsbj_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int dsbj_enable(struct drm_panel *panel)
{
	struct dsbj *ctx = panel_to_dsbj(panel);

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

static int dsbj_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	unsigned int bl_lvl;
	char bl_tb0[] = {0x51, 0x0f, 0xff};

	bl_lvl = wingtech_bright_to_bl(level,255,10,4095,48);
	if (bl_lvl > 4095)
		bl_lvl = 4095;
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
	struct dsbj *ctx = panel_to_dsbj(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = dsbj_setbacklight_cmdq,
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

static int dsbj_get_modes(struct drm_panel *panel,
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

static const struct drm_panel_funcs dsbj_drm_funcs = {
	.disable = dsbj_disable,
	.unprepare = dsbj_unprepare,
	.prepare = dsbj_prepare,
	.enable = dsbj_enable,
	.get_modes = dsbj_get_modes,
};

static int dsbj_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct dsbj *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct dsbj), GFP_KERNEL);
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
	drm_panel_init(&ctx->panel, dev, &dsbj_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &dsbj_drm_funcs;

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

	pr_info("%s- wt,dsbj,icnl9916c,hdp,vdo,60hz\n", __func__);

	return ret;
}

static void dsbj_remove(struct mipi_dsi_device *dsi)
{
	struct dsbj *ctx = mipi_dsi_get_drvdata(dsi);
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

static void dsbj_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("%s++\n", __func__);

	//fts_gestrue_status = 0;
	//pr_info("optimize shutdown sequence fts_gestrue_status is 0 %s++\n", __func__);
}

static const struct of_device_id dsbj_of_match[] = {
	{
	    .compatible = "wt,n28_icnl9916c_dsi_vdo_hdp_dsbj_mdt",
	},
	{}
};

MODULE_DEVICE_TABLE(of, dsbj_of_match);

static struct mipi_dsi_driver dsbj_driver = {
	.probe = dsbj_probe,
	.remove = dsbj_remove,
	.shutdown = dsbj_panel_shutdown,
	.driver = {
		.name = "n28_icnl9916c_dsi_vdo_hdp_dsbj_mdt",
		.owner = THIS_MODULE,
		.of_match_table = dsbj_of_match,
	},
};

module_mipi_dsi_driver(dsbj_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt dsbj icnl9916c vdo Panel Driver");
MODULE_LICENSE("GPL v2");

