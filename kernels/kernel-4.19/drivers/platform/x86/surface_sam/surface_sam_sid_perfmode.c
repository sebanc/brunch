/*
 * Surface Performance Mode Driver.
 * Allows to change cooling capabilities based on user preference.
 */

#include <asm/unaligned.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "surface_sam_ssh.h"


#define SID_PARAM_PERM		(S_IRUGO | S_IWUSR)

enum sam_perf_mode {
	SAM_PERF_MODE_NORMAL   = 1,
	SAM_PERF_MODE_BATTERY  = 2,
	SAM_PERF_MODE_PERF1    = 3,
	SAM_PERF_MODE_PERF2    = 4,

	__SAM_PERF_MODE__START = 1,
	__SAM_PERF_MODE__END   = 4,
};

enum sid_param_perf_mode {
	SID_PARAM_PERF_MODE_AS_IS    = 0,
	SID_PARAM_PERF_MODE_NORMAL   = SAM_PERF_MODE_NORMAL,
	SID_PARAM_PERF_MODE_BATTERY  = SAM_PERF_MODE_BATTERY,
	SID_PARAM_PERF_MODE_PERF1    = SAM_PERF_MODE_PERF1,
	SID_PARAM_PERF_MODE_PERF2    = SAM_PERF_MODE_PERF2,

	__SID_PARAM_PERF_MODE__START = 0,
	__SID_PARAM_PERF_MODE__END   = 4,
};


static int surface_sam_perf_mode_get(void)
{
	u8 result_buf[8] = { 0 };
	int status;

	struct surface_sam_ssh_rqst rqst = {
		.tc  = 0x03,
		.cid = 0x02,
		.iid = 0x00,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0x01,
		.cdl = 0x00,
		.pld = NULL,
	};

	struct surface_sam_ssh_buf result = {
		.cap = ARRAY_SIZE(result_buf),
		.len = 0,
		.data = result_buf,
	};

	status = surface_sam_ssh_rqst(&rqst, &result);
	if (status) {
		return status;
	}

	if (result.len != 8) {
		return -EFAULT;
	}

	return get_unaligned_le32(&result.data[0]);
}

static int surface_sam_perf_mode_set(int perf_mode)
{
	u8 payload[4] = { 0 };

	struct surface_sam_ssh_rqst rqst = {
		.tc  = 0x03,
		.cid = 0x03,
		.iid = 0x00,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0x00,
		.cdl = ARRAY_SIZE(payload),
		.pld = payload,
	};

	if (perf_mode < __SAM_PERF_MODE__START || perf_mode > __SAM_PERF_MODE__END) {
		return -EINVAL;
	}

	put_unaligned_le32(perf_mode, &rqst.pld[0]);
	return surface_sam_ssh_rqst(&rqst, NULL);
}


static int param_perf_mode_set(const char *val, const struct kernel_param *kp)
{
	int perf_mode;
	int status;

	status = kstrtoint(val, 0, &perf_mode);
	if (status) {
		return status;
	}

	if (perf_mode < __SID_PARAM_PERF_MODE__START || perf_mode > __SID_PARAM_PERF_MODE__END) {
		return -EINVAL;
	}

	return param_set_int(val, kp);
}

static const struct kernel_param_ops param_perf_mode_ops = {
	.set = param_perf_mode_set,
	.get = param_get_int,
};

static int param_perf_mode_init = SID_PARAM_PERF_MODE_AS_IS;
static int param_perf_mode_exit = SID_PARAM_PERF_MODE_AS_IS;

module_param_cb(perf_mode_init, &param_perf_mode_ops, &param_perf_mode_init, SID_PARAM_PERM);
module_param_cb(perf_mode_exit, &param_perf_mode_ops, &param_perf_mode_exit, SID_PARAM_PERM);

MODULE_PARM_DESC(perf_mode_init, "Performance-mode to be set on module initialization");
MODULE_PARM_DESC(perf_mode_exit, "Performance-mode to be set on module exit");


static ssize_t perf_mode_show(struct device *dev, struct device_attribute *attr, char *data)
{
	int perf_mode;

	perf_mode = surface_sam_perf_mode_get();
	if (perf_mode < 0) {
		dev_err(dev, "failed to get current performance mode: %d", perf_mode);
		return -EIO;
	}

	return sprintf(data, "%d\n", perf_mode);
}

static ssize_t perf_mode_store(struct device *dev, struct device_attribute *attr,
                               const char *data, size_t count)
{
	int perf_mode;
	int status;

	status = kstrtoint(data, 0, &perf_mode);
	if (status) {
		return status;
	}

	status = surface_sam_perf_mode_set(perf_mode);
	if (status) {
		return status;
	}

	// TODO: Should we notify ACPI here?
	//
	//       There is a _DSM call described as
	//           WSID._DSM: Notify DPTF on Slider State change
	//       which calls
	//           ODV3 = ToInteger (Arg3)
	//           Notify(IETM, 0x88)
	//       IETM is an INT3400 Intel Dynamic Power Performance Management
	//       device, part of the DPTF framework. From the corresponding
	//       kernel driver, it looks like event 0x88 is being ignored. Also
	//       it is currently unknown what the consequecnes of setting ODV3
	//       are.

	return count;
}

const static DEVICE_ATTR_RW(perf_mode);


static int surface_sam_sid_perfmode_probe(struct platform_device *pdev)
{
	int status;

	// link to ec
	status = surface_sam_ssh_consumer_register(&pdev->dev);
	if (status) {
		return status == -ENXIO ? -EPROBE_DEFER : status;
	}

	// set initial perf_mode
	if (param_perf_mode_init != SID_PARAM_PERF_MODE_AS_IS) {
		status = surface_sam_perf_mode_set(param_perf_mode_init);
		if (status) {
			return status;
		}
	}

	// register perf_mode attribute
	status = sysfs_create_file(&pdev->dev.kobj, &dev_attr_perf_mode.attr);
	if (status) {
		goto err_sysfs;
	}

	return 0;

err_sysfs:
	surface_sam_perf_mode_set(param_perf_mode_exit);
	return status;
}

static int surface_sam_sid_perfmode_remove(struct platform_device *pdev)
{
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_perf_mode.attr);
	surface_sam_perf_mode_set(param_perf_mode_exit);
	return 0;
}

static struct platform_driver surface_sam_sid_perfmode = {
	.probe = surface_sam_sid_perfmode_probe,
	.remove = surface_sam_sid_perfmode_remove,
	.driver = {
		.name = "surface_sam_sid_perfmode",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(surface_sam_sid_perfmode);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Surface Performance Mode Driver for 5th Generation Surface Devices");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:surface_sam_sid_perfmode");
