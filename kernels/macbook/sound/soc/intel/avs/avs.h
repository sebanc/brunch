/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Authors: Cezary Rojewski <cezary.rojewski@intel.com>
 *          Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
 */

#ifndef __SOUND_SOC_INTEL_AVS_H
#define __SOUND_SOC_INTEL_AVS_H

#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/kfifo.h>
#include <sound/hda_codec.h>
#include <sound/hda_register.h>
#include <sound/soc-component.h>
#include "messages.h"
#include "registers.h"

struct avs_dev;
struct avs_tplg;
struct avs_tplg_library;
struct avs_soc_component;
struct avs_ipc_msg;

struct avs_dsp_ops {
	int (* const power)(struct avs_dev *, u32, bool);
	int (* const reset)(struct avs_dev *, u32, bool);
	int (* const stall)(struct avs_dev *, u32, bool);
	irqreturn_t (* const irq_handler)(struct avs_dev *);
	irqreturn_t (* const irq_thread)(struct avs_dev *);
	void (* const int_control)(struct avs_dev *, bool);
	int (* const load_fw)(struct avs_dev *, struct firmware *);
	int (* const load_lib)(struct avs_dev *, struct firmware *, u32);
	int (* const transfer_mods)(struct avs_dev *, bool,
				    struct avs_module_entry *, u32);
	int (* const enable_logs)(struct avs_dev *, enum avs_log_enable,
				  u32, u32, unsigned long, u32 *);
	unsigned int (* const log_buffer_offset)(struct avs_dev *, u32);
	int (* const log_buffer_status)(struct avs_dev *, union avs_notify_msg);
	int (* const coredump)(struct avs_dev *, union avs_notify_msg);
	bool (* const d0ix_toggle)(struct avs_dev *, struct avs_ipc_msg *, bool);
	int (* const set_d0ix)(struct avs_dev *, bool);
};

#define avs_dsp_op(adev, op, ...) \
	((adev)->spec->dops->op(adev, ## __VA_ARGS__))

extern const struct avs_dsp_ops skl_dsp_ops;
extern const struct avs_dsp_ops apl_dsp_ops;

#define AVS_PLATATTR_CLDMA		BIT_ULL(0)
#define AVS_PLATATTR_IMR		BIT_ULL(1)

#define avs_platattr_test(adev, attr) \
	((adev)->spec->attributes & AVS_PLATATTR_##attr)

struct avs_spec {
	const char *name;

	const struct avs_dsp_ops *const dops;
	struct avs_fw_version min_fw_version;

	const u32 master_mask;
	const u32 init_mask;
	const u64 attributes;
	const u32 sram_base_offset;
	const u32 sram_window_size;

	const u32 rom_status;
	const u32 hipc_req;
	const u32 hipc_req_ext;
	const u32 hipc_req_busy;
	const u32 hipc_ack;
	const u32 hipc_ack_done;
	const u32 hipc_rsp;
	const u32 hipc_rsp_busy;
	const u32 hipc_ctl;
};

struct avs_fw_entry {
	char *name;
	const struct firmware *fw;

	struct list_head node;
};

struct avs_debug {
	struct kfifo trace_fifo;
	spinlock_t fifo_lock;
	spinlock_t trace_lock;
	wait_queue_head_t trace_waitq;
	u32 aging_timer_period;
	u32 fifo_full_timer_period;
	u32 logged_res;
};

struct avs_dev {
	struct hda_bus bus;
	struct device *dev;

	void __iomem *adsp_ba;
	const struct avs_spec *spec;
	struct avs_ipc *ipc;

	struct avs_fw_cfg fw_cfg;
	struct avs_hw_cfg hw_cfg;
	struct avs_mods_info *mods_info;
	struct ida **mod_idas;
	struct mutex modres_mutex;
	struct ida ppl_ida;
	struct list_head fw_list;
	struct list_head notify_sub_list;
	atomic_t *core_refs;
	char **lib_names;

	struct completion fw_ready;
	struct work_struct probe_work;
	struct work_struct recovery_work;

	struct acpi_table_nhlt *nhlt;
	struct list_head comp_list;
	spinlock_t comp_list_lock;
	struct list_head path_list;
	spinlock_t path_list_lock;
	struct mutex path_mutex;

	struct avs_debug dbg;
};

/* from hda_bus to avs_dev */
#define hda_to_avs(hda) container_of(hda, struct avs_dev, bus)
/* from hdac_bus to avs_dev */
#define hdac_to_avs(hdac) hda_to_avs(to_hda_bus(hdac))
/* from device to avs_dev */
#define to_avs_dev(dev) \
({ \
	struct hdac_bus *__bus = dev_get_drvdata(dev); \
	hdac_to_avs(__bus); \
})

int avs_dsp_core_power(struct avs_dev *adev, u32 core_mask, bool active);
int avs_dsp_core_reset(struct avs_dev *adev, u32 core_mask, bool reset);
int avs_dsp_core_stall(struct avs_dev *adev, u32 core_mask, bool stall);
int avs_dsp_core_enable(struct avs_dev *adev, u32 core_mask);
int avs_dsp_core_disable(struct avs_dev *adev, u32 core_mask);
int avs_dsp_enable(struct avs_dev *adev, u32 core_mask);
int avs_dsp_disable(struct avs_dev *adev, u32 core_mask);
int avs_dsp_get_core(struct avs_dev *adev, u32 core_id);
int avs_dsp_put_core(struct avs_dev *adev, u32 core_id);

/* Inter Process Communication */

struct avs_ipc_msg {
	union {
		u64 header;
		union avs_global_msg glb;
		union avs_reply_msg rsp;
	};
	void *data;
	size_t size;
};

struct avs_ipc {
	struct device *dev;

	struct avs_ipc_msg rx;
	u32 default_timeout;
	bool ready;

	bool completed;
	spinlock_t lock;
	struct mutex mutex;
	struct completion done_completion;
	struct completion busy_completion;

	struct delayed_work d0ix_work;
	atomic_t d0ix_disable_depth;
	bool in_d0ix;
};

/*
 * IPC handlers may return positive values which denote successful
 * HOST <-> DSP communication yet failure to process specific request.
 * Use below macro to convert returned non-zero values appropriately.
 */
#define AVS_EIPC	EREMOTEIO
#define AVS_IPC_RET(ret) \
	(((ret) <= 0) ? (ret) : -AVS_EIPC)

static inline void avs_ipc_err(struct avs_dev *adev, struct avs_ipc_msg *tx,
			       const char *name, int error)
{
	if (error == -EPERM)
		dev_dbg(adev->dev, "%s 0x%08x 0x%08x failed: %d\n", name,
			tx->glb.primary, tx->glb.ext.val, error);
	else
		dev_err(adev->dev, "%s 0x%08x 0x%08x failed: %d\n", name,
			tx->glb.primary, tx->glb.ext.val, error);
}

irqreturn_t avs_ipc_irq_handler(struct avs_dev *adev);
irqreturn_t avs_dsp_irq_handler(int irq, void *dev_id);
irqreturn_t avs_dsp_irq_thread(int irq, void *dev_id);
void avs_dsp_process_response(struct avs_dev *adev, u64 header);
int avs_dsp_send_pm_msg_timeout(struct avs_dev *adev,
				struct avs_ipc_msg request,
				struct avs_ipc_msg *reply, int timeout,
				bool wake_d0i0);
int avs_dsp_send_pm_msg(struct avs_dev *adev,
			struct avs_ipc_msg request,
			struct avs_ipc_msg *reply, bool wake_d0i0);
int avs_dsp_send_msg_timeout(struct avs_dev *adev,
			     struct avs_ipc_msg request,
			     struct avs_ipc_msg *reply, int timeout);
int avs_dsp_send_msg(struct avs_dev *adev,
		     struct avs_ipc_msg request, struct avs_ipc_msg *reply);
int avs_dsp_send_rom_msg_timeout(struct avs_dev *adev,
				 struct avs_ipc_msg request, int timeout);
int avs_dsp_send_rom_msg(struct avs_dev *adev, struct avs_ipc_msg request);
void avs_dsp_int_control(struct avs_dev *adev, bool enable);
int avs_ipc_init(struct avs_ipc *ipc, struct device *dev);
void avs_ipc_block(struct avs_ipc *ipc);

struct avs_notify_action {
	u32 identifier;
	void *context;
	void (*callback)(union avs_notify_msg, void *, size_t, void *);
	struct list_head node;
};

int avs_notify_subscribe(struct list_head *sub_list, u32 notify_id,
			 void (*callback)(union avs_notify_msg, void *, size_t, void *),
			 void *context);
int avs_notify_unsubscribe(struct list_head *sub_list, u32 notify_id, void *context);

void avs_dsp_log_buffer_status(union avs_notify_msg msg, void *data,
			       size_t data_size, void *context);
void avs_dsp_recovery_work(struct work_struct *work);
void avs_dsp_exception_caught(union avs_notify_msg msg, void *data,
			      size_t data_size, void *context);

int avs_dsp_disable_d0ix(struct avs_dev *adev);
int avs_dsp_enable_d0ix(struct avs_dev *adev);

irqreturn_t skl_dsp_irq_thread(struct avs_dev *adev);
unsigned int skl_log_buffer_offset(struct avs_dev *adev, u32 core);
int apl_enable_logs(struct avs_dev *adev, enum avs_log_enable enable,
		    u32 aging_period, u32 fifo_full_period,
		    unsigned long resource_mask, u32 *priorities);
int apl_log_buffer_status(struct avs_dev *adev, union avs_notify_msg msg);
int apl_coredump(struct avs_dev *adev, union avs_notify_msg msg);
bool apl_d0ix_toggle(struct avs_dev *adev, struct avs_ipc_msg *tx, bool wake);
int apl_set_d0ix(struct avs_dev *adev, bool enable);

/* Firmware resources management */

int avs_get_module_entry(struct avs_dev *adev, const guid_t *uuid, struct avs_module_entry *entry);
int avs_get_module_id_entry(struct avs_dev *adev, u32 module_id, struct avs_module_entry *entry);
int avs_get_module_id(struct avs_dev *adev, const guid_t *uuid);
int avs_is_module_ida_empty(struct avs_dev *adev, u32 module_id);

int avs_module_info_init(struct avs_dev *adev, bool purge);
void avs_module_info_free(struct avs_dev *adev);
int avs_module_id_alloc(struct avs_dev *adev, u16 module_id);
void avs_module_id_free(struct avs_dev *adev, u16 module_id, u8 instance_id);
int avs_request_firmware(struct avs_dev *adev, const struct firmware **fw_p, const char *name);
void avs_release_firmwares(struct avs_dev *adev);

int avs_dsp_init_module(struct avs_dev *adev, u16 module_id, u8 ppl_instance_id,
			u8 core_id, u8 domain, void *param, u32 param_size,
			u16 *instance_id);
void avs_dsp_delete_module(struct avs_dev *adev, u16 module_id, u16 instance_id,
			   u8 ppl_instance_id, u8 core_id);
int avs_dsp_create_pipeline(struct avs_dev *adev, u16 req_size, u8 priority,
			    bool lp, u16 attributes, u8 *instance_id);
int avs_dsp_delete_pipeline(struct avs_dev *adev, u8 instance_id);

/* Firmware loading */

void avs_hda_clock_gating_enable(struct avs_dev *adev, bool enable);
void avs_hda_power_gating_enable(struct avs_dev *adev, bool enable);
void avs_hda_l1sen_enable(struct avs_dev *adev, bool enable);

int avs_dsp_load_libraries(struct avs_dev *adev, struct avs_tplg_library *libs,
			   u32 num_libs);
int avs_dsp_boot_firmware(struct avs_dev *adev, bool purge);
int avs_dsp_first_boot_firmware(struct avs_dev *adev);

int avs_cldma_load_basefw(struct avs_dev *adev, struct firmware *fw);
int avs_cldma_load_library(struct avs_dev *adev, struct firmware *lib, u32 id);
int avs_cldma_transfer_modules(struct avs_dev *adev, bool load,
			       struct avs_module_entry *mods, u32 num_mods);
int avs_hda_load_basefw(struct avs_dev *adev, struct firmware *fw);
int avs_hda_load_library(struct avs_dev *adev, struct firmware *lib, u32 id);
int avs_hda_transfer_modules(struct avs_dev *adev, bool load,
			     struct avs_module_entry *mods, u32 num_mods);

/* Soc component members */

struct avs_soc_component {
	struct snd_soc_component base;
	struct avs_tplg *tplg;
	struct kobject *kobj;

	struct list_head node;
};

#define to_avs_soc_component(comp) \
	container_of(comp, struct avs_soc_component, base)

extern const struct snd_soc_dai_ops avs_dai_fe_ops;

int avs_dmic_platform_register(struct avs_dev *adev, const char *name);
int avs_ssp_platform_register(struct avs_dev *adev, const char *name,
			      unsigned long port_mask, unsigned long *tdms);
int avs_hda_platform_register(struct avs_dev *adev, const char *name);

int avs_register_all_boards(struct avs_dev *adev);
void avs_unregister_all_boards(struct avs_dev *adev);

/* Firmware tracing helpers */

unsigned int __kfifo_fromio_locked(struct kfifo *fifo, const void __iomem *src,
				   unsigned int len, spinlock_t *lock);

#define avs_log_buffer_size(adev) \
	((adev)->fw_cfg.trace_log_bytes / (adev)->hw_cfg.dsp_cores)

#define avs_log_buffer_addr(adev, core) \
	(avs_sram_addr(adev, AVS_DEBUG_WINDOW) + \
	 avs_dsp_op(adev, log_buffer_offset, core))

struct apl_log_buffer_layout {
	u32 read_ptr;
	u32 write_ptr;
	u8 buffer[];
} __packed;

#define apl_log_payload_size(adev) \
	(avs_log_buffer_size(adev) - sizeof(struct apl_log_buffer_layout))

#define apl_log_payload_addr(addr) \
	(addr + sizeof(struct apl_log_buffer_layout))

#endif /* __SOUND_SOC_INTEL_AVS_H */
