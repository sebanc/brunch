#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/platform_device.h>

// Unknown method
// \_SB_.WMIS._WDG (1 of 3)
//   GUID: 77E614ED-F19E-46D6-A613-A8669FEE1FF0
//   WMI Method:
//     Flags          : 0x02 (Method)
//     Object ID      : SR
//     Instance       : 0x01
// PASSED: Test 1, 77E614ED-F19E-46D6-A613-A8669FEE1FF0 has associated method \_SB_.WMIS.WMSR


// There are 7 different methods in LENOVO_GSENSOR_DATA, but all return the same data as tested on a C940-14IIL with BIOS version AUCN45WW

#define YOGA_MODE_CHANGE_EVENT_GUID "06129D99-6083-4164-81AD-F092F9D773A6"
#define LENOVO_GSENSOR_DATA_GUID "09B0EE6E-C3FD-4243-8DA1-7911FF80BB8C"
#define UNKNOWN_EVENT_GUID "95D1DF76-D6C0-4E16-9193-7B2A849F3DF2"

#define GET_USAGE_MODE_ID 1

#define USAGE_MODE_LAPTOP 1
#define USAGE_MODE_TABLET_FLAT 2
#define USAGE_MODE_TABLET_STAND 3
#define USAGE_MODE_TABLET_TENT 4

static struct input_dev *wmi_input_dev = NULL;
static struct platform_device *wmi_input_pdev = NULL;
static int last_mode = 0;

static void yoga_mode_exit(void);

static int eval_gsensor_data_method(u32 method_id)
{
    acpi_status status;
    int result = -1;
    struct acpi_buffer input = {0, NULL};
    struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
    union acpi_object *obj;

    status = wmi_evaluate_method(LENOVO_GSENSOR_DATA_GUID, 0, method_id, &input, &output);

    if (ACPI_FAILURE(status)) {
        pr_err("LENOVO_GSENSOR_DATA ACPI failure\n");
        return result;
    }

    obj = (union acpi_object*) output.pointer;

    if (obj && obj->type == ACPI_TYPE_INTEGER) {
        result = obj->integer.value;
    } else {
        pr_err("LENOVO_GSENSOR_DATA method %d returned invalid result\n", method_id);
    }

    kfree(obj);
    return result;
}

static int get_usage_mode(void)
{
    return eval_gsensor_data_method(GET_USAGE_MODE_ID);
}

static const char* get_usage_mode_name(int mode)
{
    static const char* usage_mode_names[] = {"laptop", "tablet flat", "tablet stand", "tablet tent"};
    static const int num_usage_modes = sizeof(usage_mode_names) / sizeof(*usage_mode_names);
    if (mode < 0) {
        return "INVALID";
    }
    if (mode > num_usage_modes) {
        return "UNKNOWN";
    }
    return usage_mode_names[mode - 1];
}

static int get_and_report_usage_mode(void)
{
    int mode;
    mode = get_usage_mode();
    if (mode < 0) {
        pr_err("get_usage_mode() failed\n");
        return 0;
    }

    if (mode != last_mode) {
        if (last_mode != 0) {
            pr_debug("Yoga usage mode changed to %s\n", get_usage_mode_name(mode));
        }
        input_report_switch(wmi_input_dev, SW_TABLET_MODE, mode != USAGE_MODE_LAPTOP);
        input_sync(wmi_input_dev);
        last_mode = mode;
    }
    return mode;
}

static int setup_input_dev(void)
{
    int err;

    wmi_input_dev = input_allocate_device();
    if (! wmi_input_dev) {
        pr_err("Failed to allocate input device\n");
        return -ENOMEM;
    }

    wmi_input_pdev = platform_device_register_simple("yoga", -1, NULL, 0);

    wmi_input_dev->name = "Yoga tablet switch input";
    wmi_input_dev->phys = "yoga/input0";
    wmi_input_dev->id.bustype = BUS_HOST;
    wmi_input_dev->dev.parent = &wmi_input_pdev->dev;

    input_set_capability(wmi_input_dev, EV_SW, SW_TABLET_MODE);

    err = input_register_device(wmi_input_dev);
    if (err) {
        pr_err("Failed to register input device\n");
        input_free_device(wmi_input_dev);
        return err;
    }

    return 0;
}

static void wmi_notify(u32 value, void *context)
{
    pr_debug("Yoga Mode Change event received\n");
    get_and_report_usage_mode();
}

static int setup_change_event_handler(void)
{
    int status;

    status = wmi_install_notify_handler(YOGA_MODE_CHANGE_EVENT_GUID, wmi_notify, NULL);
    if (ACPI_FAILURE(status)) {
        return -ENODEV;
    }
    return 0;
}

static int __init yoga_mode_init(void)
{
    int err, mode;

    if (! wmi_has_guid(LENOVO_GSENSOR_DATA_GUID) || ! wmi_has_guid(YOGA_MODE_CHANGE_EVENT_GUID)) {
        return -ENODEV;
    }

    err = setup_input_dev();
    if (err) {
        return err;
    }

    mode = get_and_report_usage_mode();
    pr_info("Yoga tablet mode switch found, currently in %s mode\n",
            get_usage_mode_name(mode));

    err = setup_change_event_handler();
    if (err) {
        pr_err("Failed to install yoga mode change event handler\n");
        yoga_mode_exit();
        return -ENODEV;
    }

    return 0;
}

static void yoga_mode_exit(void)
{
    wmi_remove_notify_handler(YOGA_MODE_CHANGE_EVENT_GUID);
    if (wmi_input_dev) {
        input_unregister_device(wmi_input_dev);
    }
    if (wmi_input_pdev) {
        platform_device_unregister(wmi_input_pdev);
    }
}

module_init(yoga_mode_init);
module_exit(yoga_mode_exit);

MODULE_ALIAS("wmi:" YOGA_MODE_CHANGE_EVENT_GUID);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukas Wohlschläger <llukaswhl@gmail.com>");
