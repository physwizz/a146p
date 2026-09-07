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

static int dimming_state = 0;
static int last_brightness = 0;	//the last backlight level values
u8 esd_bl2[] = {0x00, 0x00};

static void dsbj_panel_init(struct dsbj *ctx)
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
	pr_info("%sft8057s 1074-1066-vfp\n", __func__);
	dimming_state = 0;

	dsbj_dcs_write_seq_static(ctx,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xFF,0x80, 0x57, 0x01);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xFF,0x80, 0x57);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA3);
	dsbj_dcs_write_seq_static(ctx,0xB3,0x06, 0x40, 0x00, 0x18);
	dsbj_dcs_write_seq_static(ctx,0x00,0x93);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x6B);
	dsbj_dcs_write_seq_static(ctx,0x00,0x97);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x6B);
	dsbj_dcs_write_seq_static(ctx,0x00,0x9A);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x41);
	dsbj_dcs_write_seq_static(ctx,0x00,0x9C);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x41);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB6);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x57, 0x57);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB8);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x37, 0x37);
	dsbj_dcs_write_seq_static(ctx,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xD8,0x2B, 0x2B);
	dsbj_dcs_write_seq_static(ctx,0x00,0x82);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x55);
	dsbj_dcs_write_seq_static(ctx,0x00,0x83);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x07);
	dsbj_dcs_write_seq_static(ctx,0x00,0x96);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x0D);
	dsbj_dcs_write_seq_static(ctx,0x00,0x86);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x0D);
	dsbj_dcs_write_seq_static(ctx,0x00,0x94);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x15);
	dsbj_dcs_write_seq_static(ctx,0x00,0x9B);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x51);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA3);
	dsbj_dcs_write_seq_static(ctx,0xA5,0x04);
	dsbj_dcs_write_seq_static(ctx,0x00,0x99);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x56);
	dsbj_dcs_write_seq_static(ctx,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xE1,0x2D, 0x31, 0x3C, 0x48, 0x50, 0x58, 0x65, 0x71, 0x6F, 0x7A, 0x7A, 0x8B, 0x7B, 0x6A, 0x6D, 0x61);
	dsbj_dcs_write_seq_static(ctx,0x00,0x10);
	dsbj_dcs_write_seq_static(ctx,0xE1,0x5A, 0x4F, 0x3F, 0x35, 0x2D, 0x1C, 0x10, 0x0F);
	dsbj_dcs_write_seq_static(ctx,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xE2,0x2D, 0x31, 0x3C, 0x48, 0x50, 0x58, 0x65, 0x71, 0x6F, 0x7A, 0x7A, 0x8B, 0x7B, 0x6A, 0x6D, 0x61);
	dsbj_dcs_write_seq_static(ctx,0x00,0x10);
	dsbj_dcs_write_seq_static(ctx,0xE2,0x5A, 0x4F, 0x3F, 0x35, 0x2D, 0x1C, 0x10, 0x0F);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x00, 0xD2, 0x00, 0x3A, 0x00, 0x10);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x00, 0xEC, 0x00, 0x3A, 0x00, 0x10);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA0);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x00, 0xD2, 0x00, 0x3A, 0x00, 0x10);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB0);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x01, 0x11, 0x00, 0x3A, 0x10);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC1);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x01, 0x33, 0x01, 0x0A, 0x00, 0xCD, 0x01, 0x90);
	dsbj_dcs_write_seq_static(ctx,0x00,0x70);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x00, 0xEC, 0x00, 0x3A, 0x00, 0x10);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA3);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x00, 0x33, 0x00, 0x3C, 0x00, 0x02);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB7);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x00, 0x33);
	dsbj_dcs_write_seq_static(ctx,0x00,0x73);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x09, 0x09);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x01, 0x81, 0x09, 0x09, 0x00, 0x78, 0x00, 0x96, 0x00, 0x78, 0x00, 0x96, 0x00, 0x78, 0x00, 0x96);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x00, 0xA5, 0x16, 0x8F, 0x00, 0xA5, 0x80, 0x09, 0x09, 0x00, 0x07, 0xD0, 0x16, 0x16, 0x27);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA0);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x20, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB0);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x87, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD1);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE1);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x08, 0x03, 0xC3, 0x03, 0xC3, 0x02, 0xB0, 0x00, 0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xF1);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x14, 0x14, 0x1E, 0x01, 0x45, 0x01, 0x45, 0x01, 0x2B);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB0);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x00, 0x00, 0x6D, 0x71);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB5);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x03, 0x03, 0x5B, 0x5F);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC0);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x06, 0x06, 0x3B, 0x3F);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC5);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x06, 0x06, 0x3F, 0x43);
	dsbj_dcs_write_seq_static(ctx,0x00,0x60);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x00, 0x00, 0x6D, 0x71, 0x03, 0x03, 0x5B, 0x5F);
	dsbj_dcs_write_seq_static(ctx,0x00,0x70);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x00, 0x00, 0x6F, 0x73, 0x03, 0x03, 0x5D, 0x61);
	dsbj_dcs_write_seq_static(ctx,0x00,0xAA);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x80, 0x80, 0x10, 0x0C);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD1);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x03, 0xAA, 0x05, 0x22, 0x09, 0x59, 0x05, 0x87, 0x08, 0x23, 0x0F, 0xAC);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE1);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x05, 0x22);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE2);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x06, 0xDE, 0x06, 0xDD, 0x06, 0xDD, 0x06, 0xDD, 0x06, 0xDD, 0x06, 0xDD);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x01);
	dsbj_dcs_write_seq_static(ctx,0x00,0xF5);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x01);
	dsbj_dcs_write_seq_static(ctx,0x00,0xF6);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x3C);
	dsbj_dcs_write_seq_static(ctx,0x00,0xF1);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x3C);
	dsbj_dcs_write_seq_static(ctx,0x00,0xF7);
	dsbj_dcs_write_seq_static(ctx,0xCF,0x11);
	dsbj_dcs_write_seq_static(ctx,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0x1F,0x3C, 0x3C);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD1);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x00, 0x13, 0x01, 0x01, 0x00, 0xA3, 0x01);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE8);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x00, 0xA3, 0x00, 0xA3);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xCC,0x00, 0x16, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1E, 0x26, 0x1A, 0x14, 0x12, 0x10, 0x0E);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xCC,0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xCD,0x00, 0x17, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1E, 0x26, 0x1B, 0x15, 0x13, 0x11, 0x0F);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xCD,0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xCB,0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1);
	dsbj_dcs_write_seq_static(ctx,0x00,0xED);
	dsbj_dcs_write_seq_static(ctx,0xCB,0xC1);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xEE);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x90);
	dsbj_dcs_write_seq_static(ctx,0xC3,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA0);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB0);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x00, 0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC0);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x00, 0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD2);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE0);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83);
	dsbj_dcs_write_seq_static(ctx,0x00,0xFA);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x83, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xEF);
	dsbj_dcs_write_seq_static(ctx,0xCB,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x68);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x8A, 0x09, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x6C);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x89, 0x09, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x70);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x86, 0x09, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x74);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x85, 0x09, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x78);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x06, 0x04, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x7C);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x07, 0x04, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x08, 0x04, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x84);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x09, 0x04, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x88);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0D, 0x09, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE4);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0E, 0x09, 0x68, 0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x8C);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x81, 0x02, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x91);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x80, 0x03, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x96);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x01, 0x04, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x9B);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x02, 0x05, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA0);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x03, 0x06, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA5);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x04, 0x07, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xAA);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x05, 0x08, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xAF);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x06, 0x09, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB4);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x07, 0x0A, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB9);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x08, 0x0B, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xBE);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x09, 0x0C, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC3);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0A, 0x0D, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC8);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0B, 0x0E, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xCD);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0C, 0x0F, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD2);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0D, 0x10, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD7);
	dsbj_dcs_write_seq_static(ctx,0xC2,0x0E, 0x11, 0x02, 0x68, 0xC6);
	dsbj_dcs_write_seq_static(ctx,0x00,0xDC);
	dsbj_dcs_write_seq_static(ctx,0xC2,0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
	dsbj_dcs_write_seq_static(ctx,0x00,0x98);
	dsbj_dcs_write_seq_static(ctx,0xC4,0x08);
	dsbj_dcs_write_seq_static(ctx,0x00,0x91);
	dsbj_dcs_write_seq_static(ctx,0xE9,0xFF, 0xFF, 0xFF, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x85);
	dsbj_dcs_write_seq_static(ctx,0xC4,0x80);
	dsbj_dcs_write_seq_static(ctx,0x00,0x81);
	dsbj_dcs_write_seq_static(ctx,0xA4,0x73);
	dsbj_dcs_write_seq_static(ctx,0x00,0x86);
	dsbj_dcs_write_seq_static(ctx,0xA4,0xB6);
	dsbj_dcs_write_seq_static(ctx,0x00,0x95);
	dsbj_dcs_write_seq_static(ctx,0xC4,0x80);
	dsbj_dcs_write_seq_static(ctx,0x00,0xCA);
	dsbj_dcs_write_seq_static(ctx,0xC0,0x90, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB7);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x1D);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB1);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x1B);
	dsbj_dcs_write_seq_static(ctx,0x00,0x83);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x11);
	dsbj_dcs_write_seq_static(ctx,0x00,0x94);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x11);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB0);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB3);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB2);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x0D);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB5);
	dsbj_dcs_write_seq_static(ctx,0xC5,0x02);
	dsbj_dcs_write_seq_static(ctx,0x00,0xC2);
	dsbj_dcs_write_seq_static(ctx,0xF5,0x42);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xD0);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x01);
	dsbj_dcs_write_seq_static(ctx,0x00,0xE0);
	dsbj_dcs_write_seq_static(ctx,0xCE,0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA1);
	dsbj_dcs_write_seq_static(ctx,0xC1,0xCC);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA6);
	dsbj_dcs_write_seq_static(ctx,0xC1,0x10);
	dsbj_dcs_write_seq_static(ctx,0x00,0x71);
	dsbj_dcs_write_seq_static(ctx,0xC0,0xEC, 0x01, 0x2B, 0x00, 0x22);
	dsbj_dcs_write_seq_static(ctx,0x00,0x86);
	dsbj_dcs_write_seq_static(ctx,0xB7,0x80);
	dsbj_dcs_write_seq_static(ctx,0x00,0xA5);
	dsbj_dcs_write_seq_static(ctx,0xB0,0x1D);
	dsbj_dcs_write_seq_static(ctx,0x00,0xB0);
	dsbj_dcs_write_seq_static(ctx,0xCA,0x00,0x00,0x0C,0x00,0x00,0x06);
	dsbj_dcs_write_seq_static(ctx,0x00,0x00);
	dsbj_dcs_write_seq_static(ctx,0xFF,0x00, 0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x00,0x80);
	dsbj_dcs_write_seq_static(ctx,0xFF,0x00, 0x00);
	dsbj_dcs_write_seq_static(ctx,0x53,0x24);
	dsbj_dcs_write_seq_static(ctx,0x55,0x00);
	dsbj_dcs_write_seq_static(ctx, 0x11);
	msleep(100);
	/* Display On*/
	dsbj_dcs_write_seq_static(ctx, 0x29);
	dsbj_dcs_write_seq_static(ctx, 0x35, 0x00);
    msleep(10);

	// if(1 == rec_esd) {
		// pr_info("recover_esd_set_backlight\n");
		// esd_bl2[0] = (last_brightness & 0xFF0) >> 4;
		// esd_bl2[1] = (last_brightness & 0xF);
		// dsbj_dcs_write_seq(ctx, 0x51, esd_bl2[0], esd_bl2[1]);
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

	if (n28_gesture_status == 1) {
		printk("[FTS-lcm]dsbj_unprepare n28_gesture_status = 1\n ");
		dsbj_dcs_write_seq_static(ctx,0X28,0x00);
		msleep(20);
		dsbj_dcs_write_seq_static(ctx,0X10,0x00);
		msleep(140);
		dsbj_dcs_write_seq_static(ctx,0X00,0x00);
		dsbj_dcs_write_seq_static(ctx,0Xf7,0x5A,0xA5,0x95,0x27);
		msleep(3);
	} else {
		printk("[FTS-lcm]dsbj_unprepare n28_gesture_status = 0\n ");

		dsbj_dcs_write_seq_static(ctx,0X28,0x00);
		msleep(20);
		dsbj_dcs_write_seq_static(ctx,0X10,0x00);
		msleep(140);
		dsbj_dcs_write_seq_static(ctx,0X00,0x00);
		dsbj_dcs_write_seq_static(ctx,0Xf7,0x5A,0xA5,0x95,0x27);
		msleep(3);

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

	msleep(2);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0x0f);
	_lcm_i2c_write_bytes(AVEE_REG, 0x0f);
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
	.clock = 103740,
	.hdisplay = 720,
	.hsync_start = 720 + 156,
	.hsync_end = 720 + 156 + 4,
	.htotal = 720 + 156 + 4 + 14,
	.vdisplay = 1600,
	.vsync_start = 1600 + 300,
	.vsync_end = 1600 + 300 + 6,
	.vtotal = 1600 + 300 + 6 + 28,
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
	char bl_tb0[] = {0x51, 0x0f, 0x0f};
	char dimming[] = {0x53, 0x2C};

	if((dimming_state == 0) && (last_brightness !=0)) {
		cb(dsi, handle, dimming, ARRAY_SIZE(dimming));
		msleep(20);
		dimming_state = 1;
	}

	bl_lvl = wingtech_bright_to_bl(level,1023,52,4095,48);
	pr_info("%s backlight: level = %d,bl_lvl=%d\n", __func__, level, bl_lvl);

	bl_tb0[1] = (bl_lvl & 0xFF0) >> 4;
	bl_tb0[2] = (bl_lvl & 0xF);
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

	pr_info("%s- wt,dsbj,ft8057s,hdp,vdo,60hz\n", __func__);

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

	//n28_gesture_status = 0;
	//pr_info("optimize shutdown sequence n28_gesture_status is 0 %s++\n", __func__);
}

static const struct of_device_id dsbj_of_match[] = {
	{
	    .compatible = "wt,n28_ft8057s_dsi_vdo_hdp_dsbj_mantix",
	},
	{}
};

MODULE_DEVICE_TABLE(of, dsbj_of_match);

static struct mipi_dsi_driver dsbj_driver = {
	.probe = dsbj_probe,
	.remove = dsbj_remove,
	.shutdown = dsbj_panel_shutdown,
	.driver = {
		.name = "n28_ft8057s_dsi_vdo_hdp_dsbj_mantix",
		.owner = THIS_MODULE,
		.of_match_table = dsbj_of_match,
	},
};

static int __init dsbj_drv_init(void)
{
	int ret = 0;

	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	ret = mipi_dsi_driver_register(&dsbj_driver);
	if (ret < 0)
		pr_notice("%s, Failed to register boe driver: %d\n", __func__, ret);

	mtk_panel_unlock();
	pr_notice("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit dsbj_drv_exit(void)
{
	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	mipi_dsi_driver_unregister(&dsbj_driver);
	mtk_panel_unlock();
	pr_notice("%s-\n", __func__);
}
module_init(dsbj_drv_init);
module_exit(dsbj_drv_exit);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt dsbj ft8057s vdo Panel Driver");
MODULE_LICENSE("GPL v2");

