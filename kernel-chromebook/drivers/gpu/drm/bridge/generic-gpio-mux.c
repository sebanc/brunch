/*
 * ANX7688 HDMI->DP bridge driver
 *
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

struct gpio_display_mux {
	struct device *dev;

	struct gpio_desc *gpiod_detect;
	int detect_irq;

	struct drm_bridge bridge;

	struct drm_bridge *next[2];
};

static inline struct gpio_display_mux *bridge_to_gpio_display_mux(
		struct drm_bridge *bridge)
{
	return container_of(bridge, struct gpio_display_mux, bridge);
}

static irqreturn_t gpio_display_mux_det_threaded_handler(int unused, void *data)
{
	struct gpio_display_mux *gpio_display_mux = data;
	int active = gpiod_get_value(gpio_display_mux->gpiod_detect);

	dev_dbg(gpio_display_mux->dev, "Interrupt %d!\n", active);

	if (gpio_display_mux->bridge.dev)
		drm_kms_helper_hotplug_event(gpio_display_mux->bridge.dev);

	return IRQ_HANDLED;
}

static int gpio_display_mux_attach(struct drm_bridge *bridge)
{
	struct gpio_display_mux *gpio_display_mux =
			bridge_to_gpio_display_mux(bridge);
	struct drm_bridge *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_display_mux->next); i++) {
		next = gpio_display_mux->next[i];
		if (next)
			next->encoder = bridge->encoder;
	}

	return 0;
}

static bool gpio_display_mux_mode_fixup(struct drm_bridge *bridge,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct gpio_display_mux *gpio_display_mux =
		bridge_to_gpio_display_mux(bridge);
	int active;
	struct drm_bridge *next;

	active = gpiod_get_value(gpio_display_mux->gpiod_detect);
	next = gpio_display_mux->next[active];

	if (next && next->funcs->mode_fixup)
		return next->funcs->mode_fixup(next, mode, adjusted_mode);
	else
		return true;
}

static void gpio_display_mux_mode_set(struct drm_bridge *bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct gpio_display_mux *gpio_display_mux =
		bridge_to_gpio_display_mux(bridge);
	int active;
	struct drm_bridge *next;

	active = gpiod_get_value(gpio_display_mux->gpiod_detect);
	next = gpio_display_mux->next[active];

	if (next && next->funcs->mode_set)
		next->funcs->mode_set(next, mode, adjusted_mode);
}

/**
 * Since this driver _reacts_ to mux changes, we need to make sure all
 * downstream bridges are pre-enabled.
 */
static void gpio_display_mux_pre_enable(struct drm_bridge *bridge)
{
	struct gpio_display_mux *gpio_display_mux =
		bridge_to_gpio_display_mux(bridge);
	struct drm_bridge *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_display_mux->next); i++) {
		next = gpio_display_mux->next[i];
		if (next && next->funcs->pre_enable)
			next->funcs->pre_enable(next);
	}
}

static void gpio_display_mux_post_disable(struct drm_bridge *bridge)
{
	struct gpio_display_mux *gpio_display_mux =
		bridge_to_gpio_display_mux(bridge);
	struct drm_bridge *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_display_mux->next); i++) {
		next = gpio_display_mux->next[i];
		if (next && next->funcs->post_disable)
			next->funcs->post_disable(next);
	}
}

/**
 * In an ideal mux driver, only the currently selected bridge should be enabled.
 * For the sake of simplicity, we just just enable/disable all downstream
 * bridges at the same time.
 */
static void gpio_display_mux_enable(struct drm_bridge *bridge)
{
	struct gpio_display_mux *gpio_display_mux =
		bridge_to_gpio_display_mux(bridge);
	struct drm_bridge *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_display_mux->next); i++) {
		next = gpio_display_mux->next[i];
		if (next && next->funcs->enable)
			next->funcs->enable(next);
	}
}

static void gpio_display_mux_disable(struct drm_bridge *bridge)
{
	struct gpio_display_mux *gpio_display_mux =
		bridge_to_gpio_display_mux(bridge);
	struct drm_bridge *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_display_mux->next); i++) {
		next = gpio_display_mux->next[i];
		if (next && next->funcs->disable)
			next->funcs->disable(next);
	}
}

static const struct drm_bridge_funcs gpio_display_mux_bridge_funcs = {
	.attach = gpio_display_mux_attach,
	.mode_fixup = gpio_display_mux_mode_fixup,
	.disable = gpio_display_mux_disable,
	.post_disable = gpio_display_mux_post_disable,
	.mode_set = gpio_display_mux_mode_set,
	.pre_enable = gpio_display_mux_pre_enable,
	.enable = gpio_display_mux_enable,
};

static int gpio_display_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_display_mux *gpio_display_mux;
	struct device_node *port, *ep, *remote;
	int ret;
	u32 reg;

	gpio_display_mux = devm_kzalloc(dev, sizeof(*gpio_display_mux),
					GFP_KERNEL);
	if (!gpio_display_mux)
		return -ENOMEM;

	platform_set_drvdata(pdev, gpio_display_mux);
	gpio_display_mux->dev = &pdev->dev;

	gpio_display_mux->bridge.of_node = dev->of_node;

	gpio_display_mux->gpiod_detect =
		devm_gpiod_get(dev, "detect", GPIOD_IN);
	if (IS_ERR(gpio_display_mux->gpiod_detect))
		return PTR_ERR(gpio_display_mux->gpiod_detect);

	gpio_display_mux->detect_irq =
		gpiod_to_irq(gpio_display_mux->gpiod_detect);
	if (gpio_display_mux->detect_irq < 0) {
		dev_err(dev, "Failed to get output irq %d\n",
			gpio_display_mux->detect_irq);
		return -ENODEV;
	}

	port = of_graph_get_port_by_id(dev->of_node, 1);
	if (!port) {
		dev_err(dev, "Missing output port node\n");
		return -EINVAL;
	}

	for_each_child_of_node(port, ep) {
		if (!ep->name || (of_node_cmp(ep->name, "endpoint") != 0)) {
			of_node_put(ep);
			continue;
		}

		if (of_property_read_u32(ep, "reg", &reg) < 0 ||
				reg >= ARRAY_SIZE(gpio_display_mux->next)) {
			dev_err(dev,
			    "Missing/invalid reg property for endpoint %s\n",
				ep->full_name);
			of_node_put(ep);
			of_node_put(port);
			return -EINVAL;
		}

		remote = of_graph_get_remote_port_parent(ep);
		if (!remote) {
			dev_err(dev,
			    "Missing connector/bridge node for endpoint %s\n",
				ep->full_name);
			of_node_put(ep);
			of_node_put(port);
			return -EINVAL;
		}
		of_node_put(ep);

		if (of_device_is_compatible(remote, "hdmi-connector")) {
			of_node_put(remote);
			continue;
		}

		gpio_display_mux->next[reg] = of_drm_find_bridge(remote);
		if (!gpio_display_mux->next[reg]) {
			dev_err(dev, "Waiting for external bridge %s\n",
				remote->name);
			of_node_put(remote);
			of_node_put(port);
			return -EPROBE_DEFER;
		}

		of_node_put(remote);
	}
	of_node_put(port);

	gpio_display_mux->bridge.funcs = &gpio_display_mux_bridge_funcs;
	ret = drm_bridge_add(&gpio_display_mux->bridge);
	if (ret < 0) {
		dev_err(dev, "Failed to add drm bridge\n");
		return ret;
	}

	ret = devm_request_threaded_irq(dev, gpio_display_mux->detect_irq,
				NULL,
				gpio_display_mux_det_threaded_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT,
				"gpio-display-mux-det", gpio_display_mux);
	if (ret) {
		dev_err(dev, "Failed to request MUX_DET threaded irq\n");
		goto err_bridge_remove;
	}

	return 0;

err_bridge_remove:
	drm_bridge_remove(&gpio_display_mux->bridge);

	return ret;
}

static int gpio_display_mux_remove(struct platform_device *pdev)
{
	struct gpio_display_mux *gpio_display_mux = platform_get_drvdata(pdev);

	drm_bridge_remove(&gpio_display_mux->bridge);

	return 0;
}

static const struct of_device_id gpio_display_mux_match[] = {
	{ .compatible = "gpio-display-mux", },
	{},
};

struct platform_driver gpio_display_mux_driver = {
	.probe = gpio_display_mux_probe,
	.remove = gpio_display_mux_remove,
	.driver = {
		.name = "gpio-display-mux",
		.of_match_table = gpio_display_mux_match,
	},
};

module_platform_driver(gpio_display_mux_driver);

MODULE_DESCRIPTION("GPIO-controlled display mux");
MODULE_AUTHOR("Nicolas Boichat <drinkcat@chromium.org>");
MODULE_LICENSE("GPL v2");
