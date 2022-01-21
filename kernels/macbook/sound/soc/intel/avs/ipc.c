// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Authors: Cezary Rojewski <cezary.rojewski@intel.com>
//          Amadeusz Slawinski <amadeuszx.slawinski@linux.intel.com>
//

#include <linux/slab.h>
#include <sound/hdaudio_ext.h>
#include "avs.h"
#include "messages.h"
#include "registers.h"
#include "trace.h"

#define AVS_IPC_TIMEOUT_MS	300
#define AVS_D0IX_DELAY_MS	300

static struct avs_notify_action *
notify_get_action(struct list_head *sub_list, u32 id, void *context)
{
	struct avs_notify_action *action;

	list_for_each_entry(action, sub_list, node) {
		if (context && action->context != context)
			continue;
		if (action->identifier == id)
			return action;
	}

	return NULL;
}

int avs_notify_subscribe(struct list_head *sub_list, u32 notify_id,
			 void (*callback)(union avs_notify_msg, void *, size_t, void *),
			 void *context)
{
	struct avs_notify_action *action;

	action = notify_get_action(sub_list, notify_id, context);
	if (action)
		return -EEXIST;

	action = kmalloc(sizeof(*action), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->identifier = notify_id;
	action->context = context;
	action->callback = callback;
	INIT_LIST_HEAD(&action->node);

	list_add_tail(&action->node, sub_list);
	return 0;
}

int avs_notify_unsubscribe(struct list_head *sub_list, u32 notify_id, void *context)
{
	struct avs_notify_action *action;

	action = notify_get_action(sub_list, notify_id, context);
	if (!action)
		return -ENOENT;

	list_del(&action->node);
	kfree(action);
	return 0;
}

static int
avs_dsp_set_d0ix(struct avs_dev *adev, bool enable)
{
	struct avs_ipc *ipc = adev->ipc;
	int ret;

	/* Is transition required? */
	if (ipc->in_d0ix == enable)
		return 0;

	ret = avs_dsp_op(adev, set_d0ix, enable);
	if (ret) {
		/* Prevent further d0ix attempts on conscious IPC failure. */
		if (ret == -AVS_EIPC)
			atomic_inc(&ipc->d0ix_disable_depth);

		ipc->in_d0ix = false;
		return ret;
	}

	ipc->in_d0ix = enable;
	return 0;
}

static void avs_dsp_schedule_d0ix(struct avs_dev *adev, struct avs_ipc_msg *tx)
{
	if (atomic_read(&adev->ipc->d0ix_disable_depth))
		return;

	mod_delayed_work(system_power_efficient_wq,
			 &adev->ipc->d0ix_work,
			 msecs_to_jiffies(AVS_D0IX_DELAY_MS));
}

static void avs_dsp_d0ix_work(struct work_struct *work)
{
	struct avs_ipc *ipc =
		container_of(work, struct avs_ipc, d0ix_work.work);

	avs_dsp_set_d0ix(to_avs_dev(ipc->dev), true);
}

static int avs_dsp_wake_d0i0(struct avs_dev *adev, struct avs_ipc_msg *tx)
{
	struct avs_ipc *ipc = adev->ipc;

	if (!atomic_read(&ipc->d0ix_disable_depth)) {
		cancel_delayed_work_sync(&ipc->d0ix_work);
		return avs_dsp_set_d0ix(adev, false);
	}

	return 0;
}

int avs_dsp_disable_d0ix(struct avs_dev *adev)
{
	struct avs_ipc *ipc = adev->ipc;

	/* Prevent PG only on the first disable. */
	if (atomic_add_return(1, &ipc->d0ix_disable_depth) == 1) {
		cancel_delayed_work_sync(&ipc->d0ix_work);
		return avs_dsp_set_d0ix(adev, false);
	}

	return 0;
}

int avs_dsp_enable_d0ix(struct avs_dev *adev)
{
	struct avs_ipc *ipc = adev->ipc;

	if (atomic_dec_and_test(&ipc->d0ix_disable_depth))
		queue_delayed_work(system_power_efficient_wq,
				   &ipc->d0ix_work,
				   msecs_to_jiffies(AVS_D0IX_DELAY_MS));
	return 0;
}

void avs_dsp_log_buffer_status(union avs_notify_msg msg, void *data,
			       size_t data_size, void *context)
{
	struct avs_dev *adev = context;

	avs_dsp_op(adev, log_buffer_status, msg);
}

static void avs_dsp_recovery(struct avs_dev *adev)
{
	struct avs_soc_component *acomp;
	unsigned int core_mask;
	int ret;

	/* disconnect all running streams */
	list_for_each_entry(acomp, &adev->comp_list, node) {
		struct snd_soc_pcm_runtime *rtd;
		struct snd_soc_card *card;

		card = acomp->base.card;
		if (!card)
			continue;

		for_each_card_rtds(card, rtd) {
			struct snd_pcm *pcm;
			int dir;

			pcm = rtd->pcm;
			if (!pcm || rtd->dai_link->no_pcm)
				continue;

			for_each_pcm_streams(dir) {
				struct snd_pcm_substream *substream;

				substream = pcm->streams[dir].substream;
				if (!substream || !substream->runtime)
					continue;

				snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
			}
		}
	}

	/* forcibly shutdown all cores */
	core_mask = GENMASK(adev->hw_cfg.dsp_cores - 1, 0);
	avs_dsp_core_disable(adev, core_mask);

	/* attempt dsp reboot */
	ret = avs_dsp_boot_firmware(adev, true);
	if (ret < 0)
		dev_err(adev->dev, "firmware reboot failed: %d\n", ret);

	pm_runtime_mark_last_busy(adev->dev);
	pm_runtime_enable(adev->dev);
	pm_request_autosuspend(adev->dev);
}

void avs_dsp_recovery_work(struct work_struct *work)
{
	struct avs_dev *adev = container_of(work, struct avs_dev, recovery_work);

	avs_dsp_recovery(adev);
}

void avs_dsp_exception_caught(union avs_notify_msg msg, void *data,
			      size_t data_size, void *context)
{
	struct avs_dev *adev = context;

	dev_err(adev->dev, "dsp core exception caught\n");

	/* Re-enabled on recovery completion. */
	avs_ipc_block(adev->ipc);
	pm_runtime_disable(adev->dev);

	/* Process received notification. */
	avs_dsp_op(adev, coredump, msg);

	schedule_work(&adev->recovery_work);
}

static void avs_dsp_receive_rx(struct avs_dev *adev, u64 header)
{
	struct avs_ipc *ipc = adev->ipc;
	union avs_reply_msg msg = AVS_MSG(header);
	u64 reg;

	reg = readq(avs_sram_addr(adev, AVS_FW_REGS_WINDOW));
	trace_avs_ipc_reply_msg(header, reg);

	ipc->rx.header = header;
	if (!msg.status) {
		/* update size in case of LARGE_CONFIG_GET */
		if (msg.msg_target == AVS_MOD_MSG &&
		    msg.global_msg_type == AVS_MOD_LARGE_CONFIG_GET)
			ipc->rx.size = msg.ext.large_config.data_off_size;

		memcpy_fromio(ipc->rx.data, avs_uplink_addr(adev),
			      ipc->rx.size);
		trace_avs_msg_payload(ipc->rx.data, ipc->rx.size);
	}
}

static void avs_dsp_process_notification(struct avs_dev *adev, u64 header)
{
	struct avs_notify_action *action;
	union avs_notify_msg msg = AVS_MSG(header);
	size_t data_size = 0;
	void *data = NULL;
	u64 reg;

	reg = readq(avs_sram_addr(adev, AVS_FW_REGS_WINDOW));
	trace_avs_ipc_notify_msg(header, reg);

	switch (msg.notify_msg_type) {
		struct avs_notify_mod_data mod_data;

	case AVS_NOTIFY_FW_READY:
		dev_dbg(adev->dev, "FW READY %x\n", msg.primary);
		adev->ipc->ready = true;
		complete(&adev->fw_ready);
		break;

	case AVS_NOTIFY_PHRASE_DETECTED:
		data_size = sizeof(struct avs_notify_voice_data);
		break;

	case AVS_NOTIFY_RESOURCE_EVENT:
		data_size = sizeof(struct avs_notify_res_data);
		break;

	case AVS_NOTIFY_LOG_BUFFER_STATUS:
	case AVS_NOTIFY_EXCEPTION_CAUGHT:
		break;

	case AVS_NOTIFY_MODULE_EVENT:
		memcpy_fromio(&mod_data, avs_uplink_addr(adev), sizeof(mod_data));
		data_size = sizeof(mod_data) + mod_data.data_size;
		break;

	default:
		dev_warn(adev->dev, "unknown notification: 0x%x\n",
			 msg.primary);
		break;
	}

	if (data_size) {
		data = kmalloc(data_size, GFP_KERNEL);
		if (!data)
			return;

		memcpy_fromio(data, avs_uplink_addr(adev), data_size);
		trace_avs_msg_payload(data, data_size);
	}

	list_for_each_entry(action, &adev->notify_sub_list, node)
		if (action->identifier == msg.notify_msg_type)
			action->callback(msg, data, data_size, action->context);

	kfree(data);
}

void avs_dsp_process_response(struct avs_dev *adev, u64 header)
{
	struct avs_ipc *ipc = adev->ipc;
	unsigned long flags;

	if (avs_msg_is_reply(header)) {
		spin_lock_irqsave(&ipc->lock, flags);
		avs_dsp_receive_rx(adev, header);
		ipc->completed = true;
		spin_unlock_irqrestore(&ipc->lock, flags);
	} else {
		avs_dsp_process_notification(adev, header);
	}

	complete(&ipc->busy_completion);
}

irqreturn_t avs_ipc_irq_handler(struct avs_dev *adev)
{
	const struct avs_spec *const spec = adev->spec;
	struct avs_ipc *ipc = adev->ipc;
	u32 adspis, hipc_rsp, hipc_ack;
	irqreturn_t ret = IRQ_NONE;

	adspis = snd_hdac_adsp_readl(adev, AZX_ADSP_REG_ADSPIS);
	if (adspis == UINT_MAX || !(adspis & AZX_ADSP_ADSPIS_IPC))
		return ret;

	hipc_ack = snd_hdac_adsp_readl(adev, spec->hipc_ack);
	hipc_rsp = snd_hdac_adsp_readl(adev, spec->hipc_rsp);

	/* DSP acked host's request */
	if (hipc_ack & spec->hipc_ack_done) {
		/* mask done interrupt */
		snd_hdac_adsp_updatel(adev, spec->hipc_ctl,
				      AZX_ADSP_HIPCCTL_DONE, 0);

		complete(&ipc->done_completion);

		/* tell DSP it has our attention */
		snd_hdac_adsp_updatel(adev, spec->hipc_ack,
				      spec->hipc_ack_done,
				      spec->hipc_ack_done);
		/* unmask done interrupt */
		snd_hdac_adsp_updatel(adev, spec->hipc_ctl,
				      AZX_ADSP_HIPCCTL_DONE,
				      AZX_ADSP_HIPCCTL_DONE);
		ret = IRQ_HANDLED;
	}

	/* DSP sent new response to process */
	if (hipc_rsp & spec->hipc_rsp_busy) {
		/* mask busy interrupt */
		snd_hdac_adsp_updatel(adev, spec->hipc_ctl,
				      AZX_ADSP_HIPCCTL_BUSY, 0);

		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

irqreturn_t avs_dsp_irq_handler(int irq, void *dev_id)
{
	struct avs_dev *adev = dev_id;

	/* Check IPC interrupt status */
	return avs_dsp_op(adev, irq_handler);
}

irqreturn_t avs_dsp_irq_thread(int irq, void *dev_id)
{
	struct avs_dev *adev = dev_id;

	/* Dispatch IPC interrupt */
	return avs_dsp_op(adev, irq_thread);
}

static bool avs_ipc_is_busy(struct avs_ipc *ipc)
{
	struct avs_dev *adev = to_avs_dev(ipc->dev);
	const struct avs_spec *const spec = adev->spec;
	u32 hipc_rsp;

	hipc_rsp = snd_hdac_adsp_readl(adev, spec->hipc_rsp);
	return hipc_rsp & spec->hipc_rsp_busy;
}

static int avs_ipc_wait_busy_completion(struct avs_ipc *ipc, int timeout)
{
	int ret;

again:
	ret = wait_for_completion_timeout(&ipc->busy_completion,
					  msecs_to_jiffies(timeout));
	/*
	 * DSP could be unresponsive at this point e.g. manifested by
	 * EXCEPTION_CAUGHT notification. If so, no point in continuing.
	 */
	if (!ipc->ready)
		return -EPERM;

	if (!ret) {
		if (!avs_ipc_is_busy(ipc))
			return -ETIMEDOUT;
		/*
		 * Fw did its job, either notification or reply
		 * has been received - now wait until it's processed.
		 */
		wait_for_completion_killable(&ipc->busy_completion);
	}

	/* Ongoing notification bh may cause early wakeup */
	spin_lock_irq(&ipc->lock);
	if (!ipc->completed) {
		/* Reply delayed due to nofitication. */
		reinit_completion(&ipc->busy_completion);
		spin_unlock_irq(&ipc->lock);
		goto again;
	}

	spin_unlock_irq(&ipc->lock);
	return 0;
}

static void avs_ipc_msg_init(struct avs_ipc *ipc, struct avs_ipc_msg request,
			     struct avs_ipc_msg *reply)
{
	lockdep_assert_held(&ipc->lock);

	ipc->rx.header = 0;
	ipc->rx.size = reply ? reply->size : 0;
	ipc->completed = false;

	reinit_completion(&ipc->done_completion);
	reinit_completion(&ipc->busy_completion);
}

static void avs_dsp_send_tx(struct avs_dev *adev, const struct avs_ipc_msg *tx,
			    bool read_fwregs)
{
	const struct avs_spec *const spec = adev->spec;
	u64 reg = ULONG_MAX;

	if (read_fwregs)
		reg = readq(avs_sram_addr(adev, AVS_FW_REGS_WINDOW));
	trace_avs_request(tx, reg);

	if (tx->size)
		memcpy_toio(avs_downlink_addr(adev), tx->data, tx->size);
	snd_hdac_adsp_writel(adev, spec->hipc_req_ext, tx->header >> 32);
	snd_hdac_adsp_writel(adev, spec->hipc_req,
			     (tx->header & UINT_MAX) | spec->hipc_req_busy);
}

static int avs_dsp_do_send_msg(struct avs_dev *adev, struct avs_ipc_msg request,
			       struct avs_ipc_msg *reply, int timeout)
{
	struct avs_ipc *ipc = adev->ipc;
	unsigned long flags;
	int ret;

	if (!ipc->ready)
		return -EPERM;

	mutex_lock(&ipc->mutex);

	spin_lock_irqsave(&ipc->lock, flags);
	avs_ipc_msg_init(ipc, request, reply);
	avs_dsp_send_tx(adev, &request, true);
	spin_unlock_irqrestore(&ipc->lock, flags);

	ret = avs_ipc_wait_busy_completion(ipc, timeout);
	if (ret) {
		if (ret == -ETIMEDOUT) {
			dev_crit(adev->dev, "communication severed: %d, rebooting dsp..\n",
				 ret);

			/* Re-enabled on recovery completion. */
			avs_ipc_block(ipc);
			pm_runtime_disable(adev->dev);

			schedule_work(&adev->recovery_work);
		}
		goto exit;
	}

	ret = ipc->rx.rsp.status;
	if (reply) {
		reply->header = ipc->rx.header;
		if (reply->data && ipc->rx.size)
			memcpy(reply->data, ipc->rx.data, reply->size);
	}

exit:
	mutex_unlock(&ipc->mutex);
	return ret;
}

static int avs_dsp_send_msg_sequence(struct avs_dev *adev,
				     struct avs_ipc_msg request,
				     struct avs_ipc_msg *reply, int timeout,
				     bool wake_d0i0, bool schedule_d0ix)
{
	int ret;

	trace_avs_d0ix("wake", wake_d0i0, request.header);
	if (wake_d0i0) {
		ret = avs_dsp_wake_d0i0(adev, &request);
		if (ret)
			return ret;
	}

	ret = avs_dsp_do_send_msg(adev, request, reply, timeout);
	if (ret)
		return ret;

	trace_avs_d0ix("schedule", schedule_d0ix, request.header);
	if (schedule_d0ix)
		avs_dsp_schedule_d0ix(adev, &request);

	return 0;
}

int avs_dsp_send_pm_msg_timeout(struct avs_dev *adev,
				struct avs_ipc_msg request,
				struct avs_ipc_msg *reply, int timeout,
				bool wake_d0i0)
{
	return avs_dsp_send_msg_sequence(adev, request, reply, timeout,
					 wake_d0i0, false);
}

int avs_dsp_send_pm_msg(struct avs_dev *adev,
			struct avs_ipc_msg request,
			struct avs_ipc_msg *reply, bool wake_d0i0)
{
	return avs_dsp_send_pm_msg_timeout(adev, request, reply,
					   adev->ipc->default_timeout,
					   wake_d0i0);
}

int avs_dsp_send_msg_timeout(struct avs_dev *adev, struct avs_ipc_msg request,
			     struct avs_ipc_msg *reply, int timeout)
{
	bool wake_d0i0 = avs_dsp_op(adev, d0ix_toggle, &request, true);
	bool schedule_d0ix = avs_dsp_op(adev, d0ix_toggle, &request, false);

	return avs_dsp_send_msg_sequence(adev, request, reply, timeout,
					 wake_d0i0, schedule_d0ix);
}

int avs_dsp_send_msg(struct avs_dev *adev, struct avs_ipc_msg request,
		     struct avs_ipc_msg *reply)
{
	return avs_dsp_send_msg_timeout(adev, request, reply,
					adev->ipc->default_timeout);
}

static int avs_dsp_do_send_rom_msg(struct avs_dev *adev, struct avs_ipc_msg request,
				   int timeout)
{
	struct avs_ipc *ipc = adev->ipc;
	unsigned long flags;
	int ret;

	mutex_lock(&ipc->mutex);

	spin_lock_irqsave(&ipc->lock, flags);
	avs_ipc_msg_init(ipc, request, NULL);
	/*
	 * with hw still stalled, memory windows may not be
	 * configured properly so avoid accessing SRAM
	 */
	avs_dsp_send_tx(adev, &request, false);
	spin_unlock_irqrestore(&ipc->lock, flags);

	/* ROM messages must be sent before master core is unstalled */
	avs_dsp_op(adev, stall, adev->spec->master_mask, false);

	ret = wait_for_completion_timeout(&ipc->done_completion,
					  msecs_to_jiffies(timeout));

	mutex_unlock(&ipc->mutex);

	if (!ret)
		return -ETIMEDOUT;
	return 0;
}

int avs_dsp_send_rom_msg_timeout(struct avs_dev *adev,
				 struct avs_ipc_msg request, int timeout)
{
	return avs_dsp_do_send_rom_msg(adev, request, timeout);
}

int avs_dsp_send_rom_msg(struct avs_dev *adev, struct avs_ipc_msg request)
{
	return avs_dsp_send_rom_msg_timeout(adev, request,
					    adev->ipc->default_timeout);
}

void avs_dsp_int_control(struct avs_dev *adev, bool enable)
{
	const struct avs_spec *const spec = adev->spec;
	u32 value;

	value = enable ? AZX_ADSP_ADSPIC_IPC : 0;
	snd_hdac_adsp_updatel(adev, AZX_ADSP_REG_ADSPIC,
			      AZX_ADSP_ADSPIC_IPC, value);

	value = enable ? AZX_ADSP_HIPCCTL_DONE : 0;
	snd_hdac_adsp_updatel(adev, spec->hipc_ctl,
			      AZX_ADSP_HIPCCTL_DONE, value);

	value = enable ? AZX_ADSP_HIPCCTL_BUSY : 0;
	snd_hdac_adsp_updatel(adev, spec->hipc_ctl,
			      AZX_ADSP_HIPCCTL_BUSY, value);
}

int avs_ipc_init(struct avs_ipc *ipc, struct device *dev)
{
	ipc->rx.data = devm_kzalloc(dev, AVS_MAILBOX_SIZE, GFP_KERNEL);
	if (!ipc->rx.data)
		return -ENOMEM;

	ipc->dev = dev;
	ipc->ready = false;
	ipc->default_timeout = AVS_IPC_TIMEOUT_MS;
	INIT_DELAYED_WORK(&ipc->d0ix_work, avs_dsp_d0ix_work);
	init_completion(&ipc->done_completion);
	init_completion(&ipc->busy_completion);
	spin_lock_init(&ipc->lock);
	mutex_init(&ipc->mutex);

	return 0;
}

void avs_ipc_block(struct avs_ipc *ipc)
{
	ipc->ready = false;
	cancel_delayed_work_sync(&ipc->d0ix_work);
	ipc->in_d0ix = false;
}
