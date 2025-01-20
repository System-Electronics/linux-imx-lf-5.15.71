// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright © 2025 TODO
 *
 * Based on panel-waveshare-dsi.c by Raspberry Pi Ltd
 *
 * Copyright © 2023 Raspberry Pi Ltd
 *
 * Based on panel-raspberrypi-touchscreen by Broadcom
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/pm.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#define WS_DSI_DRIVER_NAME "ws-ts-imx"
#define WS_DSI_I2C_ADDR 0x45

struct ws_panel_imx {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct i2c_client *i2c;
	const struct drm_display_mode *mode;
	enum drm_panel_orientation orientation;
};

struct ws_panel_imx_data {
	const struct drm_display_mode *mode;
	int lanes;
	unsigned long mode_flags;
};

/* 2.8inch 480x640
 * https://www.waveshare.com/product/raspberry-pi/displays/2.8inch-dsi-lcd.htm
 */
static const struct drm_display_mode ws_panel_imx_2_8_mode = {
	.clock = 50000,
	.hdisplay = 480,
	.hsync_start = 480 + 150,
	.hsync_end = 480 + 150 + 50,
	.htotal = 480 + 150 + 50 + 150,
	.vdisplay = 640,
	.vsync_start = 640 + 150,
	.vsync_end = 640 + 150 + 50,
	.vtotal = 640 + 150 + 50 + 150,
};

static const struct ws_panel_imx_data ws_panel_imx_2_8_data = {
	.mode = &ws_panel_imx_2_8_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 3.4inch 800x800 Round
 * https://www.waveshare.com/product/displays/lcd-oled/3.4inch-dsi-lcd-c.htm
 */
static const struct drm_display_mode ws_panel_imx_3_4_mode = {
	.clock = 50000,
	.hdisplay = 800,
	.hsync_start = 800 + 32,
	.hsync_end = 800 + 32 + 6,
	.htotal = 800 + 32 + 6 + 120,
	.vdisplay = 800,
	.vsync_start = 800 + 8,
	.vsync_end = 800 + 8 + 4,
	.vtotal = 800 + 8 + 4 + 16,
};

static const struct ws_panel_imx_data ws_panel_imx_3_4_data = {
	.mode = &ws_panel_imx_3_4_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 4.0inch 480x800
 * https://www.waveshare.com/product/raspberry-pi/displays/4inch-dsi-lcd.htm
 */
static const struct drm_display_mode ws_panel_imx_4_0_mode = {
	.clock = 50000,
	.hdisplay = 480,
	.hsync_start = 480 + 150,
	.hsync_end = 480 + 150 + 100,
	.htotal = 480 + 150 + 100 + 150,
	.vdisplay = 800,
	.vsync_start = 800 + 20,
	.vsync_end = 800 + 20 + 100,
	.vtotal = 800 + 20 + 100 + 20,
};

static const struct ws_panel_imx_data ws_panel_imx_4_0_data = {
	.mode = &ws_panel_imx_4_0_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 7.0inch C 1024x600
 * https://www.waveshare.com/product/raspberry-pi/displays/lcd-oled/7inch-dsi-lcd-c-with-case-a.htm
 */
static const struct drm_display_mode ws_panel_imx_7_0_c_mode = {
	.clock = 50000,
	.hdisplay = 1024,
	.hsync_start = 1024 + 100,
	.hsync_end = 1024 + 100 + 100,
	.htotal = 1024 + 100 + 100 + 100,
	.vdisplay = 600,
	.vsync_start = 600 + 10,
	.vsync_end = 600 + 10 + 10,
	.vtotal = 600 + 10 + 10 + 10,
};

static const struct ws_panel_imx_data ws_panel_imx_7_0_c_data = {
	.mode = &ws_panel_imx_7_0_c_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 7.9inch 400x1280
 * https://www.waveshare.com/product/raspberry-pi/displays/7.9inch-dsi-lcd.htm
 */
static const struct drm_display_mode ws_panel_imx_7_9_mode = {
	.clock = 50000,
	.hdisplay = 400,
	.hsync_start = 400 + 40,
	.hsync_end = 400 + 40 + 30,
	.htotal = 400 + 40 + 30 + 40,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 10,
	.vtotal = 1280 + 20 + 10 + 20,
};

static const struct ws_panel_imx_data ws_panel_imx_7_9_data = {
	.mode = &ws_panel_imx_7_9_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 8.0inch or 10.1inch 1280x800
 * https://www.waveshare.com/product/raspberry-pi/displays/8inch-dsi-lcd-c.htm
 * https://www.waveshare.com/product/raspberry-pi/displays/10.1inch-dsi-lcd-c.htm
 */
static const struct drm_display_mode ws_panel_imx_10_1_mode = {
	// 83333 -> 86666, due to new hsync values, computed as htotal*vtotal*fps / 1000
	.clock = 86666,
	.hdisplay = 1280,
	// 156 -> 216, computed as ~0.16*hdisplay
	.hsync_start = 1280 + 216,
	.hsync_end = 1280 + 216 + 20,
	.htotal = 1280 + 216 + 20 + 40,
	.vdisplay = 800,
	.vsync_start = 800 + 40,
	.vsync_end = 800 + 40 + 48,
	.vtotal = 800 + 40 + 48 + 40,
};

static const struct ws_panel_imx_data ws_panel_imx_10_1_data = {
	.mode = &ws_panel_imx_10_1_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 11.9inch 320x1480
 * https://www.waveshare.com/product/raspberry-pi/displays/11.9inch-dsi-lcd.htm
 */
static const struct drm_display_mode ws_panel_imx_11_9_mode = {
	.clock = 50000,
	.hdisplay = 320,
	.hsync_start = 320 + 60,
	.hsync_end = 320 + 60 + 60,
	.htotal = 320 + 60 + 60 + 60,
	.vdisplay = 1480,
	.vsync_start = 1480 + 60,
	.vsync_end = 1480 + 60 + 60,
	.vtotal = 1480 + 60 + 60 + 60,
};

static const struct ws_panel_imx_data ws_panel_imx_11_9_data = {
	.mode = &ws_panel_imx_11_9_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

static const struct drm_display_mode ws_panel_imx_4_mode = {
	.clock = 50000,
	.hdisplay = 720,
	.hsync_start = 720 + 32,
	.hsync_end = 720 + 32 + 200,
	.htotal = 720 + 32 + 200 + 120,
	.vdisplay = 720,
	.vsync_start = 720 + 8,
	.vsync_end = 720 + 8 + 4,
	.vtotal = 720 + 8 + 4 + 16,
};

static const struct ws_panel_imx_data ws_panel_imx_4_data = {
	.mode = &ws_panel_imx_4_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 5.0inch 720x1280
 * https://www.waveshare.com/5inch-dsi-lcd-d.htm
 */
static const struct drm_display_mode ws_panel_imx_5_0_mode = {
	.clock = 83333,
	.hdisplay = 720,
	.hsync_start = 720 + 100,
	.hsync_end = 720 + 100 + 80,
	.htotal = 720 + 100 + 80 + 100,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 20,
	.vtotal = 1280 + 20 + 20 + 20,
};

static const struct ws_panel_imx_data ws_panel_imx_5_0_data = {
	.mode = &ws_panel_imx_5_0_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 6.25inch 720x1560
 * https://www.waveshare.com/6.25inch-dsi-lcd.htm
 */
static const struct drm_display_mode ws_panel_imx_6_25_mode = {
	.clock = 83333,
	.hdisplay = 720,
	.hsync_start = 720 + 50,
	.hsync_end = 720 + 50 + 50,
	.htotal = 720 + 50 + 50 + 50,
	.vdisplay = 1560,
	.vsync_start = 1560 + 20,
	.vsync_end = 1560 + 20 + 20,
	.vtotal = 1560 + 20 + 20 + 20,
};

static const struct ws_panel_imx_data ws_panel_imx_6_25_data = {
	.mode = &ws_panel_imx_6_25_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

/* 8.8inch 480x1920
 * https://www.waveshare.com/8.8inch-dsi-lcd.htm
 */
static const struct drm_display_mode ws_panel_imx_8_8_mode = {
	.clock = 83333,
	.hdisplay = 480,
	.hsync_start = 480 + 50,
	.hsync_end = 480 + 50 + 50,
	.htotal = 480 + 50 + 50 + 50,
	.vdisplay = 1920,
	.vsync_start = 1920 + 20,
	.vsync_end = 1920 + 20 + 20,
	.vtotal = 1920 + 20 + 20 + 20,
};

static const struct ws_panel_imx_data ws_panel_imx_8_8_data = {
	.mode = &ws_panel_imx_8_8_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
};

static const struct drm_display_mode ws_panel_imx_13_3_4lane_mode = {
	.clock = 148500,
	.hdisplay = 1920,
	.hsync_start = 1920 + 88,
	.hsync_end = 1920 + 88 + 44,
	.htotal = 1920 + 88 + 44 + 148,
	.vdisplay = 1080,
	.vsync_start = 1080 + 4,
	.vsync_end = 1080 + 4 + 5,
	.vtotal = 1080 + 4 + 5 + 36,
};

static const struct ws_panel_imx_data ws_panel_imx_13_3_4lane_data = {
	.mode = &ws_panel_imx_13_3_4lane_mode,
	.lanes = 4,
	.mode_flags = MIPI_DSI_MODE_VIDEO  | MIPI_DSI_MODE_LPM,
};

static const struct drm_display_mode ws_panel_imx_13_3_2lane_mode = {
	.clock = 83333,
	.hdisplay = 1920,
	.hsync_start = 1920 + 88,
	.hsync_end = 1920 + 88 + 44,
	.htotal = 1920 + 88 + 44 + 148,
	.vdisplay = 1080,
	.vsync_start = 1080 + 4,
	.vsync_end = 1080 + 4 + 5,
	.vtotal = 1080 + 4 + 5 + 36,
};

static const struct ws_panel_imx_data ws_panel_imx_13_3_2lane_data = {
	.mode = &ws_panel_imx_13_3_2lane_mode,
	.lanes = 2,
	.mode_flags = MIPI_DSI_MODE_VIDEO  | MIPI_DSI_MODE_LPM,
};

static struct ws_panel_imx *panel_to_ts(struct drm_panel *panel)
{
	return container_of(panel, struct ws_panel_imx, base);
}

static void ws_panel_imx_i2c_write(struct ws_panel_imx *ts, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(ts->i2c, reg, val);
	if (ret)
		dev_err(&ts->i2c->dev, "I2C write failed: %d\n", ret);
}

static int ws_panel_imx_disable(struct drm_panel *panel)
{
	struct ws_panel_imx *ts = panel_to_ts(panel);

	ws_panel_imx_i2c_write(ts, 0xad, 0x00);

	return 0;
}

static int ws_panel_imx_unprepare(struct drm_panel *panel)
{
	return 0;
}

static int ws_panel_imx_prepare(struct drm_panel *panel)
{
	return 0;
}

static int ws_panel_imx_enable(struct drm_panel *panel)
{
	struct ws_panel_imx *ts = panel_to_ts(panel);

	if (ts->mode == &ws_panel_imx_13_3_2lane_mode)
		ws_panel_imx_i2c_write(ts, 0xad, 0x02);
	else
		ws_panel_imx_i2c_write(ts, 0xad, 0x01);

	return 0;
}

static int ws_panel_imx_get_modes(struct drm_panel *panel,
			      struct drm_connector *connector)
{
	static const u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	struct ws_panel_imx *ts = panel_to_ts(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ts->mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			ts->mode->hdisplay,
			ts->mode->vdisplay,
			drm_mode_vrefresh(ts->mode));
	}

	mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	connector->display_info.bpc = 8;
	connector->display_info.width_mm = 154;
	connector->display_info.height_mm = 86;
	drm_display_info_set_bus_formats(&connector->display_info,
					 &bus_format, 1);

	/*
	 * TODO: Remove once all drm drivers call
	 * drm_connector_set_orientation_from_panel()
	 */
	drm_connector_set_panel_orientation(connector, ts->orientation);

	return 1;
}

static const struct drm_panel_funcs ws_panel_imx_funcs = {
	.disable = ws_panel_imx_disable,
	.unprepare = ws_panel_imx_unprepare,
	.prepare = ws_panel_imx_prepare,
	.enable = ws_panel_imx_enable,
	.get_modes = ws_panel_imx_get_modes,
};

static int ws_panel_imx_bl_update_status(struct backlight_device *bl)
{
	struct ws_panel_imx *ts = bl_get_data(bl);

	ws_panel_imx_i2c_write(ts, 0xab, 0xff - backlight_get_brightness(bl));
	ws_panel_imx_i2c_write(ts, 0xaa, 0x01);

	return 0;
}

static const struct backlight_ops ws_panel_imx_bl_ops = {
	.update_status = ws_panel_imx_bl_update_status,
};

static struct backlight_device *
ws_panel_imx_create_backlight(struct ws_panel_imx *ts)
{
	struct device *dev = ts->base.dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 255,
		.max_brightness = 255,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, ts,
					      &ws_panel_imx_bl_ops, &props);
}

static int ws_panel_imx_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ws_panel_imx *ts;
	const struct ws_panel_imx_data *_ws_panel_imx_data;
	struct device_node *node;
	struct i2c_adapter *i2c_bus;
	struct i2c_client *i2c_dev;
	int ret;

	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	_ws_panel_imx_data = of_device_get_match_data(dev);
	if (!_ws_panel_imx_data)
		return -EINVAL;

	ts->mode = _ws_panel_imx_data->mode;
	if (!ts->mode)
		return -EINVAL;

	node = of_parse_phandle(dev->of_node, "i2c-bus", 0); 
	if (!node) {
		dev_err(dev, "Cannot find i2c-bus\n");
		return -ENODEV;
	}

	i2c_bus = of_find_i2c_adapter_by_node(node);
	of_node_put(node);
	if (!i2c_bus)
		return -EPROBE_DEFER;

	i2c_dev = devm_i2c_new_dummy_device(dev, i2c_bus, WS_DSI_I2C_ADDR);
	i2c_put_adapter(i2c_bus);
	if (IS_ERR(i2c_dev)) {
		dev_err(dev, "Failed to allocate I2C device\n");
		return PTR_ERR(i2c_dev);
	}

	ts->i2c = i2c_dev;

	ws_panel_imx_i2c_write(ts, 0xc0, 0x01);
	ws_panel_imx_i2c_write(ts, 0xc2, 0x01);
	ws_panel_imx_i2c_write(ts, 0xac, 0x01);

	ret = of_drm_get_panel_orientation(dev->of_node, &ts->orientation);
	if (ret) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, ret);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, ts);
	ts->dsi = dsi;

	drm_panel_init(&ts->base, dev, &ws_panel_imx_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ts->base.backlight = ws_panel_imx_create_backlight(ts);
	if (IS_ERR(ts->base.backlight)) {
		ret = PTR_ERR(ts->base.backlight);
		dev_err(dev, "Failed to create backlight: %d\n", ret);
		return ret;
	}

	drm_panel_add(&ts->base);

	// TODO: SYNC_PULSE was added just to make things work
	ts->dsi->mode_flags = _ws_panel_imx_data->mode_flags | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	ts->dsi->format = MIPI_DSI_FMT_RGB888;
	ts->dsi->lanes = _ws_panel_imx_data->lanes;

	ret = mipi_dsi_attach(ts->dsi);
	if (ret) {
		drm_panel_remove(&ts->base);
		dev_err(dev, "Failed to attach dsi to host: %d\n", ret);
	}

	return ret;
}

static int ws_panel_imx_remove(struct mipi_dsi_device *dsi)
{
	struct ws_panel_imx *ts = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(ts->dsi);
	drm_panel_remove(&ts->base);

	return 0;
}

static void ws_panel_imx_shutdown(struct mipi_dsi_device *dsi)
{
	struct ws_panel_imx *ts = mipi_dsi_get_drvdata(dsi);

	drm_panel_disable(&ts->base);
	drm_panel_unprepare(&ts->base);
}

static const struct of_device_id ws_panel_imx_of_ids[] = {
	{
		.compatible = "waveshare,2.8inch-panel",
		.data = &ws_panel_imx_2_8_data,
	}, {
		.compatible = "waveshare,3.4inch-panel",
		.data = &ws_panel_imx_3_4_data,
	}, {
		.compatible = "waveshare,4.0inch-panel",
		.data = &ws_panel_imx_4_0_data,
	}, {
		.compatible = "waveshare,7.0inch-c-panel",
		.data = &ws_panel_imx_7_0_c_data,
	}, {
		.compatible = "waveshare,7.9inch-panel",
		.data = &ws_panel_imx_7_9_data,
	}, {
		.compatible = "waveshare,8.0inch-panel",
		.data = &ws_panel_imx_10_1_data,
	}, {
		.compatible = "waveshare,10.1inch-panel",
		.data = &ws_panel_imx_10_1_data,
	}, {
		.compatible = "waveshare,11.9inch-panel",
		.data = &ws_panel_imx_11_9_data,
	}, {
		.compatible = "waveshare,4inch-panel",
		.data = &ws_panel_imx_4_data,
	}, {
		.compatible = "waveshare,5.0inch-panel",
		.data = &ws_panel_imx_5_0_data,
	}, {
		.compatible = "waveshare,6.25inch-panel",
		.data = &ws_panel_imx_6_25_data,
	}, {
		.compatible = "waveshare,8.8inch-panel",
		.data = &ws_panel_imx_8_8_data,
	}, {
		.compatible = "waveshare,13.3inch-4lane-panel",
		.data = &ws_panel_imx_13_3_4lane_data,
	}, {
		.compatible = "waveshare,13.3inch-2lane-panel",
		.data = &ws_panel_imx_13_3_2lane_data,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, ws_panel_imx_of_ids);

static struct mipi_dsi_driver ws_panel_imx_driver = {
	.driver = {
		.name = "ws_panel_imx",
		.of_match_table = ws_panel_imx_of_ids,
	},
	.probe = ws_panel_imx_probe,
	.remove = ws_panel_imx_remove,
	.shutdown = ws_panel_imx_shutdown,
};
module_mipi_dsi_driver(ws_panel_imx_driver);

MODULE_AUTHOR("TODO <todo@todo.it>");
MODULE_DESCRIPTION("Waveshare DSI panel driver (IMX compatible)");
MODULE_LICENSE("GPL v2");
