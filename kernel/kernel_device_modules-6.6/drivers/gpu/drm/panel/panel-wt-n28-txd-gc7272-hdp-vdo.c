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

struct txd {
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

#define txd_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		txd_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define txd_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		txd_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct txd *panel_to_txd(struct drm_panel *panel)
{
	return container_of(panel, struct txd, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int txd_dcs_read(struct txd *ctx, u8 cmd, void *data, size_t len)
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

static void txd_panel_get_data(struct txd *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = txd_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void txd_dcs_write(struct txd *ctx, const void *data, size_t len)
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
u8 esd_bl4[] = {0x00, 0x00};

static void txd_panel_init(struct txd *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(25 * 1000, 25 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%sgc7272 1074-1066-vfp\n", __func__);

	txd_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
	txd_dcs_write_seq_static(ctx,0xFF,0x10);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x21);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x22);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x23);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x24);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x27);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x26);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x28);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0xA3);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0xB3);
	txd_dcs_write_seq_static(ctx,0xFB,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0x2D,0xAA);
	txd_dcs_write_seq_static(ctx,0xA3,0x12);
	txd_dcs_write_seq_static(ctx,0xA7,0x12);
	txd_dcs_write_seq_static(ctx,0xFF,0xB3);
	txd_dcs_write_seq_static(ctx,0x47,0x01);
	txd_dcs_write_seq_static(ctx,0x4E,0x46);
	txd_dcs_write_seq_static(ctx,0x3F,0x37);
	txd_dcs_write_seq_static(ctx,0x50,0x10);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0x29,0xE0);
	txd_dcs_write_seq_static(ctx,0x20,0x84);
	txd_dcs_write_seq_static(ctx,0x21,0x00);
	txd_dcs_write_seq_static(ctx,0x22,0x80);
	txd_dcs_write_seq_static(ctx,0x2E,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0xA3);
	txd_dcs_write_seq_static(ctx,0x58,0xAA);
	txd_dcs_write_seq_static(ctx,0xFF,0x26);
	txd_dcs_write_seq_static(ctx,0x43,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x22);
	txd_dcs_write_seq_static(ctx,0x0E,0x33);
	txd_dcs_write_seq_static(ctx,0x0C,0x01);
	txd_dcs_write_seq_static(ctx,0x0B,0x2A);
	txd_dcs_write_seq_static(ctx,0x1F,0x06); //close vedio drop
	txd_dcs_write_seq_static(ctx,0xFF,0x28);
	txd_dcs_write_seq_static(ctx,0x7D,0x1F);
	txd_dcs_write_seq_static(ctx,0x7b,0x40);
	txd_dcs_write_seq_static(ctx,0xFF,0x22);
	txd_dcs_write_seq_static(ctx,0xE4,0x00);
	txd_dcs_write_seq_static(ctx,0x01,0x06);
	txd_dcs_write_seq_static(ctx,0x02,0x40);
	txd_dcs_write_seq_static(ctx,0x03,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0xC3,0x00);
	txd_dcs_write_seq_static(ctx,0xC4,0xA3);
	txd_dcs_write_seq_static(ctx,0xC5,0x00);
	txd_dcs_write_seq_static(ctx,0xC6,0xA3);
	txd_dcs_write_seq_static(ctx,0xB3,0x00);
	txd_dcs_write_seq_static(ctx,0xB4,0x20);
	txd_dcs_write_seq_static(ctx,0xB5,0x00);
	txd_dcs_write_seq_static(ctx,0xB6,0xA0);
	txd_dcs_write_seq_static(ctx,0xD3,0x24);
	txd_dcs_write_seq_static(ctx,0xFF,0x22);
	txd_dcs_write_seq_static(ctx,0x25,0x08);
	txd_dcs_write_seq_static(ctx,0x26,0x00);
	txd_dcs_write_seq_static(ctx,0x2E,0x6F);
	txd_dcs_write_seq_static(ctx,0x2F,0x00);
	txd_dcs_write_seq_static(ctx,0x36,0x09);
	txd_dcs_write_seq_static(ctx,0x37,0x00);
	txd_dcs_write_seq_static(ctx,0x3F,0x6F);
	txd_dcs_write_seq_static(ctx,0x40,0x00);
	txd_dcs_write_seq_static(ctx,0xFF,0x28);
	txd_dcs_write_seq_static(ctx,0x01,0x1B);
	txd_dcs_write_seq_static(ctx,0x02,0x25);
	txd_dcs_write_seq_static(ctx,0x03,0x1B);
	txd_dcs_write_seq_static(ctx,0x04,0x25);
	txd_dcs_write_seq_static(ctx,0x05,0x05);
	txd_dcs_write_seq_static(ctx,0x06,0x01);
	txd_dcs_write_seq_static(ctx,0x07,0x1A);
	txd_dcs_write_seq_static(ctx,0x08,0x25);
	txd_dcs_write_seq_static(ctx,0x09,0x25);
	txd_dcs_write_seq_static(ctx,0x0A,0x25);
	txd_dcs_write_seq_static(ctx,0x0B,0x0F);
	txd_dcs_write_seq_static(ctx,0x0C,0x0D);
	txd_dcs_write_seq_static(ctx,0x0D,0x0B);
	txd_dcs_write_seq_static(ctx,0x0E,0x09);
	txd_dcs_write_seq_static(ctx,0x0F,0x25);
	txd_dcs_write_seq_static(ctx,0x10,0x25);
	txd_dcs_write_seq_static(ctx,0x11,0x25);
	txd_dcs_write_seq_static(ctx,0x12,0x25);
	txd_dcs_write_seq_static(ctx,0x13,0x1B);
	txd_dcs_write_seq_static(ctx,0x14,0x1B);
	txd_dcs_write_seq_static(ctx,0x15,0x1B);
	txd_dcs_write_seq_static(ctx,0x16,0x25);
	txd_dcs_write_seq_static(ctx,0x17,0x1B);
	txd_dcs_write_seq_static(ctx,0x18,0x25);
	txd_dcs_write_seq_static(ctx,0x19,0x1B);
	txd_dcs_write_seq_static(ctx,0x1A,0x25);
	txd_dcs_write_seq_static(ctx,0x1B,0x04);
	txd_dcs_write_seq_static(ctx,0x1C,0x00);
	txd_dcs_write_seq_static(ctx,0x1D,0x1A);
	txd_dcs_write_seq_static(ctx,0x1E,0x25);
	txd_dcs_write_seq_static(ctx,0x1F,0x25);
	txd_dcs_write_seq_static(ctx,0x20,0x25);
	txd_dcs_write_seq_static(ctx,0x21,0x0E);
	txd_dcs_write_seq_static(ctx,0x22,0x0C);
	txd_dcs_write_seq_static(ctx,0x23,0x0A);
	txd_dcs_write_seq_static(ctx,0x24,0x08);
	txd_dcs_write_seq_static(ctx,0x25,0x25);
	txd_dcs_write_seq_static(ctx,0x26,0x25);
	txd_dcs_write_seq_static(ctx,0x27,0x25);
	txd_dcs_write_seq_static(ctx,0x28,0x25);
	txd_dcs_write_seq_static(ctx,0x29,0x1B);
	txd_dcs_write_seq_static(ctx,0x2A,0x1B);
	txd_dcs_write_seq_static(ctx,0x2B,0x1B);
	txd_dcs_write_seq_static(ctx,0x2D,0x25);
	txd_dcs_write_seq_static(ctx,0x30,0x05);
	txd_dcs_write_seq_static(ctx,0x31,0x05);
	txd_dcs_write_seq_static(ctx,0x32,0x05);
	txd_dcs_write_seq_static(ctx,0x33,0x05);
	txd_dcs_write_seq_static(ctx,0x34,0x05);
	txd_dcs_write_seq_static(ctx,0x35,0x05);
	txd_dcs_write_seq_static(ctx,0x38,0x05);
	txd_dcs_write_seq_static(ctx,0x39,0x05);
	txd_dcs_write_seq_static(ctx,0x2F,0x28);
	txd_dcs_write_seq_static(ctx,0xFF,0x21);
	txd_dcs_write_seq_static(ctx,0x7E,0x03);
	txd_dcs_write_seq_static(ctx,0x7F,0x23);
	txd_dcs_write_seq_static(ctx,0x8B,0x23);
	txd_dcs_write_seq_static(ctx,0x80,0x03);
	txd_dcs_write_seq_static(ctx,0x8C,0x18);
	txd_dcs_write_seq_static(ctx,0xAF,0x40);
	txd_dcs_write_seq_static(ctx,0xB0,0x40);
	txd_dcs_write_seq_static(ctx,0x83,0x02);
	txd_dcs_write_seq_static(ctx,0x8F,0x02);
	txd_dcs_write_seq_static(ctx,0x84,0x82);
	txd_dcs_write_seq_static(ctx,0x90,0x82);
	txd_dcs_write_seq_static(ctx,0x85,0x82);
	txd_dcs_write_seq_static(ctx,0x91,0x82);
	txd_dcs_write_seq_static(ctx,0x87,0x24);
	txd_dcs_write_seq_static(ctx,0x93,0x20);
	txd_dcs_write_seq_static(ctx,0x82,0xB0);
	txd_dcs_write_seq_static(ctx,0x8E,0xB0);
	txd_dcs_write_seq_static(ctx,0x2B,0x00);
	txd_dcs_write_seq_static(ctx,0x2E,0x00);
	txd_dcs_write_seq_static(ctx,0x88,0xB7);
	txd_dcs_write_seq_static(ctx,0x89,0x20);
	txd_dcs_write_seq_static(ctx,0x8A,0x33);
	txd_dcs_write_seq_static(ctx,0x94,0xB7);
	txd_dcs_write_seq_static(ctx,0x95,0x20);
	txd_dcs_write_seq_static(ctx,0x96,0x33);
	txd_dcs_write_seq_static(ctx,0x45,0x33);
	txd_dcs_write_seq_static(ctx,0x46,0xB6);
	txd_dcs_write_seq_static(ctx,0x4C,0xB6);
	txd_dcs_write_seq_static(ctx,0x5E,0xA4);
	txd_dcs_write_seq_static(ctx,0x64,0xA4);
	txd_dcs_write_seq_static(ctx,0x47,0x07);
	txd_dcs_write_seq_static(ctx,0x4D,0x06);
	txd_dcs_write_seq_static(ctx,0x5F,0x20);
	txd_dcs_write_seq_static(ctx,0x65,0x21);
	txd_dcs_write_seq_static(ctx,0x76,0x40);
	txd_dcs_write_seq_static(ctx,0x77,0x40);
	txd_dcs_write_seq_static(ctx,0x7A,0x40);
	txd_dcs_write_seq_static(ctx,0x7B,0x40);
	txd_dcs_write_seq_static(ctx,0x49,0x82);
	txd_dcs_write_seq_static(ctx,0x4A,0x82);
	txd_dcs_write_seq_static(ctx,0x4F,0x82);
	txd_dcs_write_seq_static(ctx,0x50,0x82);
	txd_dcs_write_seq_static(ctx,0x61,0x82);
	txd_dcs_write_seq_static(ctx,0x62,0x82);
	txd_dcs_write_seq_static(ctx,0x67,0x82);
	txd_dcs_write_seq_static(ctx,0x68,0x82);
	txd_dcs_write_seq_static(ctx,0xC2,0x8A);
	txd_dcs_write_seq_static(ctx,0xC6,0x52);
	txd_dcs_write_seq_static(ctx,0x29,0x00);
	txd_dcs_write_seq_static(ctx,0xC3,0x8A);
	txd_dcs_write_seq_static(ctx,0xC7,0x42);
	txd_dcs_write_seq_static(ctx,0xFF,0x22);
	txd_dcs_write_seq_static(ctx,0x05,0x00);
	txd_dcs_write_seq_static(ctx,0x08,0x01);
	txd_dcs_write_seq_static(ctx,0x0F,0x01);
	txd_dcs_write_seq_static(ctx,0xFF,0x28);
	txd_dcs_write_seq_static(ctx,0x3D,0x4F);
	txd_dcs_write_seq_static(ctx,0x3E,0x4F);
	txd_dcs_write_seq_static(ctx,0x3F,0x58);
	txd_dcs_write_seq_static(ctx,0x40,0x58);
	txd_dcs_write_seq_static(ctx,0x45,0x4F);
	txd_dcs_write_seq_static(ctx,0x46,0x4F);
	txd_dcs_write_seq_static(ctx,0x47,0x58);
	txd_dcs_write_seq_static(ctx,0x48,0x58);
	txd_dcs_write_seq_static(ctx,0x4D,0xA2);
	txd_dcs_write_seq_static(ctx,0x50,0x2A);
	txd_dcs_write_seq_static(ctx,0x52,0x72);
	txd_dcs_write_seq_static(ctx,0x53,0x22);
	txd_dcs_write_seq_static(ctx,0x56,0x12);
	txd_dcs_write_seq_static(ctx,0x57,0x20);
	txd_dcs_write_seq_static(ctx,0x5A,0x88);
	txd_dcs_write_seq_static(ctx,0x5B,0x8C);
	//{0x62,0x75);
	//{0x63,0x75);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0x7E,0x03);
	txd_dcs_write_seq_static(ctx,0x7F,0x00);
	txd_dcs_write_seq_static(ctx,0x80,0x64);
	txd_dcs_write_seq_static(ctx,0x81,0x00);
	txd_dcs_write_seq_static(ctx,0x82,0x00);
	txd_dcs_write_seq_static(ctx,0x83,0x64);
	txd_dcs_write_seq_static(ctx,0x84,0x64);
	txd_dcs_write_seq_static(ctx,0x85,0x40);
	txd_dcs_write_seq_static(ctx,0x86,0x39);
	txd_dcs_write_seq_static(ctx,0x87,0x40);
	txd_dcs_write_seq_static(ctx,0x88,0x39);
	txd_dcs_write_seq_static(ctx,0x8A,0x0A);
	txd_dcs_write_seq_static(ctx,0x8B,0x0A);
	txd_dcs_write_seq_static(ctx,0xFF,0x25);
	txd_dcs_write_seq_static(ctx,0x75,0x00);
	txd_dcs_write_seq_static(ctx,0x76,0x00);
	txd_dcs_write_seq_static(ctx,0x77,0x64);
	txd_dcs_write_seq_static(ctx,0x78,0x00);
	txd_dcs_write_seq_static(ctx,0x79,0x64);
	txd_dcs_write_seq_static(ctx,0x7A,0x64);
	txd_dcs_write_seq_static(ctx,0x7B,0x64);
	txd_dcs_write_seq_static(ctx,0x7C,0x32);
	txd_dcs_write_seq_static(ctx,0x7D,0x76);
	txd_dcs_write_seq_static(ctx,0x7E,0x32);
	txd_dcs_write_seq_static(ctx,0x7F,0x76);
	txd_dcs_write_seq_static(ctx,0x80,0x00);
	txd_dcs_write_seq_static(ctx,0x81,0x03);
	txd_dcs_write_seq_static(ctx,0x82,0x03);
	txd_dcs_write_seq_static(ctx,0xFF,0x23);
	txd_dcs_write_seq_static(ctx,0xEF,0x07); //software version
	txd_dcs_write_seq_static(ctx,0x29,0x03);
	txd_dcs_write_seq_static(ctx,0x01,0x00,0x00,0x00,0x24,0x00,0x4D,0x00,0x62,0x00,0x78,0x00,0x8A,0x00,0x99,0x00,0xA6);
	txd_dcs_write_seq_static(ctx,0x02,0x00,0xB5,0x00,0xDE,0x00,0xFE,0x01,0x2E,0x01,0x57,0x01,0x96,0x01,0xCC,0x01,0xCE);
	txd_dcs_write_seq_static(ctx,0x03,0x02,0x06,0x02,0x4E,0x02,0x7C,0x02,0xB9,0x02,0xE2,0x03,0x1A,0x03,0x2A,0x03,0x3F);
	txd_dcs_write_seq_static(ctx,0x04,0x03,0x55,0x03,0x70,0x03,0x8E,0x03,0xB1,0x03,0xDF,0x03,0xFF);
	txd_dcs_write_seq_static(ctx,0x0D,0x00,0x00,0x00,0x24,0x00,0x4D,0x00,0x62,0x00,0x78,0x00,0x8A,0x00,0x99,0x00,0xA6);
	txd_dcs_write_seq_static(ctx,0x0E,0x00,0xB5,0x00,0xDE,0x00,0xFE,0x01,0x2E,0x01,0x57,0x01,0x96,0x01,0xCC,0x01,0xCE);
	txd_dcs_write_seq_static(ctx,0x0F,0x02,0x06,0x02,0x4E,0x02,0x7C,0x02,0xB9,0x02,0xE2,0x03,0x1A,0x03,0x2A,0x03,0x3F);
	txd_dcs_write_seq_static(ctx,0x10,0x03,0x55,0x03,0x70,0x03,0x8E,0x03,0xB1,0x03,0xDF,0x03,0xFF);
	txd_dcs_write_seq_static(ctx,0x2B,0x41);
	txd_dcs_write_seq_static(ctx,0xF0,0x01);
	txd_dcs_write_seq_static(ctx,0x2D,0x65);
	txd_dcs_write_seq_static(ctx,0x2E,0x00);
	txd_dcs_write_seq_static(ctx,0x32,0x02);
	txd_dcs_write_seq_static(ctx,0x33,0x1A);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0xF1,0xE0);
	txd_dcs_write_seq_static(ctx,0xF2,0x03);
	txd_dcs_write_seq_static(ctx,0xFF,0x20);
	txd_dcs_write_seq_static(ctx,0xF5,0xE0);
	txd_dcs_write_seq_static(ctx,0xF6,0x0B);
	txd_dcs_write_seq_static(ctx,0xFF,0x10);
	txd_dcs_write_seq_static(ctx,0x53,0x2c);
	txd_dcs_write_seq_static(ctx,0x55,0x00);
	txd_dcs_write_seq_static(ctx,0x36,0x08);
	txd_dcs_write_seq_static(ctx,0x69,0x00);
	txd_dcs_write_seq_static(ctx,0x35,0x00);
	txd_dcs_write_seq_static(ctx,0xBA,0x03);
	txd_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	/* Display On*/
	txd_dcs_write_seq_static(ctx, 0x29);
    msleep(20);

	// if(1 == rec_esd) {
		// pr_info("recover_esd_set_backlight\n");
		// esd_bl4[0] = (last_brightness & 0xF00) >> 8;
		// esd_bl4[1] = (last_brightness & 0xFF);
		// txd_dcs_write_seq(ctx, 0x51, esd_bl4[0], esd_bl4[1]);
	// }

	pr_info("%s-\n", __func__);
}

static int txd_disable(struct drm_panel *panel)
{
	struct txd *ctx = panel_to_txd(panel);

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
static int txd_unprepare(struct drm_panel *panel)
{

	struct txd *ctx = panel_to_txd(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;
	//panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[FTS] tpd_focaltech_notifier_callback in txd-unprepare\n ");

	if (n28_gesture_status == 1) {
		txd_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
		txd_dcs_write_seq_static(ctx,0xFF,0x10);
		txd_dcs_write_seq_static(ctx,0x28,0x00);
		msleep(50);
		txd_dcs_write_seq_static(ctx,0x10,0x00);
		msleep(120);
		printk("[GTP-lcm]txd_unprepare n28_gesture_status = 1\n ");
	} else {
		printk("[GTP-lcm]txd_unprepare n28_gesture_status = 0\n ");
		txd_dcs_write_seq_static(ctx,0xff,0x55,0xaa,0x66);
		txd_dcs_write_seq_static(ctx,0xff,0x22);
		txd_dcs_write_seq_static(ctx,0xe4,0x00);
		txd_dcs_write_seq_static(ctx,0xff,0x10);
		txd_dcs_write_seq_static(ctx,0x28,0x00);
		msleep(50);
		txd_dcs_write_seq_static(ctx,0x10,0x00);
		msleep(100);
		txd_dcs_write_seq_static(ctx,0x4f,0x01);
		msleep(10);

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

static int txd_prepare(struct drm_panel *panel)
{
	struct txd *ctx = panel_to_txd(panel);
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
	txd_panel_init(ctx);

	//add for TP resume
	//panel_notifier_call_chain(PANEL_PREPARE, NULL);
	//printk("[FTS] tpd_focaltech_notifier_callback in txd_prepare\n ");

	ret = ctx->error;
	if (ret < 0)
		txd_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	txd_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int txd_enable(struct drm_panel *panel)
{
	struct txd *ctx = panel_to_txd(panel);

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
	.clock = 104311,
	.hdisplay = 720,
	.hsync_start = 720 + 122,
	.hsync_end = 720 + 122 + 8,
	.htotal = 720 + 122 + 8 + 130,
	.vdisplay = 1600,
	.vsync_start = 1600 + 140,
	.vsync_end = 1600 + 140 + 6,
	.vtotal = 1600 + 140 + 6 + 28,
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

static int txd_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	unsigned int bl_lvl;
	char bl_tb0[] = {0x51, 0x0f, 0xe9};

	bl_lvl = wingtech_bright_to_bl(level,1023,52,4095,48);
	if(bl_lvl > 4073)
		bl_lvl = 4073;
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
	struct txd *ctx = panel_to_txd(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = txd_setbacklight_cmdq,
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

static int txd_get_modes(struct drm_panel *panel,
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

static const struct drm_panel_funcs txd_drm_funcs = {
	.disable = txd_disable,
	.unprepare = txd_unprepare,
	.prepare = txd_prepare,
	.enable = txd_enable,
	.get_modes = txd_get_modes,
};

static int txd_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct txd *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct txd), GFP_KERNEL);
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
	drm_panel_init(&ctx->panel, dev, &txd_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &txd_drm_funcs;

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

	pr_info("%s- wt,txd,gc7272,hdp,vdo,60hz\n", __func__);

	return ret;
}

static void txd_remove(struct mipi_dsi_device *dsi)
{
	struct txd *ctx = mipi_dsi_get_drvdata(dsi);
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

static void txd_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("%s++\n", __func__);

	//fts_gestrue_status = 0;
	//pr_info("optimize shutdown sequence fts_gestrue_status is 0 %s++\n", __func__);
}

static const struct of_device_id txd_of_match[] = {
	{
	    .compatible = "wt,n28_gc7272_dsi_vdo_hdp_txd_sharp",
	},
	{}
};

MODULE_DEVICE_TABLE(of, txd_of_match);

static struct mipi_dsi_driver txd_driver = {
	.probe = txd_probe,
	.remove = txd_remove,
	.shutdown = txd_panel_shutdown,
	.driver = {
		.name = "n28_gc7272_dsi_vdo_hdp_txd_sharp",
		.owner = THIS_MODULE,
		.of_match_table = txd_of_match,
	},
};

static int __init txd_drv_init(void)
{
	int ret = 0;

	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	ret = mipi_dsi_driver_register(&txd_driver);
	if (ret < 0)
		pr_notice("%s, Failed to register boe driver: %d\n", __func__, ret);

	mtk_panel_unlock();
	pr_notice("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit txd_drv_exit(void)
{
	pr_notice("%s+\n", __func__);
	mtk_panel_lock();
	mipi_dsi_driver_unregister(&txd_driver);
	mtk_panel_unlock();
	pr_notice("%s-\n", __func__);
}
module_init(txd_drv_init);
module_exit(txd_drv_exit);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt txd gc7272 vdo Panel Driver");
MODULE_LICENSE("GPL v2");

