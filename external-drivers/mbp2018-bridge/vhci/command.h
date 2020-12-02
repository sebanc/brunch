#ifndef BCE_VHCI_COMMAND_H
#define BCE_VHCI_COMMAND_H

#include "queue.h"
#include <linux/jiffies.h>
#include <linux/usb.h>

#define BCE_VHCI_CMD_TIMEOUT_SHORT msecs_to_jiffies(2000)
#define BCE_VHCI_CMD_TIMEOUT_LONG msecs_to_jiffies(30000)

#define BCE_VHCI_BULK_MAX_ACTIVE_URBS_POW2 2
#define BCE_VHCI_BULK_MAX_ACTIVE_URBS (1 << BCE_VHCI_BULK_MAX_ACTIVE_URBS_POW2)

typedef u8 bce_vhci_port_t;
typedef u8 bce_vhci_device_t;

enum bce_vhci_command {
    BCE_VHCI_CMD_CONTROLLER_ENABLE = 1,
    BCE_VHCI_CMD_CONTROLLER_DISABLE = 2,
    BCE_VHCI_CMD_CONTROLLER_START = 3,
    BCE_VHCI_CMD_CONTROLLER_PAUSE = 4,

    BCE_VHCI_CMD_PORT_POWER_ON = 0x10,
    BCE_VHCI_CMD_PORT_POWER_OFF = 0x11,
    BCE_VHCI_CMD_PORT_RESUME = 0x12,
    BCE_VHCI_CMD_PORT_SUSPEND = 0x13,
    BCE_VHCI_CMD_PORT_RESET = 0x14,
    BCE_VHCI_CMD_PORT_DISABLE = 0x15,
    BCE_VHCI_CMD_PORT_STATUS = 0x16,

    BCE_VHCI_CMD_DEVICE_CREATE = 0x30,
    BCE_VHCI_CMD_DEVICE_DESTROY = 0x31,

    BCE_VHCI_CMD_ENDPOINT_CREATE = 0x40,
    BCE_VHCI_CMD_ENDPOINT_DESTROY = 0x41,
    BCE_VHCI_CMD_ENDPOINT_SET_STATE = 0x42,
    BCE_VHCI_CMD_ENDPOINT_RESET = 0x44,

    /* Device to host only */
    BCE_VHCI_CMD_ENDPOINT_REQUEST_STATE = 0x43,
    BCE_VHCI_CMD_TRANSFER_REQUEST = 0x1000,
    BCE_VHCI_CMD_CONTROL_TRANSFER_STATUS = 0x1005
};

enum bce_vhci_endpoint_state {
    BCE_VHCI_ENDPOINT_ACTIVE = 0,
    BCE_VHCI_ENDPOINT_PAUSED = 1,
    BCE_VHCI_ENDPOINT_STALLED = 2
};

static inline int bce_vhci_cmd_controller_enable(struct bce_vhci_command_queue *q, u8 busNum, u16 *portMask)
{
    int status;
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_CONTROLLER_ENABLE;
    cmd.param1 = 0x7100u | busNum;
    status = bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
    if (!status)
        *portMask = (u16) res.param2;
    return status;
}
static inline int bce_vhci_cmd_controller_disable(struct bce_vhci_command_queue *q)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_CONTROLLER_DISABLE;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
}
static inline int bce_vhci_cmd_controller_start(struct bce_vhci_command_queue *q)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_CONTROLLER_START;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
}
static inline int bce_vhci_cmd_controller_pause(struct bce_vhci_command_queue *q)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_CONTROLLER_PAUSE;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
}

static inline int bce_vhci_cmd_port_power_on(struct bce_vhci_command_queue *q, bce_vhci_port_t port)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_POWER_ON;
    cmd.param1 = port;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}
static inline int bce_vhci_cmd_port_power_off(struct bce_vhci_command_queue *q, bce_vhci_port_t port)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_POWER_OFF;
    cmd.param1 = port;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}
static inline int bce_vhci_cmd_port_resume(struct bce_vhci_command_queue *q, bce_vhci_port_t port)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_RESUME;
    cmd.param1 = port;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
}
static inline int bce_vhci_cmd_port_suspend(struct bce_vhci_command_queue *q, bce_vhci_port_t port)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_SUSPEND;
    cmd.param1 = port;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
}
static inline int bce_vhci_cmd_port_reset(struct bce_vhci_command_queue *q, bce_vhci_port_t port, u32 timeout)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_RESET;
    cmd.param1 = port;
    cmd.param2 = timeout;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}
static inline int bce_vhci_cmd_port_disable(struct bce_vhci_command_queue *q, bce_vhci_port_t port)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_DISABLE;
    cmd.param1 = port;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}
static inline int bce_vhci_cmd_port_status(struct bce_vhci_command_queue *q, bce_vhci_port_t port,
        u32 clearFlags, u32 *resStatus)
{
    int status;
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_PORT_STATUS;
    cmd.param1 = port;
    cmd.param2 = clearFlags & 0x560000;
    status = bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
    if (status >= 0)
        *resStatus = (u32) res.param2;
    return status;
}

static inline int bce_vhci_cmd_device_create(struct bce_vhci_command_queue *q, bce_vhci_port_t port,
        bce_vhci_device_t *dev)
{
    int status;
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_DEVICE_CREATE;
    cmd.param1 = port;
    status = bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
    if (!status)
        *dev = (bce_vhci_device_t) res.param2;
    return status;
}
static inline int bce_vhci_cmd_device_destroy(struct bce_vhci_command_queue *q, bce_vhci_device_t dev)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_DEVICE_DESTROY;
    cmd.param1 = dev;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_LONG);
}

static inline int bce_vhci_cmd_endpoint_create(struct bce_vhci_command_queue *q, bce_vhci_device_t dev,
        struct usb_endpoint_descriptor *desc)
{
    struct bce_vhci_message cmd, res;
    int endpoint_type = usb_endpoint_type(desc);
    int maxp = usb_endpoint_maxp(desc);
    int maxp_burst = usb_endpoint_maxp_mult(desc) * maxp;
    u8 max_active_requests_pow2 = 0;
    cmd.cmd = BCE_VHCI_CMD_ENDPOINT_CREATE;
    cmd.param1 = dev | ((desc->bEndpointAddress & 0x8Fu) << 8);
    if (endpoint_type == USB_ENDPOINT_XFER_BULK)
        max_active_requests_pow2 = BCE_VHCI_BULK_MAX_ACTIVE_URBS_POW2;
    cmd.param2 = endpoint_type | ((max_active_requests_pow2 & 0xf) << 4) | (maxp << 16) | ((u64) maxp_burst << 32);
    if (endpoint_type == USB_ENDPOINT_XFER_INT)
        cmd.param2 |= (desc->bInterval - 1) << 8;
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}
static inline int bce_vhci_cmd_endpoint_destroy(struct bce_vhci_command_queue *q, bce_vhci_device_t dev, u8 endpoint)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_ENDPOINT_DESTROY;
    cmd.param1 = dev | (endpoint << 8);
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}
static inline int bce_vhci_cmd_endpoint_set_state(struct bce_vhci_command_queue *q, bce_vhci_device_t dev, u8 endpoint,
        enum bce_vhci_endpoint_state newState, enum bce_vhci_endpoint_state *retState)
{
    int status;
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_ENDPOINT_SET_STATE;
    cmd.param1 = dev | (endpoint << 8);
    cmd.param2 = (u64) newState;
    status = bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
    if (status != BCE_VHCI_INTERNAL_ERROR && status != BCE_VHCI_NO_POWER)
        *retState = (enum bce_vhci_endpoint_state) res.param2;
    return status;
}
static inline int bce_vhci_cmd_endpoint_reset(struct bce_vhci_command_queue *q, bce_vhci_device_t dev, u8 endpoint)
{
    struct bce_vhci_message cmd, res;
    cmd.cmd = BCE_VHCI_CMD_ENDPOINT_RESET;
    cmd.param1 = dev | (endpoint << 8);
    return bce_vhci_command_queue_execute(q, &cmd, &res, BCE_VHCI_CMD_TIMEOUT_SHORT);
}


#endif //BCE_VHCI_COMMAND_H
