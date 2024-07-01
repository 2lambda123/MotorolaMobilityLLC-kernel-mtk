// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#define DRIVER_NAME "ak7377a"
#define AK7377A_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define AK7377A_NAME				"mot_dubai_ak7377a"
#define AK7377A_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define AK7377A_FOCUS_STEPS			1
#define AK7377A_SET_POSITION_ADDR		0x00

#define AK7377A_CMD_DELAY			0xff
#define AK7377A_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define AK7377A_MOVE_STEPS			16
#define AK7377A_MOVE_DELAY_US			8400
#define AK7377A_STABLE_TIME_US			20000

static struct i2c_client *g_pstAF_I2Cclient;

/* ak7377a device structure */
struct ak7377a_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};
#define DW9781_I2C_SLAVE_ADDR 0xE4

static struct i2c_client *dw9781_i2c_client = NULL;

void dw9781_set_i2c_client(struct i2c_client *i2c_client)
{
	dw9781_i2c_client = i2c_client;
}
static inline struct ak7377a_device *to_ak7377a_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct ak7377a_device, ctrls);
}

static inline struct ak7377a_device *sd_to_ak7377a_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ak7377a_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};


static int ak7377a_set_position(struct ak7377a_device *ak7377a, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377a->sd);
	int retry = 3;
	int ret;

	while (--retry > 0) {
		ret = i2c_smbus_write_word_data(client, AK7377A_SET_POSITION_ADDR,
					 swab16(val << 6));
		if (ret < 0) {
			usleep_range(AK7377A_MOVE_DELAY_US,
				     AK7377A_MOVE_DELAY_US + 1000);
		} else {
			break;
		}
	}
	return ret;
}

static int ak7377a_release(struct ak7377a_device *ak7377a)
{
	int ret, val;
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377a->sd);

	for (val = round_down(ak7377a->focus->val, AK7377A_MOVE_STEPS);
	     val >= 0; val -= AK7377A_MOVE_STEPS) {
		ret = ak7377a_set_position(ak7377a, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(AK7377A_MOVE_DELAY_US,
			     AK7377A_MOVE_DELAY_US + 1000);
	}

	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	msleep(20);

	/*
	 * Wait for the motor to stabilize after the last movement
	 * to prevent the motor from shaking.
	 */
	usleep_range(AK7377A_STABLE_TIME_US - AK7377A_MOVE_DELAY_US,
		     AK7377A_STABLE_TIME_US - AK7377A_MOVE_DELAY_US + 1000);

	return 0;
}

int write_reg_16bit_value_16bit(unsigned short regAddr, unsigned short regData)
{
	int i4RetValue = 0;
	struct i2c_msg msgs;
	char puSendCmd[4] = { (char)(regAddr>>8), (char)(regAddr&0xff),
	                      (char)(regData >> 8),
	                      (char)(regData&0xFF) };

	LOG_INF("DW9781 I2C read:%04x, data:%04x", regAddr, regData);

	if (!dw9781_i2c_client) {
		printk("[%s] FATAL: DW9781 i2c client is NULL!!!",__func__);
		return -1;
	}

	msgs.addr  = DW9781_I2C_SLAVE_ADDR >> 1;
	msgs.flags = 0;
	msgs.len   = 4;
	msgs.buf   = puSendCmd;

	i4RetValue = i2c_transfer(dw9781_i2c_client->adapter, &msgs, 1);
	if (i4RetValue != 1) {
		printk("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

int read_reg_16bit_value_16bit(unsigned short regAddr, unsigned short *regData)
{
	int i4RetValue = 0;
	unsigned short readTemp = 0;
	struct i2c_msg msg[2];
	char puReadCmd[2] = { (char)(regAddr>>8), (char)(regAddr&0xff)};

	LOG_INF("DW9781 I2C read:%04x", regAddr);

	if (!dw9781_i2c_client) {
		printk("[%s] FATAL: DW9781 i2c client is NULL!!!",__func__);
		return -1;
	}
	dw9781_i2c_client->addr = DW9781_I2C_SLAVE_ADDR >> 1;

	msg[0].addr = dw9781_i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;

	msg[1].addr = dw9781_i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = (u8*)&readTemp;

	i4RetValue = i2c_transfer(dw9781_i2c_client->adapter, msg, ARRAY_SIZE(msg));

	if (i4RetValue != 2) {
		printk("DW9781 I2C read failed!!\n");
		return -1;
	}

	*regData = ((readTemp>>8) & 0xff) | ((readTemp<<8) & 0xff00);

	return 0;
}


static void os_mdelay(unsigned long ms)
{
	unsigned long us = ms*1000;
	usleep_range(us, us+2000);
}

void ois_reset(void)
{
	LOG_INF("[dw9781c_ois_reset] ois reset\r\n");
	write_reg_16bit_value_16bit(0xD002, 0x0001); /* printfc reset */
	os_mdelay(4);
	write_reg_16bit_value_16bit(0xD001, 0x0001); /* Active mode (DSP ON) */
	os_mdelay(25); /* ST gyro - over wait 25ms, default Servo On */
	write_reg_16bit_value_16bit(0xEBF1, 0x56FA); /* User protection release */
}

static int ak7377a_init(struct ak7377a_device *ak7377a)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ak7377a->sd);
	int ret = 0;
	unsigned short lock_ois = 0;
	LOG_INF("+\n");

	client->addr = AK7377A_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);

	LOG_INF("Check HW version: %x\n", ret);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = i2c_smbus_write_byte_data(client, 0x02, 0x00);

	ois_reset();
	read_reg_16bit_value_16bit(0x7015, &lock_ois); /* lock_ois: 0x7015 */
	LOG_INF("Check HW lock_ois: %x\n", lock_ois);
	client->addr = AK7377A_I2C_SLAVE_ADDR >> 1;
	LOG_INF("-\n");

	return 0;
}

/* Power handling */
static int ak7377a_power_off(struct ak7377a_device *ak7377a)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = ak7377a_release(ak7377a);
	if (ret)
		LOG_INF("ak7377a release failed!\n");

	ret = regulator_disable(ak7377a->vin);
	if (ret)
		return ret;

	ret = regulator_disable(ak7377a->vdd);
	if (ret)
		return ret;

	if (ak7377a->vcamaf_pinctrl && ak7377a->vcamaf_off)
		ret = pinctrl_select_state(ak7377a->vcamaf_pinctrl,
					ak7377a->vcamaf_off);

	return ret;
}

static int ak7377a_power_on(struct ak7377a_device *ak7377a)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(ak7377a->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(ak7377a->vdd);
	if (ret < 0)
		return ret;

	if (ak7377a->vcamaf_pinctrl && ak7377a->vcamaf_on)
		ret = pinctrl_select_state(ak7377a->vcamaf_pinctrl,
					ak7377a->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(AK7377A_CTRL_DELAY_US, AK7377A_CTRL_DELAY_US + 100);

	ret = ak7377a_init(ak7377a);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(ak7377a->vin);
	regulator_disable(ak7377a->vdd);
	if (ak7377a->vcamaf_pinctrl && ak7377a->vcamaf_off) {
		pinctrl_select_state(ak7377a->vcamaf_pinctrl,
				ak7377a->vcamaf_off);
	}

	return ret;
}

static int ak7377a_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7377a_device *ak7377a = to_ak7377a_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = ak7377a_set_position(ak7377a, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops ak7377a_vcm_ctrl_ops = {
	.s_ctrl = ak7377a_set_ctrl,
};

static int ak7377a_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int ak7377a_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops ak7377a_int_ops = {
	.open = ak7377a_open,
	.close = ak7377a_close,
};

static const struct v4l2_subdev_ops ak7377a_ops = { };

static void ak7377a_subdev_cleanup(struct ak7377a_device *ak7377a)
{
	v4l2_async_unregister_subdev(&ak7377a->sd);
	v4l2_ctrl_handler_free(&ak7377a->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&ak7377a->sd.entity);
#endif
}

static int ak7377a_init_controls(struct ak7377a_device *ak7377a)
{
	struct v4l2_ctrl_handler *hdl = &ak7377a->ctrls;
	const struct v4l2_ctrl_ops *ops = &ak7377a_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	ak7377a->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, AK7377A_MAX_FOCUS_POS, AK7377A_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	ak7377a->sd.ctrl_handler = hdl;

	return 0;
}

static int ak7377a_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ak7377a_device *ak7377a;
	int ret;

	LOG_INF("%s\n", __func__);
	g_pstAF_I2Cclient = client;
	ak7377a = devm_kzalloc(dev, sizeof(*ak7377a), GFP_KERNEL);
	if (!ak7377a)
		return -ENOMEM;

	ak7377a->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(ak7377a->vin)) {
		ret = PTR_ERR(ak7377a->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	ak7377a->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ak7377a->vdd)) {
		ret = PTR_ERR(ak7377a->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	ak7377a->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ak7377a->vcamaf_pinctrl)) {
		ret = PTR_ERR(ak7377a->vcamaf_pinctrl);
		ak7377a->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		ak7377a->vcamaf_on = pinctrl_lookup_state(
			ak7377a->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(ak7377a->vcamaf_on)) {
			ret = PTR_ERR(ak7377a->vcamaf_on);
			ak7377a->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		ak7377a->vcamaf_off = pinctrl_lookup_state(
			ak7377a->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(ak7377a->vcamaf_off)) {
			ret = PTR_ERR(ak7377a->vcamaf_off);
			ak7377a->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&ak7377a->sd, client, &ak7377a_ops);
	ak7377a->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ak7377a->sd.internal_ops = &ak7377a_int_ops;
	dw9781_set_i2c_client(g_pstAF_I2Cclient);
	ret = ak7377a_init_controls(ak7377a);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&ak7377a->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	ak7377a->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&ak7377a->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	ak7377a_subdev_cleanup(ak7377a);
	return ret;
}

static int ak7377a_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7377a_device *ak7377a = sd_to_ak7377a_vcm(sd);

	LOG_INF("%s\n", __func__);

	ak7377a_subdev_cleanup(ak7377a);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		ak7377a_power_off(ak7377a);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused ak7377a_vcm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7377a_device *ak7377a = sd_to_ak7377a_vcm(sd);

	return ak7377a_power_off(ak7377a);
}

static int __maybe_unused ak7377a_vcm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ak7377a_device *ak7377a = sd_to_ak7377a_vcm(sd);

	return ak7377a_power_on(ak7377a);
}

static const struct i2c_device_id ak7377a_id_table[] = {
	{ AK7377A_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ak7377a_id_table);

static const struct of_device_id ak7377a_of_table[] = {
	{ .compatible = "mediatek,mot_dubai_ak7377a" },
	{ },
};
MODULE_DEVICE_TABLE(of, ak7377a_of_table);

static const struct dev_pm_ops ak7377a_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(ak7377a_vcm_suspend, ak7377a_vcm_resume, NULL)
};

static struct i2c_driver ak7377a_i2c_driver = {
	.driver = {
		.name = AK7377A_NAME,
		.pm = &ak7377a_pm_ops,
		.of_match_table = ak7377a_of_table,
	},
	.probe_new  = ak7377a_probe,
	.remove = ak7377a_remove,
	.id_table = ak7377a_id_table,
};

module_i2c_driver(ak7377a_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("AK7377A VCM driver");
MODULE_LICENSE("GPL v2");
