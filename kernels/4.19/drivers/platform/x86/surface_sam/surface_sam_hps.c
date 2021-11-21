/*
 * Surface dGPU hot-plug system driver.
 * Supports explicit setting of the dGPU power-state on the Surface Book 2 and
 * properly handles hot-plugging by detaching the base.
 */

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#include "surface_sam_ssh.h"
#include "surface_sam_san.h"


// TODO: vgaswitcheroo integration


static void dbg_dump_drvsta(struct platform_device *pdev, const char *prefix);


#define SHPS_DSM_REVISION	1
#define SHPS_DSM_GPU_ADDRS	0x02
#define SHPS_DSM_GPU_POWER	0x05
static const guid_t SHPS_DSM_UUID =
	GUID_INIT(0x5515a847, 0xed55, 0x4b27, 0x83, 0x52, 0xcd,
		  0x32, 0x0e, 0x10, 0x36, 0x0a);


#define SAM_DGPU_TC			0x13
#define SAM_DGPU_CID_POWERON		0x02

#define SAM_DTX_TC			0x11
#define SAM_DTX_CID_LATCH_LOCK		0x06
#define SAM_DTX_CID_LATCH_UNLOCK	0x07

#define SHPS_DSM_GPU_ADDRS_RP		"RP5_PCIE"
#define SHPS_DSM_GPU_ADDRS_DGPU		"DGPU_PCIE"


static const struct acpi_gpio_params gpio_base_presence_int = { 0, 0, false };
static const struct acpi_gpio_params gpio_base_presence     = { 1, 0, false };
static const struct acpi_gpio_params gpio_dgpu_power_int    = { 2, 0, false };
static const struct acpi_gpio_params gpio_dgpu_power        = { 3, 0, false };
static const struct acpi_gpio_params gpio_dgpu_presence_int = { 4, 0, false };
static const struct acpi_gpio_params gpio_dgpu_presence     = { 5, 0, false };

static const struct acpi_gpio_mapping shps_acpi_gpios[] = {
	{ "base_presence-int-gpio", &gpio_base_presence_int, 1 },
	{ "base_presence-gpio",     &gpio_base_presence,     1 },
	{ "dgpu_power-int-gpio",    &gpio_dgpu_power_int,    1 },
	{ "dgpu_power-gpio",        &gpio_dgpu_power,        1 },
	{ "dgpu_presence-int-gpio", &gpio_dgpu_presence_int, 1 },
	{ "dgpu_presence-gpio",     &gpio_dgpu_presence,     1 },
	{ },
};


enum shps_dgpu_power {
	SHPS_DGPU_POWER_OFF      = 0,
	SHPS_DGPU_POWER_ON       = 1,
	SHPS_DGPU_POWER_UNKNOWN  = 2,
};

static const char* shps_dgpu_power_str(enum shps_dgpu_power power) {
	if (power == SHPS_DGPU_POWER_OFF)
		return "off";
	else if (power == SHPS_DGPU_POWER_ON)
		return "on";
	else if (power == SHPS_DGPU_POWER_UNKNOWN)
		return "unknown";
	else
		return "<invalid>";
}


struct shps_driver_data {
	struct mutex lock;
	struct pci_dev *dgpu_root_port;
	struct pci_saved_state *dgpu_root_port_state;
	struct gpio_desc *gpio_dgpu_power;
	struct gpio_desc *gpio_dgpu_presence;
	struct gpio_desc *gpio_base_presence;
	unsigned int irq_dgpu_presence;
	unsigned int irq_base_presence;
	unsigned long state;
};

#define SHPS_STATE_BIT_PWRTGT		0	/* desired power state: 1 for on, 0 for off */
#define SHPS_STATE_BIT_RPPWRON_SYNC	1	/* synchronous/requested power-up in progress  */
#define SHPS_STATE_BIT_WAKE_ENABLED	2	/* wakeup via base-presence GPIO enabled */


#define SHPS_DGPU_PARAM_PERM		(S_IRUGO | S_IWUSR)

enum shps_dgpu_power_mp {
	SHPS_DGPU_MP_POWER_OFF  = SHPS_DGPU_POWER_OFF,
	SHPS_DGPU_MP_POWER_ON   = SHPS_DGPU_POWER_ON,
	SHPS_DGPU_MP_POWER_ASIS = -1,

	__SHPS_DGPU_MP_POWER_START = -1,
	__SHPS_DGPU_MP_POWER_END   = 1,
};

static int param_dgpu_power_set(const char *val, const struct kernel_param *kp)
{
	int power = SHPS_DGPU_MP_POWER_OFF;
	int status;

	status = kstrtoint(val, 0, &power);
	if (status) {
		return status;
	}

	if (power < __SHPS_DGPU_MP_POWER_START || power > __SHPS_DGPU_MP_POWER_END) {
		return -EINVAL;
	}

	return param_set_int(val, kp);
}

static const struct kernel_param_ops param_dgpu_power_ops = {
	.set = param_dgpu_power_set,
	.get = param_get_int,
};

static int param_dgpu_power_init = SHPS_DGPU_MP_POWER_OFF;
static int param_dgpu_power_exit = SHPS_DGPU_MP_POWER_ON;
static int param_dgpu_power_susp = SHPS_DGPU_MP_POWER_ASIS;
static bool param_dtx_latch = true;

module_param_cb(dgpu_power_init, &param_dgpu_power_ops, &param_dgpu_power_init, SHPS_DGPU_PARAM_PERM);
module_param_cb(dgpu_power_exit, &param_dgpu_power_ops, &param_dgpu_power_exit, SHPS_DGPU_PARAM_PERM);
module_param_cb(dgpu_power_susp, &param_dgpu_power_ops, &param_dgpu_power_susp, SHPS_DGPU_PARAM_PERM);
module_param_named(dtx_latch, param_dtx_latch, bool, SHPS_DGPU_PARAM_PERM);

MODULE_PARM_DESC(dgpu_power_init, "dGPU power state to be set on init (0: off / 1: on / 2: as-is, default: off)");
MODULE_PARM_DESC(dgpu_power_exit, "dGPU power state to be set on exit (0: off / 1: on / 2: as-is, default: on)");
MODULE_PARM_DESC(dgpu_power_susp, "dGPU power state to be set on exit (0: off / 1: on / 2: as-is, default: as-is)");
MODULE_PARM_DESC(dtx_latch, "lock/unlock DTX base latch in accordance to power-state (Y/n)");


static int dtx_cmd_simple(u8 cid)
{
	struct surface_sam_ssh_rqst rqst = {
		.tc  = SAM_DTX_TC,
		.cid = cid,
		.iid = 0,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0,
		.cdl = 0,
		.pld = NULL,
	};

	return surface_sam_ssh_rqst(&rqst, NULL);
}

inline static int shps_dtx_latch_lock(void)
{
	return dtx_cmd_simple(SAM_DTX_CID_LATCH_LOCK);
}

inline static int shps_dtx_latch_unlock(void)
{
	return dtx_cmd_simple(SAM_DTX_CID_LATCH_UNLOCK);
}


static int shps_dgpu_dsm_get_pci_addr(struct platform_device *pdev, const char* entry)
{
	acpi_handle handle = ACPI_HANDLE(&pdev->dev);
	union acpi_object *result;
	union acpi_object *e0;
	union acpi_object *e1;
	union acpi_object *e2;
	u64 device_addr = 0;
	u8 bus, dev, fun;
	int i;

	result = acpi_evaluate_dsm_typed(handle, &SHPS_DSM_UUID, SHPS_DSM_REVISION,
					 SHPS_DSM_GPU_ADDRS, NULL, ACPI_TYPE_PACKAGE);

	if (IS_ERR_OR_NULL(result))
		return result ? PTR_ERR(result) : -EIO;

	// three entries per device: name, address, <integer>
	for (i = 0; i + 2 < result->package.count; i += 3) {
		e0 = &result->package.elements[i];
		e1 = &result->package.elements[i + 1];
		e2 = &result->package.elements[i + 2];

		if (e0->type != ACPI_TYPE_STRING) {
			ACPI_FREE(result);
			return -EIO;
		}

		if (e1->type != ACPI_TYPE_INTEGER) {
			ACPI_FREE(result);
			return -EIO;
		}

		if (e2->type != ACPI_TYPE_INTEGER) {
			ACPI_FREE(result);
			return -EIO;
		}

		if (strncmp(e0->string.pointer, entry, 64) == 0)
			device_addr = e1->integer.value;
	}

	ACPI_FREE(result);
	if (device_addr == 0)
		return -ENODEV;

	// convert address
	bus = (device_addr & 0x0FF00000) >> 20;
	dev = (device_addr & 0x000F8000) >> 15;
	fun = (device_addr & 0x00007000) >> 12;

	return bus << 8 | PCI_DEVFN(dev, fun);
}

static struct pci_dev *shps_dgpu_dsm_get_pci_dev(struct platform_device *pdev, const char* entry)
{
	struct pci_dev *dev;
	int addr;

	addr = shps_dgpu_dsm_get_pci_addr(pdev, entry);
	if (addr < 0)
		return ERR_PTR(addr);

	dev = pci_get_domain_bus_and_slot(0, (addr & 0xFF00) >> 8, addr & 0xFF);
	return dev ? dev : ERR_PTR(-ENODEV);
}


static int shps_dgpu_dsm_get_power_unlocked(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct gpio_desc *gpio = drvdata->gpio_dgpu_power;
	int status;

	status = gpiod_get_value_cansleep(gpio);
	if (status < 0)
		return status;

	return status == 0 ? SHPS_DGPU_POWER_OFF : SHPS_DGPU_POWER_ON;
}

static int shps_dgpu_dsm_get_power(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	mutex_lock(&drvdata->lock);
	status = shps_dgpu_dsm_get_power_unlocked(pdev);
	mutex_unlock(&drvdata->lock);

	return status;
}

static int __shps_dgpu_dsm_set_power_unlocked(struct platform_device *pdev, enum shps_dgpu_power power)
{
	acpi_handle handle = ACPI_HANDLE(&pdev->dev);
	union acpi_object *result;
	union acpi_object param;

	dev_info(&pdev->dev, "setting dGPU direct power to \'%s\'\n", shps_dgpu_power_str(power));

	param.type = ACPI_TYPE_INTEGER;
	param.integer.value = power == SHPS_DGPU_POWER_ON;

	result = acpi_evaluate_dsm_typed(handle, &SHPS_DSM_UUID, SHPS_DSM_REVISION,
	                                 SHPS_DSM_GPU_POWER, &param, ACPI_TYPE_BUFFER);

	if (IS_ERR_OR_NULL(result))
		return result ? PTR_ERR(result) : -EIO;

	// check for the expected result
	if (result->buffer.length != 1 || result->buffer.pointer[0] != 0) {
		ACPI_FREE(result);
		return -EIO;
	}

	ACPI_FREE(result);
	return 0;
}

static int shps_dgpu_dsm_set_power_unlocked(struct platform_device *pdev, enum shps_dgpu_power power)
{
	int status;

	if (power != SHPS_DGPU_POWER_ON && power != SHPS_DGPU_POWER_OFF)
		return -EINVAL;

	status = shps_dgpu_dsm_get_power_unlocked(pdev);
	if (status < 0)
		return status;
	if (status == power)
		return 0;

	return __shps_dgpu_dsm_set_power_unlocked(pdev, power);
}

static int shps_dgpu_dsm_set_power(struct platform_device *pdev, enum shps_dgpu_power power)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	mutex_lock(&drvdata->lock);
	status = shps_dgpu_dsm_set_power_unlocked(pdev, power);
	mutex_unlock(&drvdata->lock);

	return status;
}


static bool shps_rp_link_up(struct pci_dev *rp)
{
	u16 lnksta = 0, sltsta = 0;

	pcie_capability_read_word(rp, PCI_EXP_LNKSTA, &lnksta);
	pcie_capability_read_word(rp, PCI_EXP_SLTSTA, &sltsta);

	return (lnksta & PCI_EXP_LNKSTA_DLLLA) || (sltsta & PCI_EXP_SLTSTA_PDS);
}


static int shps_dgpu_rp_get_power_unlocked(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct pci_dev *rp = drvdata->dgpu_root_port;

	if (rp->current_state == PCI_D3hot || rp->current_state == PCI_D3cold)
		return SHPS_DGPU_POWER_OFF;
	else if (rp->current_state == PCI_UNKNOWN || rp->current_state == PCI_POWER_ERROR)
		return SHPS_DGPU_POWER_UNKNOWN;
	else
		return SHPS_DGPU_POWER_ON;
}

static int shps_dgpu_rp_get_power(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	mutex_lock(&drvdata->lock);
	status = shps_dgpu_rp_get_power_unlocked(pdev);
	mutex_unlock(&drvdata->lock);

	return status;
}

static int __shps_dgpu_rp_set_power_unlocked(struct platform_device *pdev, enum shps_dgpu_power power)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct pci_dev *rp = drvdata->dgpu_root_port;
	int status, i;

	dev_info(&pdev->dev, "setting dGPU power state to \'%s\'\n", shps_dgpu_power_str(power));

	dbg_dump_drvsta(pdev, "__shps_dgpu_rp_set_power_unlocked.1");
	if (power == SHPS_DGPU_POWER_ON) {
		set_bit(SHPS_STATE_BIT_RPPWRON_SYNC, &drvdata->state);
		pci_set_power_state(rp, PCI_D0);

		if (drvdata->dgpu_root_port_state)
			pci_load_and_free_saved_state(rp, &drvdata->dgpu_root_port_state);

		pci_restore_state(rp);

		if (!pci_is_enabled(rp))
			pci_enable_device(rp);

		pci_set_master(rp);
		clear_bit(SHPS_STATE_BIT_RPPWRON_SYNC, &drvdata->state);

		set_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);
	} else {
		if (!drvdata->dgpu_root_port_state) {
			pci_save_state(rp);
			drvdata->dgpu_root_port_state = pci_store_saved_state(rp);
		}

		/*
		 * To properly update the hot-plug system we need to "remove" the dGPU
		 * before disabling it and sending it to D3cold. Following this, we
		 * need to wait for the link and slot status to actually change.
		 */
		status = shps_dgpu_dsm_set_power_unlocked(pdev, SHPS_DGPU_POWER_OFF);
		if (status)
			return status;

		for (i = 0; i < 20 && shps_rp_link_up(rp); i++)
			msleep(50);

		if (shps_rp_link_up(rp))
			dev_err(&pdev->dev, "dGPU removal via DSM timed out\n");

		pci_clear_master(rp);

		if (pci_is_enabled(rp))
			pci_disable_device(rp);

		pci_set_power_state(rp, PCI_D3cold);

		clear_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);
	}
	dbg_dump_drvsta(pdev, "__shps_dgpu_rp_set_power_unlocked.2");

	return 0;
}

static int shps_dgpu_rp_set_power_unlocked(struct platform_device *pdev, enum shps_dgpu_power power)
{
	int status;

	if (power != SHPS_DGPU_POWER_ON && power != SHPS_DGPU_POWER_OFF)
		return -EINVAL;

	status = shps_dgpu_rp_get_power_unlocked(pdev);
	if (status < 0)
		return status;
	if (status == power)
		return 0;

	return __shps_dgpu_rp_set_power_unlocked(pdev, power);
}

static int shps_dgpu_rp_set_power(struct platform_device *pdev, enum shps_dgpu_power power)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	mutex_lock(&drvdata->lock);
	status = shps_dgpu_rp_set_power_unlocked(pdev, power);
	mutex_unlock(&drvdata->lock);

	return status;
}


static int shps_dgpu_set_power(struct platform_device *pdev, enum shps_dgpu_power power)
{
	int status;

	if (!param_dtx_latch)
		return shps_dgpu_rp_set_power(pdev, power);

	if (power == SHPS_DGPU_POWER_ON) {
		status = shps_dtx_latch_lock();
		if (status)
			return status;

		status = shps_dgpu_rp_set_power(pdev, power);
		if (status)
			shps_dtx_latch_unlock();

		return status;
	} else {
		status = shps_dgpu_rp_set_power(pdev, power);
		if (status)
			return status;

		return shps_dtx_latch_unlock();
	}
}


static int shps_dgpu_is_present(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	return gpiod_get_value_cansleep(drvdata->gpio_dgpu_presence);
}


static ssize_t dgpu_power_show(struct device *dev, struct device_attribute *attr, char *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	int power = shps_dgpu_rp_get_power(pdev);

	if (power < 0)
		return power;

	return sprintf(data, "%s\n", shps_dgpu_power_str(power));
}

static ssize_t dgpu_power_store(struct device *dev, struct device_attribute *attr,
				const char *data, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	enum shps_dgpu_power power;
	bool b = false;
	int status;

	status = kstrtobool(data, &b);
	if (status)
		return status;

	status = shps_dgpu_is_present(pdev);
	if (status <= 0)
		return status < 0 ? status : -EPERM;

	power = b ? SHPS_DGPU_POWER_ON : SHPS_DGPU_POWER_OFF;
	status = shps_dgpu_set_power(pdev, power);

	return status < 0 ? status : count;
}

static ssize_t dgpu_power_dsm_show(struct device *dev, struct device_attribute *attr, char *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	int power = shps_dgpu_dsm_get_power(pdev);

	if (power < 0)
		return power;

	return sprintf(data, "%s\n", shps_dgpu_power_str(power));
}

static ssize_t dgpu_power_dsm_store(struct device *dev, struct device_attribute *attr,
				    const char *data, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	enum shps_dgpu_power power;
	bool b = false;
	int status;

	status = kstrtobool(data, &b);
	if (status)
		return status;

	status = shps_dgpu_is_present(pdev);
	if (status <= 0)
		return status < 0 ? status : -EPERM;

	power = b ? SHPS_DGPU_POWER_ON : SHPS_DGPU_POWER_OFF;
	status = shps_dgpu_dsm_set_power(pdev, power);

	return status < 0 ? status : count;
}

static DEVICE_ATTR_RW(dgpu_power);
static DEVICE_ATTR_RW(dgpu_power_dsm);

static struct attribute *shps_power_attrs[] = {
	&dev_attr_dgpu_power.attr,
	&dev_attr_dgpu_power_dsm.attr,
	NULL,
};
ATTRIBUTE_GROUPS(shps_power);


static void dbg_dump_power_states(struct platform_device *pdev, const char *prefix)
{
	enum shps_dgpu_power power_dsm;
	enum shps_dgpu_power power_rp;
	int status;

	status = shps_dgpu_rp_get_power_unlocked(pdev);
	if (status < 0)
		dev_err(&pdev->dev, "%s: failed to get root-port power state: %d\n", prefix, status);
	power_rp = status;

	status = shps_dgpu_rp_get_power_unlocked(pdev);
	if (status < 0)
		dev_err(&pdev->dev, "%s: failed to get direct power state: %d\n", prefix, status);
	power_dsm = status;

	dev_dbg(&pdev->dev, "%s: root-port power state: %d\n", prefix, power_rp);
	dev_dbg(&pdev->dev, "%s: direct power state:    %d\n", prefix, power_dsm);
}

static void dbg_dump_pciesta(struct platform_device *pdev, const char *prefix)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct pci_dev *rp = drvdata->dgpu_root_port;
	u16 lnksta, lnksta2, sltsta, sltsta2;

	pcie_capability_read_word(rp, PCI_EXP_LNKSTA, &lnksta);
	pcie_capability_read_word(rp, PCI_EXP_LNKSTA2, &lnksta2);
	pcie_capability_read_word(rp, PCI_EXP_SLTSTA, &sltsta);
	pcie_capability_read_word(rp, PCI_EXP_SLTSTA2, &sltsta2);

	dev_dbg(&pdev->dev, "%s: LNKSTA: 0x%04x", prefix, lnksta);
	dev_dbg(&pdev->dev, "%s: LNKSTA2: 0x%04x", prefix, lnksta2);
	dev_dbg(&pdev->dev, "%s: SLTSTA: 0x%04x", prefix, sltsta);
	dev_dbg(&pdev->dev, "%s: SLTSTA2: 0x%04x", prefix, sltsta2);
}

static void dbg_dump_drvsta(struct platform_device *pdev, const char *prefix)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct pci_dev *rp = drvdata->dgpu_root_port;

	dev_dbg(&pdev->dev, "%s: RP power: %d", prefix, rp->current_state);
	dev_dbg(&pdev->dev, "%s: RP state saved: %d", prefix, rp->state_saved);
	dev_dbg(&pdev->dev, "%s: RP state stored: %d", prefix, !!drvdata->dgpu_root_port_state);
	dev_dbg(&pdev->dev, "%s: RP enabled: %d", prefix, atomic_read(&rp->enable_cnt));
	dev_dbg(&pdev->dev, "%s: RP mastered: %d", prefix, rp->is_busmaster);
}


static int shps_pm_prepare(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	bool pwrtgt;
	int status = 0;

	dbg_dump_power_states(pdev, "shps_pm_prepare");

	if (param_dgpu_power_susp != SHPS_DGPU_MP_POWER_ASIS) {
		pwrtgt = test_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);

		status = shps_dgpu_set_power(pdev, param_dgpu_power_susp);
		if (status) {
			dev_err(&pdev->dev, "failed to power %s dGPU: %d\n",
				param_dgpu_power_susp == SHPS_DGPU_MP_POWER_OFF ? "off" : "on",
				status);
			return status;
		}

		if (pwrtgt)
			set_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);
		else
			clear_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);
	}

	return 0;
}

static void shps_pm_complete(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	dbg_dump_power_states(pdev, "shps_pm_complete");
	dbg_dump_pciesta(pdev, "shps_pm_complete");
	dbg_dump_drvsta(pdev, "shps_pm_complete.1");

	// update power target, dGPU may have been detached while suspended
	status = shps_dgpu_is_present(pdev);
	if (status < 0) {
		dev_err(&pdev->dev, "failed to get dGPU presence: %d\n", status);
		return;
	} else if (status == 0) {
		clear_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);
	}

	/*
	 * During resume, the PCIe core will power on the root-port, which in turn
	 * will power on the dGPU. Most of the state synchronization is already
	 * handled via the SAN RQSG handler, so it is in a fully consistent
	 * on-state here. If requested, turn it off here.
	 *
	 * As there seem to be some synchronization issues turning off the dGPU
	 * directly after the power-on SAN RQSG notification during the resume
	 * process, let's do this here.
	 *
	 * TODO/FIXME:
	 *   This does not combat unhandled power-ons when the device is not fully
	 *   resumed, i.e. re-suspended before shps_pm_complete is called. Those
	 *   should normally not be an issue, but the dGPU does get hot even though
	 *   it is suspended, so ideally we want to keep it off.
	 */
	if (!test_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state)) {
		status = shps_dgpu_set_power(pdev, SHPS_DGPU_POWER_OFF);
		if (status)
			dev_err(&pdev->dev, "failed to power-off dGPU: %d\n", status);
	}

	dbg_dump_drvsta(pdev, "shps_pm_complete.2");
}

static int shps_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	if (device_may_wakeup(dev)) {
		status = enable_irq_wake(drvdata->irq_base_presence);
		if (status)
			return status;

		set_bit(SHPS_STATE_BIT_WAKE_ENABLED, &drvdata->state);
	}

	return 0;
}

static int shps_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status = 0;

	if (test_and_clear_bit(SHPS_STATE_BIT_WAKE_ENABLED, &drvdata->state)) {
		status = disable_irq_wake(drvdata->irq_base_presence);
	}

	return status;
}

static void shps_shutdown(struct platform_device *pdev)
{
	int status;

	/*
	 * Turn on dGPU before shutting down. This allows the core drivers to
	 * properly shut down the device. If we don't do this, the pcieport driver
	 * will complain that the device has already been disabled.
	 */
	status = shps_dgpu_set_power(pdev, SHPS_DGPU_POWER_ON);
	if (status)
		dev_err(&pdev->dev, "failed to turn on dGPU: %d\n", status);
}

static int shps_dgpu_detached(struct platform_device *pdev)
{
	dbg_dump_power_states(pdev, "shps_dgpu_detached");
	return shps_dgpu_set_power(pdev, SHPS_DGPU_POWER_OFF);
}

static int shps_dgpu_attached(struct platform_device *pdev)
{
	dbg_dump_power_states(pdev, "shps_dgpu_attached");
	return 0;
}

static int shps_dgpu_powered_on(struct platform_device *pdev)
{
	/*
	 * This function gets called directly after a power-state transition of
	 * the dGPU root port out of D3cold state, indicating a power-on of the
	 * dGPU. Specifically, this function is called from the RQSG handler of
	 * SAN, invoked by the ACPI _ON method of the dGPU root port. This means
	 * that this function is run inside `pci_set_power_state(rp, ...)`
	 * syncrhonously and thus returns before the `pci_set_power_state` call
	 * does.
	 *
	 * `pci_set_power_state` may either be called by us or when the PCI
	 * subsystem decides to power up the root port (e.g. during resume). Thus
	 * we should use this function to ensure that the dGPU and root port
	 * states are consistent when an unexpected power-up is encountered.
	 */

	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct pci_dev *rp = drvdata->dgpu_root_port;
	int status;

	dbg_dump_drvsta(pdev, "shps_dgpu_powered_on.1");

	// if we caused the root port to power-on, return
	if (test_bit(SHPS_STATE_BIT_RPPWRON_SYNC, &drvdata->state))
		return 0;

	// if dGPU is not present, force power-target to off and return
	status = shps_dgpu_is_present(pdev);
	if (status == 0)
		clear_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state);
	if (status <= 0)
		return status;

	mutex_lock(&drvdata->lock);

	dbg_dump_power_states(pdev, "shps_dgpu_powered_on.1");
	dbg_dump_pciesta(pdev, "shps_dgpu_powered_on.1");
	if (drvdata->dgpu_root_port_state)
		pci_load_and_free_saved_state(rp, &drvdata->dgpu_root_port_state);
	pci_restore_state(rp);
	if (!pci_is_enabled(rp))
		pci_enable_device(rp);
	pci_set_master(rp);
	dbg_dump_drvsta(pdev, "shps_dgpu_powered_on.2");
	dbg_dump_power_states(pdev, "shps_dgpu_powered_on.2");
	dbg_dump_pciesta(pdev, "shps_dgpu_powered_on.2");

	mutex_unlock(&drvdata->lock);

	if (!test_bit(SHPS_STATE_BIT_PWRTGT, &drvdata->state)) {
		dev_warn(&pdev->dev, "unexpected dGPU power-on detected");
		// TODO: schedule state re-check and update
	}

	return 0;
}


static int shps_dgpu_handle_rqsg(struct surface_sam_san_rqsg *rqsg, void *data)
{
	struct platform_device *pdev = data;

	if (rqsg->tc == SAM_DGPU_TC && rqsg->cid == SAM_DGPU_CID_POWERON)
		return shps_dgpu_powered_on(pdev);

	dev_warn(&pdev->dev, "unimplemented dGPU request: RQSG(0x%02x, 0x%02x, 0x%02x)",
		 rqsg->tc, rqsg->cid, rqsg->iid);
	return 0;
}

static irqreturn_t shps_dgpu_presence_irq(int irq, void *data)
{
	struct platform_device *pdev = data;
	bool dgpu_present;
	int status;

	status = shps_dgpu_is_present(pdev);
	if (status < 0) {
		dev_err(&pdev->dev, "failed to check physical dGPU presence: %d\n", status);
		return IRQ_HANDLED;
	}

	dgpu_present = status != 0;
	dev_info(&pdev->dev, "dGPU physically %s\n", dgpu_present ? "attached" : "detached");

	if (dgpu_present)
		status = shps_dgpu_attached(pdev);
	else
		status = shps_dgpu_detached(pdev);

	if (status)
		dev_err(&pdev->dev, "error handling dGPU interrupt: %d\n", status);

	return IRQ_HANDLED;
}

static irqreturn_t shps_base_presence_irq(int irq, void *data)
{
	return IRQ_HANDLED;	// nothing to do, just wake
}


static int shps_gpios_setup(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	struct gpio_desc *gpio_dgpu_power;
	struct gpio_desc *gpio_dgpu_presence;
	struct gpio_desc *gpio_base_presence;
	int status;

	// get GPIOs
	gpio_dgpu_power = devm_gpiod_get(&pdev->dev, "dgpu_power", GPIOD_IN);
	if (IS_ERR(gpio_dgpu_power)) {
		status = PTR_ERR(gpio_dgpu_power);
		goto err_out;
	}

	gpio_dgpu_presence = devm_gpiod_get(&pdev->dev, "dgpu_presence", GPIOD_IN);
	if (IS_ERR(gpio_dgpu_presence)) {
		status = PTR_ERR(gpio_dgpu_presence);
		goto err_out;
	}

	gpio_base_presence = devm_gpiod_get(&pdev->dev, "base_presence", GPIOD_IN);
	if (IS_ERR(gpio_base_presence)) {
		status = PTR_ERR(gpio_base_presence);
		goto err_out;
	}

	// export GPIOs
	status = gpiod_export(gpio_dgpu_power, false);
	if (status)
		goto err_out;

	status = gpiod_export(gpio_dgpu_presence, false);
	if (status)
		goto err_export_dgpu_presence;

	status = gpiod_export(gpio_base_presence, false);
	if (status)
		goto err_export_base_presence;

	// create sysfs links
	status = gpiod_export_link(&pdev->dev, "gpio-dgpu_power", gpio_dgpu_power);
	if (status)
		goto err_link_dgpu_power;

	status = gpiod_export_link(&pdev->dev, "gpio-dgpu_presence", gpio_dgpu_presence);
	if (status)
		goto err_link_dgpu_presence;

	status = gpiod_export_link(&pdev->dev, "gpio-base_presence", gpio_base_presence);
	if (status)
		goto err_link_base_presence;

	drvdata->gpio_dgpu_power = gpio_dgpu_power;
	drvdata->gpio_dgpu_presence = gpio_dgpu_presence;
	drvdata->gpio_base_presence = gpio_base_presence;
	return 0;

err_link_base_presence:
	sysfs_remove_link(&pdev->dev.kobj, "gpio-dgpu_presence");
err_link_dgpu_presence:
	sysfs_remove_link(&pdev->dev.kobj, "gpio-dgpu_power");
err_link_dgpu_power:
	gpiod_unexport(gpio_base_presence);
err_export_base_presence:
	gpiod_unexport(gpio_dgpu_presence);
err_export_dgpu_presence:
	gpiod_unexport(gpio_dgpu_power);
err_out:
	return status;
}

static void shps_gpios_remove(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);

	sysfs_remove_link(&pdev->dev.kobj, "gpio-base_presence");
	sysfs_remove_link(&pdev->dev.kobj, "gpio-dgpu_presence");
	sysfs_remove_link(&pdev->dev.kobj, "gpio-dgpu_power");
	gpiod_unexport(drvdata->gpio_base_presence);
	gpiod_unexport(drvdata->gpio_dgpu_presence);
	gpiod_unexport(drvdata->gpio_dgpu_power);
}

static int shps_gpios_setup_irq(struct platform_device *pdev)
{
	const int irqf_dgpu = IRQF_SHARED | IRQF_ONESHOT | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
	const int irqf_base = IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	status = gpiod_to_irq(drvdata->gpio_base_presence);
	if (status < 0)
		return status;
	drvdata->irq_base_presence = status;

	status = gpiod_to_irq(drvdata->gpio_dgpu_presence);
	if (status < 0)
		return status;
	drvdata->irq_dgpu_presence = status;

	status = request_irq(drvdata->irq_base_presence,
			     shps_base_presence_irq, irqf_base,
			     "shps_base_presence_irq", pdev);
	if (status)
		return status;

	status = request_threaded_irq(drvdata->irq_dgpu_presence,
				      NULL, shps_dgpu_presence_irq, irqf_dgpu,
				      "shps_dgpu_presence_irq", pdev);
	if (status) {
		free_irq(drvdata->irq_base_presence, pdev);
		return status;
	}

	return 0;
}

static void shps_gpios_remove_irq(struct platform_device *pdev)
{
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);

	free_irq(drvdata->irq_base_presence, pdev);
	free_irq(drvdata->irq_dgpu_presence, pdev);
}

static int shps_probe(struct platform_device *pdev)
{
	struct acpi_device *shps_dev = ACPI_COMPANION(&pdev->dev);
	struct shps_driver_data *drvdata;
	struct device_link *link;
	int power, status;

	if (gpiod_count(&pdev->dev, NULL) < 0)
		return -ENODEV;

	// link to SSH
	status = surface_sam_ssh_consumer_register(&pdev->dev);
	if (status) {
		return status == -ENXIO ? -EPROBE_DEFER : status;
	}

	// link to SAN
	status = surface_sam_san_consumer_register(&pdev->dev, 0);
	if (status) {
		return status == -ENXIO ? -EPROBE_DEFER : status;
	}

	status = acpi_dev_add_driver_gpios(shps_dev, shps_acpi_gpios);
	if (status)
		return status;

	drvdata = kzalloc(sizeof(struct shps_driver_data), GFP_KERNEL);
	if (!drvdata) {
		status = -ENOMEM;
		goto err_drvdata;
	}
	mutex_init(&drvdata->lock);
	platform_set_drvdata(pdev, drvdata);

	drvdata->dgpu_root_port = shps_dgpu_dsm_get_pci_dev(pdev, SHPS_DSM_GPU_ADDRS_RP);
	if (IS_ERR(drvdata->dgpu_root_port)) {
		status = PTR_ERR(drvdata->dgpu_root_port);
		goto err_rp_lookup;
	}

	status = shps_gpios_setup(pdev);
	if (status)
		goto err_gpio;

	status = shps_gpios_setup_irq(pdev);
	if (status)
		goto err_gpio_irqs;

	status = device_add_groups(&pdev->dev, shps_power_groups);
	if (status)
		goto err_devattr;

	link = device_link_add(&pdev->dev, &drvdata->dgpu_root_port->dev,
			       DL_FLAG_PM_RUNTIME | DL_FLAG_AUTOREMOVE_CONSUMER);
	if (!link)
		goto err_devlink;

	surface_sam_san_set_rqsg_handler(shps_dgpu_handle_rqsg, pdev);

	// if dGPU is not present turn-off root-port, else obey module param
	status = shps_dgpu_is_present(pdev);
	if (status < 0)
		goto err_devlink;

	power = status == 0 ? SHPS_DGPU_POWER_OFF : param_dgpu_power_init;
	if (power != SHPS_DGPU_MP_POWER_ASIS) {
		status = shps_dgpu_set_power(pdev, power);
		if (status)
			goto err_devlink;
	}

	device_init_wakeup(&pdev->dev, true);
	return 0;

err_devlink:
	device_remove_groups(&pdev->dev, shps_power_groups);
err_devattr:
	shps_gpios_remove_irq(pdev);
err_gpio_irqs:
	shps_gpios_remove(pdev);
err_gpio:
	pci_dev_put(drvdata->dgpu_root_port);
err_rp_lookup:
	platform_set_drvdata(pdev, NULL);
	kfree(drvdata);
err_drvdata:
	acpi_dev_remove_driver_gpios(shps_dev);
	return status;
}

static int shps_remove(struct platform_device *pdev)
{
	struct acpi_device *shps_dev = ACPI_COMPANION(&pdev->dev);
	struct shps_driver_data *drvdata = platform_get_drvdata(pdev);
	int status;

	if (param_dgpu_power_exit != SHPS_DGPU_MP_POWER_ASIS) {
		status = shps_dgpu_set_power(pdev, param_dgpu_power_exit);
		if (status)
			dev_err(&pdev->dev, "failed to set dGPU power state: %d\n", status);
	}

	device_set_wakeup_capable(&pdev->dev, false);
	surface_sam_san_set_rqsg_handler(NULL, NULL);
	device_remove_groups(&pdev->dev, shps_power_groups);
	shps_gpios_remove_irq(pdev);
	shps_gpios_remove(pdev);
	pci_dev_put(drvdata->dgpu_root_port);
	platform_set_drvdata(pdev, NULL);
	kfree(drvdata);

	acpi_dev_remove_driver_gpios(shps_dev);
	return 0;
}


static const struct dev_pm_ops shps_pm_ops = {
	.prepare = shps_pm_prepare,
	.complete = shps_pm_complete,
	.suspend = shps_pm_suspend,
	.resume = shps_pm_resume,
};

static const struct acpi_device_id shps_acpi_match[] = {
	{ "MSHW0153", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, shps_acpi_match);

struct platform_driver surface_sam_hps = {
	.probe = shps_probe,
	.remove = shps_remove,
	.shutdown = shps_shutdown,
	.driver = {
		.name = "surface_dgpu_hps",
		.acpi_match_table = ACPI_PTR(shps_acpi_match),
		.pm = &shps_pm_ops,
	},
};
module_platform_driver(surface_sam_hps);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Surface Hot-Plug System (HPS) and dGPU power-state Driver for Surface Book 2");
MODULE_LICENSE("GPL v2");
