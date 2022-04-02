#ifndef AAUDIO_PROTOCOL_H
#define AAUDIO_PROTOCOL_H

#include <linux/types.h>

struct aaudio_device;

typedef u64 aaudio_device_id_t;
typedef u64 aaudio_object_id_t;

struct aaudio_msg {
    void *data;
    size_t size;
};

struct __attribute__((packed)) aaudio_msg_header {
    char tag[4];
    u8 type;
    aaudio_device_id_t device_id; // Idk, use zero for commands?
};
struct __attribute__((packed)) aaudio_msg_base {
    u32 msg;
    u32 status;
};

struct aaudio_prop_addr {
    u32 scope;
    u32 selector;
    u32 element;
};
#define AAUDIO_PROP(scope, sel, el) (struct aaudio_prop_addr) { scope, sel, el }

enum {
    AAUDIO_MSG_TYPE_COMMAND = 1,
    AAUDIO_MSG_TYPE_RESPONSE = 2,
    AAUDIO_MSG_TYPE_NOTIFICATION = 3
};

enum {
    AAUDIO_MSG_START_IO = 0,
    AAUDIO_MSG_START_IO_RESPONSE = 1,
    AAUDIO_MSG_STOP_IO = 2,
    AAUDIO_MSG_STOP_IO_RESPONSE = 3,
    AAUDIO_MSG_UPDATE_TIMESTAMP = 4,
    AAUDIO_MSG_GET_PROPERTY = 7,
    AAUDIO_MSG_GET_PROPERTY_RESPONSE = 8,
    AAUDIO_MSG_SET_PROPERTY = 9,
    AAUDIO_MSG_SET_PROPERTY_RESPONSE = 10,
    AAUDIO_MSG_PROPERTY_LISTENER = 11,
    AAUDIO_MSG_PROPERTY_LISTENER_RESPONSE = 12,
    AAUDIO_MSG_PROPERTY_CHANGED = 13,
    AAUDIO_MSG_SET_INPUT_STREAM_ADDRESS_RANGES = 18,
    AAUDIO_MSG_SET_INPUT_STREAM_ADDRESS_RANGES_RESPONSE = 19,
    AAUDIO_MSG_GET_INPUT_STREAM_LIST = 24,
    AAUDIO_MSG_GET_INPUT_STREAM_LIST_RESPONSE = 25,
    AAUDIO_MSG_GET_OUTPUT_STREAM_LIST = 26,
    AAUDIO_MSG_GET_OUTPUT_STREAM_LIST_RESPONSE = 27,
    AAUDIO_MSG_SET_REMOTE_ACCESS = 32,
    AAUDIO_MSG_SET_REMOTE_ACCESS_RESPONSE = 33,
    AAUDIO_MSG_UPDATE_TIMESTAMP_RESPONSE = 34,

    AAUDIO_MSG_NOTIFICATION_ALIVE = 100,
    AAUDIO_MSG_GET_DEVICE_LIST = 101,
    AAUDIO_MSG_GET_DEVICE_LIST_RESPONSE = 102,
    AAUDIO_MSG_NOTIFICATION_BOOT = 104
};

enum {
    AAUDIO_REMOTE_ACCESS_OFF = 0,
    AAUDIO_REMOTE_ACCESS_ON = 2
};

enum {
    AAUDIO_PROP_SCOPE_GLOBAL = 0x676c6f62, // 'glob'
    AAUDIO_PROP_SCOPE_INPUT  = 0x696e7074, // 'inpt'
    AAUDIO_PROP_SCOPE_OUTPUT = 0x6f757470  // 'outp'
};

enum {
    AAUDIO_PROP_UID          = 0x75696420, // 'uid '
    AAUDIO_PROP_BOOL_VALUE   = 0x6263766c, // 'bcvl'
    AAUDIO_PROP_JACK_PLUGGED = 0x6a61636b, // 'jack'
    AAUDIO_PROP_SEL_VOLUME   = 0x64656176, // 'deav'
    AAUDIO_PROP_LATENCY      = 0x6c746e63, // 'ltnc'
    AAUDIO_PROP_PHYS_FORMAT  = 0x70667420  // 'pft '
};

int aaudio_msg_read_base(struct aaudio_msg *msg, struct aaudio_msg_base *base);

int aaudio_msg_read_start_io_response(struct aaudio_msg *msg);
int aaudio_msg_read_stop_io_response(struct aaudio_msg *msg);
int aaudio_msg_read_update_timestamp(struct aaudio_msg *msg, aaudio_device_id_t *devid,
        u64 *timestamp, u64 *update_seed);
int aaudio_msg_read_get_property_response(struct aaudio_msg *msg, aaudio_object_id_t *obj,
        struct aaudio_prop_addr *prop, void **data, u64 *data_size);
int aaudio_msg_read_set_property_response(struct aaudio_msg *msg, aaudio_object_id_t *obj);
int aaudio_msg_read_property_listener_response(struct aaudio_msg *msg,aaudio_object_id_t *obj,
        struct aaudio_prop_addr *prop);
int aaudio_msg_read_property_changed(struct aaudio_msg *msg, aaudio_device_id_t *devid, aaudio_object_id_t *obj,
        struct aaudio_prop_addr *prop);
int aaudio_msg_read_set_input_stream_address_ranges_response(struct aaudio_msg *msg);
int aaudio_msg_read_get_input_stream_list_response(struct aaudio_msg *msg, aaudio_object_id_t **str_l, u64 *str_cnt);
int aaudio_msg_read_get_output_stream_list_response(struct aaudio_msg *msg, aaudio_object_id_t **str_l, u64 *str_cnt);
int aaudio_msg_read_set_remote_access_response(struct aaudio_msg *msg);
int aaudio_msg_read_get_device_list_response(struct aaudio_msg *msg, aaudio_device_id_t **dev_l, u64 *dev_cnt);

void aaudio_msg_write_start_io(struct aaudio_msg *msg, aaudio_device_id_t dev);
void aaudio_msg_write_stop_io(struct aaudio_msg *msg, aaudio_device_id_t dev);
void aaudio_msg_write_get_property(struct aaudio_msg *msg, aaudio_device_id_t dev, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop, void *qualifier, u64 qualifier_size);
void aaudio_msg_write_set_property(struct aaudio_msg *msg, aaudio_device_id_t dev, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop, void *data, u64 data_size, void *qualifier, u64 qualifier_size);
void aaudio_msg_write_property_listener(struct aaudio_msg *msg, aaudio_device_id_t dev, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop);
void aaudio_msg_write_set_input_stream_address_ranges(struct aaudio_msg *msg, aaudio_device_id_t devid);
void aaudio_msg_write_get_input_stream_list(struct aaudio_msg *msg, aaudio_device_id_t devid);
void aaudio_msg_write_get_output_stream_list(struct aaudio_msg *msg, aaudio_device_id_t devid);
void aaudio_msg_write_set_remote_access(struct aaudio_msg *msg, u64 mode);
void aaudio_msg_write_alive_notification(struct aaudio_msg *msg, u32 proto_ver, u32 msg_ver);
void aaudio_msg_write_update_timestamp_response(struct aaudio_msg *msg);
void aaudio_msg_write_get_device_list(struct aaudio_msg *msg);


int aaudio_cmd_start_io(struct aaudio_device *a, aaudio_device_id_t devid);
int aaudio_cmd_stop_io(struct aaudio_device *a, aaudio_device_id_t devid);
int aaudio_cmd_get_property(struct aaudio_device *a, struct aaudio_msg *buf,
        aaudio_device_id_t devid, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop, void *qualifier, u64 qualifier_size, void **data, u64 *data_size);
int aaudio_cmd_get_primitive_property(struct aaudio_device *a,
        aaudio_device_id_t devid, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop, void *qualifier, u64 qualifier_size, void *data, u64 data_size);
int aaudio_cmd_set_property(struct aaudio_device *a, aaudio_device_id_t devid, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop, void *qualifier, u64 qualifier_size, void *data, u64 data_size);
int aaudio_cmd_property_listener(struct aaudio_device *a, aaudio_device_id_t devid, aaudio_object_id_t obj,
        struct aaudio_prop_addr prop);
int aaudio_cmd_set_input_stream_address_ranges(struct aaudio_device *a, aaudio_device_id_t devid);
int aaudio_cmd_get_input_stream_list(struct aaudio_device *a, struct aaudio_msg *buf, aaudio_device_id_t devid,
        aaudio_object_id_t **str_l, u64 *str_cnt);
int aaudio_cmd_get_output_stream_list(struct aaudio_device *a, struct aaudio_msg *buf, aaudio_device_id_t devid,
        aaudio_object_id_t **str_l, u64 *str_cnt);
int aaudio_cmd_set_remote_access(struct aaudio_device *a, u64 mode);
int aaudio_cmd_get_device_list(struct aaudio_device *a, struct aaudio_msg *buf,
        aaudio_device_id_t **dev_l, u64 *dev_cnt);



#endif //AAUDIO_PROTOCOL_H
