// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define DW9768_NAME				"dw9768"
#define DW9768_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define DW9768_FOCUS_STEPS			1
#define DW9768_CONTROL_REG			0x02
#define DW9768_SET_POSITION_ADDR		0x03
#define DW9768_CONTOL_POWER_DOWN		BIT(0)
#define DW9768_AAC_MODE_EN			BIT(1)

#define DW9768_CMD_DELAY			0xff
#define DW9768_CTRL_DELAY_US			5000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define DW9768_MOVE_STEPS			16
#define DW9768_MOVE_DELAY_US			8400
#define DW9768_STABLE_TIME_US			20000

/* dw9768 device structure */
struct dw9768 {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
};

static inline struct dw9768 *to_dw9768_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9768, ctrls);
}

static inline struct dw9768 *sd_to_dw9768_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9768, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list dw9768_init_regs[] = {
	{DW9768_CONTROL_REG, DW9768_AAC_MODE_EN},
	{DW9768_CMD_DELAY, DW9768_CMD_DELAY},
	{0x06, 0x41},
	{0x07, 0x39},
	{DW9768_CMD_DELAY, DW9768_CMD_DELAY},
};

static int dw9768_write_smbus(struct dw9768 *dw9768, unsigned char reg,
			      unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9768->sd);
	int ret = 0;

	if (reg == DW9768_CMD_DELAY  && value == DW9768_CMD_DELAY)
		usleep_range(DW9768_CTRL_DELAY_US,
			     DW9768_CTRL_DELAY_US + 100);
	else
		ret = i2c_smbus_write_byte_data(client, reg, value);
	return ret;
}

static int dw9768_write_array(struct dw9768 *dw9768, struct regval_list *vals,
			      u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = dw9768_write_smbus(dw9768, vals[i].reg_num,
					 vals[i].value);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int dw9768_set_position(struct dw9768 *dw9768, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9768->sd);

	return i2c_smbus_write_word_data(client, DW9768_SET_POSITION_ADDR,
					 swab16(val));
}

static int dw9768_release(struct dw9768 *dw9768)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9768->sd);
	int ret, val;

	for (val = round_down(dw9768->focus->val, DW9768_MOVE_STEPS);
	     val >= 0; val -= DW9768_MOVE_STEPS) {
		ret = dw9768_set_position(dw9768, val);
		if (ret) {
			dev_err(&client->dev, "%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9768_MOVE_DELAY_US,
			     DW9768_MOVE_DELAY_US + 1000);
	}

	/*
	 * Wait for the motor to stabilize after the last movement
	 * to prevent the motor from shaking.
	 */
	usleep_range(DW9768_STABLE_TIME_US - DW9768_MOVE_DELAY_US,
		     DW9768_STABLE_TIME_US - DW9768_MOVE_DELAY_US + 1000);

	ret = i2c_smbus_write_byte_data(client, DW9768_CONTROL_REG,
					DW9768_CONTOL_POWER_DOWN);
	if (ret)
		return ret;

	usleep_range(DW9768_CTRL_DELAY_US, DW9768_CTRL_DELAY_US + 100);

	return 0;
}

static int dw9768_init(struct dw9768 *dw9768)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9768->sd);
	int ret, val;

	ret = dw9768_write_array(dw9768, dw9768_init_regs,
				 ARRAY_SIZE(dw9768_init_regs));
	if (ret)
		return ret;

	for (val = dw9768->focus->val % DW9768_MOVE_STEPS;
	     val <= dw9768->focus->val;
	     val += DW9768_MOVE_STEPS) {
		ret = dw9768_set_position(dw9768, val);
		if (ret) {
			dev_err(&client->dev, "%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(DW9768_MOVE_DELAY_US,
			     DW9768_MOVE_DELAY_US + 1000);
	}

	return 0;
}

/* Power handling */
static int dw9768_power_off(struct dw9768 *dw9768)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dw9768->sd);
	int ret;

	ret = dw9768_release(dw9768);
	if (ret)
		dev_err(&client->dev, "dw9768 release failed!\n");

	ret = regulator_disable(dw9768->vin);
	if (ret)
		return ret;

	return regulator_disable(dw9768->vdd);
}

static int dw9768_power_on(struct dw9768 *dw9768)
{
	int ret;

	ret = regulator_enable(dw9768->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(dw9768->vdd);
	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(DW9768_CTRL_DELAY_US, DW9768_CTRL_DELAY_US + 100);

	ret = dw9768_init(dw9768);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(dw9768->vin);
	regulator_disable(dw9768->vdd);

	return ret;
}

static int dw9768_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dw9768 *dw9768 = to_dw9768_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE)
		return dw9768_set_position(dw9768, ctrl->val);

	return 0;
}

static const struct v4l2_ctrl_ops dw9768_vcm_ctrl_ops = {
	.s_ctrl = dw9768_set_ctrl,
};

static int dw9768_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int dw9768_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops dw9768_int_ops = {
	.open = dw9768_open,
	.close = dw9768_close,
};

static const struct v4l2_subdev_ops dw9768_ops = { };

static void dw9768_subdev_cleanup(struct dw9768 *dw9768)
{
	v4l2_async_unregister_subdev(&dw9768->sd);
	v4l2_ctrl_handler_free(&dw9768->ctrls);
	media_entity_cleanup(&dw9768->sd.entity);
}

static int dw9768_init_controls(struct dw9768 *dw9768)
{
	struct v4l2_ctrl_handler *hdl = &dw9768->ctrls;
	const struct v4l2_ctrl_ops *ops = &dw9768_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	dw9768->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, DW9768_MAX_FOCUS_POS, DW9768_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	dw9768->sd.ctrl_handler = hdl;

	return 0;
}

static int dw9768_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct dw9768 *dw9768;
	int ret;

	dw9768 = devm_kzalloc(dev, sizeof(*dw9768), GFP_KERNEL);
	if (!dw9768)
		return -ENOMEM;

	dw9768->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(dw9768->vin)) {
		ret = PTR_ERR(dw9768->vin);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "cannot get vin regulator\n");
		return ret;
	}

	dw9768->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(dw9768->vdd)) {
		ret = PTR_ERR(dw9768->vdd);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "cannot get vdd regulator\n");
		return ret;
	}

	v4l2_i2c_subdev_init(&dw9768->sd, client, &dw9768_ops);
	dw9768->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9768->sd.internal_ops = &dw9768_int_ops;

	ret = dw9768_init_controls(dw9768);
	if (ret)
		goto err_cleanup;

	ret = media_entity_pads_init(&dw9768->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	dw9768->sd.entity.function = MEDIA_ENT_F_LENS;

	ret = v4l2_async_register_subdev(&dw9768->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	dw9768_subdev_cleanup(dw9768);
	return ret;
}

static int dw9768_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9768 *dw9768 = sd_to_dw9768_vcm(sd);

	dw9768_subdev_cleanup(dw9768);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		dw9768_power_off(dw9768);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused dw9768_vcm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9768 *dw9768 = sd_to_dw9768_vcm(sd);

	return dw9768_power_off(dw9768);
}

static int __maybe_unused dw9768_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9768 *dw9768 = sd_to_dw9768_vcm(sd);

	return dw9768_power_on(dw9768);
}

static const struct i2c_device_id dw9768_id_table[] = {
	{ DW9768_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw9768_id_table);

static const struct of_device_id dw9768_of_table[] = {
	{ .compatible = "dongwoon,dw9768" },
	{ },
};
MODULE_DEVICE_TABLE(of, dw9768_of_table);

static const struct dev_pm_ops dw9768_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dw9768_vcm_suspend, dw9768_vcm_resume, NULL)
};

static struct i2c_driver dw9768_i2c_driver = {
	.driver = {
		.name = DW9768_NAME,
		.pm = &dw9768_pm_ops,
		.of_match_table = dw9768_of_table,
	},
	.probe_new  = dw9768_probe,
	.remove = dw9768_remove,
	.id_table = dw9768_id_table,
};

module_i2c_driver(dw9768_i2c_driver);

MODULE_AUTHOR("Dongchun Zhu <dongchun.zhu@mediatek.com>");
MODULE_DESCRIPTION("DW9768 VCM driver");
MODULE_LICENSE("GPL v2");
