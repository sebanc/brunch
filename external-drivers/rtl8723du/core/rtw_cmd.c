// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_CMD_C_

#include <drv_types.h>
#include <hal_data.h>

#ifndef DBG_CMD_EXECUTE
	#define DBG_CMD_EXECUTE 0
#endif

/*
Caller and the rtw_cmd_thread can protect cmd_q by spin_lock.
No irqsave is necessary.
*/

int	_rtw_init_cmd_priv(struct	cmd_priv *pcmdpriv)
{
	int res = _SUCCESS;


	sema_init(&(pcmdpriv->cmd_queue_sema), 0);
	/* sema_init(&(pcmdpriv->cmd_done_sema), 0); */
	sema_init(&(pcmdpriv->start_cmdthread_sema), 0);

	_rtw_init_queue(&(pcmdpriv->cmd_queue));

	/* allocate DMA-able/Non-Page memory for cmd_buf and rsp_buf */

	pcmdpriv->cmd_seq = 1;

	pcmdpriv->cmd_allocated_buf = rtw_zmalloc(MAX_CMDSZ + CMDBUFF_ALIGN_SZ);

	if (!pcmdpriv->cmd_allocated_buf) {
		res = _FAIL;
		goto exit;
	}

	pcmdpriv->cmd_buf = pcmdpriv->cmd_allocated_buf  +  CMDBUFF_ALIGN_SZ - ((SIZE_PTR)(pcmdpriv->cmd_allocated_buf) & (CMDBUFF_ALIGN_SZ - 1));

	pcmdpriv->rsp_allocated_buf = rtw_zmalloc(MAX_RSPSZ + 4);

	if (!pcmdpriv->rsp_allocated_buf) {
		res = _FAIL;
		goto exit;
	}

	pcmdpriv->rsp_buf = pcmdpriv->rsp_allocated_buf  +  4 - ((SIZE_PTR)(pcmdpriv->rsp_allocated_buf) & 3);

	pcmdpriv->cmd_issued_cnt = pcmdpriv->cmd_done_cnt = pcmdpriv->rsp_cnt = 0;

	_rtw_mutex_init(&pcmdpriv->sctx_mutex);
exit:


	return res;

}

#ifdef CONFIG_C2H_WK
static void c2h_wk_callback(_workitem *work)
{
	struct evt_priv *evtpriv = container_of(work, struct evt_priv, c2h_wk);
	struct adapter *adapter = container_of(evtpriv, struct adapter, evtpriv);
	u8 *c2h_evt;
	c2h_id_filter direct_hdl_filter = rtw_hal_c2h_id_handle_directly;
	u8 id, seq, plen;
	u8 *payload;

	evtpriv->c2h_wk_alive = true;

	while (!rtw_cbuf_empty(evtpriv->c2h_queue)) {
		c2h_evt = (u8 *)rtw_cbuf_pop(evtpriv->c2h_queue);
		if (c2h_evt) {
			/* This C2H event is read, clear it */
			c2h_evt_clear(adapter);
		} else {
			c2h_evt = (u8 *)rtw_malloc(C2H_REG_LEN);
			if (!c2h_evt) {
				rtw_warn_on(1);
				continue;
			}

			/* This C2H event is not read, read & clear now */
			if (rtw_hal_c2h_evt_read(adapter, c2h_evt) != _SUCCESS) {
				rtw_mfree(c2h_evt, C2H_REG_LEN);
				continue;
			}
		}

		/* Special pointer to trigger c2h_evt_clear only */
		if ((void *)c2h_evt == (void *)evtpriv)
			continue;

		if (!rtw_hal_c2h_valid(adapter, c2h_evt)
			|| rtw_hal_c2h_reg_hdr_parse(adapter, c2h_evt, &id, &seq, &plen, &payload) != _SUCCESS
		) {
			rtw_mfree(c2h_evt, C2H_REG_LEN);
			continue;
		}

		if (direct_hdl_filter(adapter, id, seq, plen, payload)) {
			/* Handle directly */
			rtw_hal_c2h_handler(adapter, id, seq, plen, payload);
			rtw_mfree(c2h_evt, C2H_REG_LEN);
		} else {
			/* Enqueue into cmd_thread for others */
			rtw_c2h_reg_wk_cmd(adapter, c2h_evt);
			rtw_mfree(c2h_evt, C2H_REG_LEN);
		}
	}

	evtpriv->c2h_wk_alive = false;
}
#endif /* CONFIG_C2H_WK */

int _rtw_init_evt_priv(struct evt_priv *pevtpriv)
{
	int res = _SUCCESS;


#ifdef CONFIG_H2CLBK
	sema_init(&(pevtpriv->lbkevt_done), 0);
	pevtpriv->lbkevt_limit = 0;
	pevtpriv->lbkevt_num = 0;
	pevtpriv->cmdevt_parm = NULL;
#endif

	/* allocate DMA-able/Non-Page memory for cmd_buf and rsp_buf */
	ATOMIC_SET(&pevtpriv->event_seq, 0);
	pevtpriv->evt_done_cnt = 0;

#ifdef CONFIG_EVENT_THREAD_MODE

	sema_init(&(pevtpriv->evt_notify), 0);

	pevtpriv->evt_allocated_buf = rtw_zmalloc(MAX_EVTSZ + 4);
	if (!pevtpriv->evt_allocated_buf) {
		res = _FAIL;
		goto exit;
	}
	pevtpriv->evt_buf = pevtpriv->evt_allocated_buf  +  4 - ((unsigned int)(pevtpriv->evt_allocated_buf) & 3);

	_rtw_init_queue(&(pevtpriv->evt_queue));

exit:

#endif /* end of CONFIG_EVENT_THREAD_MODE */

#ifdef CONFIG_C2H_WK
	_init_workitem(&pevtpriv->c2h_wk, c2h_wk_callback, NULL);
	pevtpriv->c2h_wk_alive = false;
	pevtpriv->c2h_queue = rtw_cbuf_alloc(C2H_QUEUE_MAX_LEN + 1);
#endif


	return res;
}

void _rtw_free_evt_priv(struct	evt_priv *pevtpriv)
{


#ifdef CONFIG_EVENT_THREAD_MODE
	if (pevtpriv->evt_allocated_buf)
		rtw_mfree(pevtpriv->evt_allocated_buf, MAX_EVTSZ + 4);
#endif

#ifdef CONFIG_C2H_WK
	_cancel_workitem_sync(&pevtpriv->c2h_wk);
	while (pevtpriv->c2h_wk_alive)
		rtw_msleep_os(10);

	while (!rtw_cbuf_empty(pevtpriv->c2h_queue)) {
		void *c2h;
		c2h = rtw_cbuf_pop(pevtpriv->c2h_queue);
		if (c2h && c2h != (void *)pevtpriv)
			rtw_mfree(c2h, 16);
	}
	rtw_cbuf_free(pevtpriv->c2h_queue);
#endif



}

void _rtw_free_cmd_priv(struct	cmd_priv *pcmdpriv)
{

	if (pcmdpriv) {
		if (pcmdpriv->cmd_allocated_buf)
			rtw_mfree(pcmdpriv->cmd_allocated_buf, MAX_CMDSZ + CMDBUFF_ALIGN_SZ);

		if (pcmdpriv->rsp_allocated_buf)
			rtw_mfree(pcmdpriv->rsp_allocated_buf, MAX_RSPSZ + 4);

		_rtw_mutex_free(&pcmdpriv->sctx_mutex);
	}
}

/* Calling Context:
 *
 * rtw_enqueue_cmd can only be called between kernel thread,
 * since only spin_lock is used.
 * 
 * ISR/Call-Back functions can't call this sub-function.
 */
int _rtw_enqueue_cmd(struct __queue *queue, struct cmd_obj *obj, bool to_head)
{
	unsigned long irqL;


	if (!obj)
		goto exit;

	_enter_critical(&queue->lock, &irqL);

	if (to_head)
		list_add(&obj->list, &queue->queue);
	else
		list_add_tail(&obj->list, &queue->queue);

	_exit_critical(&queue->lock, &irqL);

exit:

	return _SUCCESS;
}

struct	cmd_obj	*_rtw_dequeue_cmd(struct __queue *queue)
{
	unsigned long irqL;
	struct cmd_obj *obj;

	_enter_critical(&queue->lock, &irqL);

	if (rtw_is_list_empty(&(queue->queue)))
		obj = NULL;
	else {
		obj = container_of(get_next(&(queue->queue)), struct cmd_obj, list);

		rtw_list_delete(&obj->list);
	}

	_exit_critical(&queue->lock, &irqL);
	return obj;
}

u32	rtw_init_cmd_priv(struct cmd_priv *pcmdpriv)
{
	u32	res;
	res = _rtw_init_cmd_priv(pcmdpriv);
	return res;
}

u32	rtw_init_evt_priv(struct	evt_priv *pevtpriv)
{
	int	res;
	res = _rtw_init_evt_priv(pevtpriv);
	return res;
}

void rtw_free_evt_priv(struct	evt_priv *pevtpriv)
{
	_rtw_free_evt_priv(pevtpriv);
}

void rtw_free_cmd_priv(struct	cmd_priv *pcmdpriv)
{
	_rtw_free_cmd_priv(pcmdpriv);
}

int rtw_cmd_filter(struct cmd_priv *pcmdpriv, struct cmd_obj *cmd_obj);
int rtw_cmd_filter(struct cmd_priv *pcmdpriv, struct cmd_obj *cmd_obj)
{
	u8 bAllow = false; /* set to true to allow enqueuing cmd when hw_init_completed is false */

#ifdef SUPPORT_HW_RFOFF_DETECTED
	/* To decide allow or not */
	if ((adapter_to_pwrctl(pcmdpriv->adapt)->bHWPwrPindetect)
	    && (!pcmdpriv->adapt->registrypriv.usbss_enable)
	   ) {
		if (cmd_obj->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
			struct drvextra_cmd_parm	*pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)cmd_obj->parmbuf;
			if (pdrvextra_cmd_parm->ec_id == POWER_SAVING_CTRL_WK_CID) {
				/* RTW_INFO("==>enqueue POWER_SAVING_CTRL_WK_CID\n"); */
				bAllow = true;
			}
		}
	}
#endif

	if (cmd_obj->cmdcode == GEN_CMD_CODE(_SetChannelPlan))
		bAllow = true;

	if (cmd_obj->no_io)
		bAllow = true;

	if ((!rtw_is_hw_init_completed(pcmdpriv->adapt) && (!bAllow))
	    || !ATOMIC_READ(&(pcmdpriv->cmdthd_running))	/* com_thread not running */
	   ) {
		if (DBG_CMD_EXECUTE)
			RTW_INFO(ADPT_FMT" drop "CMD_FMT" hw_init_completed:%u, cmdthd_running:%u\n", ADPT_ARG(cmd_obj->adapt)
				, CMD_ARG(cmd_obj), rtw_get_hw_init_completed(cmd_obj->adapt), ATOMIC_READ(&pcmdpriv->cmdthd_running));
		return _FAIL;
	}
	return _SUCCESS;
}



u32 rtw_enqueue_cmd(struct cmd_priv *pcmdpriv, struct cmd_obj *cmd_obj)
{
	int res = _FAIL;
	struct adapter * adapt = pcmdpriv->adapt;


	if (!cmd_obj)
		goto exit;

	cmd_obj->adapt = adapt;

#ifdef CONFIG_CONCURRENT_MODE
	/* change pcmdpriv to primary's pcmdpriv */
	if (adapt->adapter_type != PRIMARY_ADAPTER)
		pcmdpriv = &(GET_PRIMARY_ADAPTER(adapt)->cmdpriv);
#endif

	res = rtw_cmd_filter(pcmdpriv, cmd_obj);
	if ((_FAIL == res) || (cmd_obj->cmdsz > MAX_CMDSZ)) {
		if (cmd_obj->cmdsz > MAX_CMDSZ) {
			RTW_INFO("%s failed due to obj->cmdsz(%d) > MAX_CMDSZ(%d)\n", __func__, cmd_obj->cmdsz, MAX_CMDSZ);
			rtw_warn_on(1);
		}

		if (cmd_obj->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
			struct drvextra_cmd_parm *extra_parm = (struct drvextra_cmd_parm *)cmd_obj->parmbuf;

			if (extra_parm->pbuf && extra_parm->size > 0)
				rtw_mfree(extra_parm->pbuf, extra_parm->size);
		}
		rtw_free_cmd_obj(cmd_obj);
		goto exit;
	}

	res = _rtw_enqueue_cmd(&pcmdpriv->cmd_queue, cmd_obj, 0);

	if (res == _SUCCESS)
		up(&pcmdpriv->cmd_queue_sema);

exit:


	return res;
}

struct	cmd_obj	*rtw_dequeue_cmd(struct cmd_priv *pcmdpriv)
{
	struct cmd_obj *cmd_obj;


	cmd_obj = _rtw_dequeue_cmd(&pcmdpriv->cmd_queue);

	return cmd_obj;
}

void rtw_cmd_clr_isr(struct	cmd_priv *pcmdpriv)
{
	pcmdpriv->cmd_done_cnt++;
}

void rtw_free_cmd_obj(struct cmd_obj *pcmd)
{
	if (pcmd->parmbuf) {
		/* free parmbuf in cmd_obj */
		rtw_mfree((unsigned char *)pcmd->parmbuf, pcmd->cmdsz);
	}
	if (pcmd->rsp) {
		if (pcmd->rspsz != 0) {
			/* free rsp in cmd_obj */
			rtw_mfree((unsigned char *)pcmd->rsp, pcmd->rspsz);
		}
	}
	/* free cmd_obj */
	rtw_mfree((unsigned char *)pcmd, sizeof(struct cmd_obj));
}

void rtw_stop_cmd_thread(struct adapter *adapter)
{
	if (adapter->cmdThread) {
		up(&adapter->cmdpriv.cmd_queue_sema);
		rtw_thread_stop(adapter->cmdThread);
		adapter->cmdThread = NULL;
	}
}

int rtw_cmd_thread(void *context)
{
	u8 ret;
	struct cmd_obj *pcmd;
	u8 *pcmdbuf, *prspbuf;
	unsigned long cmd_start_time;
	u32 cmd_process_time;
	u8(*cmd_hdl)(struct adapter *adapt, u8 *pbuf);
	void (*pcmd_callback)(struct adapter *dev, struct cmd_obj *pcmd);
	struct adapter * adapt = (struct adapter *)context;
	struct cmd_priv *pcmdpriv = &(adapt->cmdpriv);
	struct drvextra_cmd_parm *extra_parm = NULL;
	unsigned long irqL;

	thread_enter("RTW_CMD_THREAD");

	pcmdbuf = pcmdpriv->cmd_buf;
	prspbuf = pcmdpriv->rsp_buf;
	ATOMIC_SET(&(pcmdpriv->cmdthd_running), true);
	up(&pcmdpriv->start_cmdthread_sema);


	while (1) {
		if (_rtw_down_sema(&pcmdpriv->cmd_queue_sema) == _FAIL) {
			RTW_PRINT(FUNC_ADPT_FMT" _rtw_down_sema(&pcmdpriv->cmd_queue_sema) return _FAIL, break\n", FUNC_ADPT_ARG(adapt));
			break;
		}

		if (RTW_CANNOT_RUN(adapt)) {
			RTW_DBG(FUNC_ADPT_FMT "- bDriverStopped(%s) bSurpriseRemoved(%s)\n",
				FUNC_ADPT_ARG(adapt),
				rtw_is_drv_stopped(adapt) ? "True" : "False",
				rtw_is_surprise_removed(adapt) ? "True" : "False");
			break;
		}

		_enter_critical(&pcmdpriv->cmd_queue.lock, &irqL);
		if (rtw_is_list_empty(&(pcmdpriv->cmd_queue.queue))) {
			/* RTW_INFO("%s: cmd queue is empty!\n", __func__); */
			_exit_critical(&pcmdpriv->cmd_queue.lock, &irqL);
			continue;
		}
		_exit_critical(&pcmdpriv->cmd_queue.lock, &irqL);

_next:
		if (RTW_CANNOT_RUN(adapt)) {
			RTW_PRINT("%s: DriverStopped(%s) SurpriseRemoved(%s) break at line %d\n",
				  __func__
				, rtw_is_drv_stopped(adapt) ? "True" : "False"
				, rtw_is_surprise_removed(adapt) ? "True" : "False"
				  , __LINE__);
			break;
		}

		pcmd = rtw_dequeue_cmd(pcmdpriv);
		if (!pcmd)
			continue;

		cmd_start_time = rtw_get_current_time();
		pcmdpriv->cmd_issued_cnt++;

		if (pcmd->cmdsz > MAX_CMDSZ) {
			RTW_ERR("%s cmdsz:%d > MAX_CMDSZ:%d\n", __func__, pcmd->cmdsz, MAX_CMDSZ);
			pcmd->res = H2C_PARAMETERS_ERROR;
			goto post_process;
		}

		if (pcmd->cmdcode >= (sizeof(wlancmds) / sizeof(struct cmd_hdl))) {
			RTW_ERR("%s undefined cmdcode:%d\n", __func__, pcmd->cmdcode);
			pcmd->res = H2C_PARAMETERS_ERROR;
			goto post_process;
		}

		cmd_hdl = wlancmds[pcmd->cmdcode].h2cfuns;
		if (!cmd_hdl) {
			RTW_ERR("%s no cmd_hdl for cmdcode:%d\n", __func__, pcmd->cmdcode);
			pcmd->res = H2C_PARAMETERS_ERROR;
			goto post_process;
		}

		if (_FAIL == rtw_cmd_filter(pcmdpriv, pcmd)) {
			pcmd->res = H2C_DROPPED;
			if (pcmd->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
				extra_parm = (struct drvextra_cmd_parm *)pcmd->parmbuf;
				if (extra_parm && extra_parm->pbuf && extra_parm->size > 0)
					rtw_mfree(extra_parm->pbuf, extra_parm->size);
			}
			goto post_process;
		}

		if (DBG_CMD_EXECUTE)
			RTW_INFO(ADPT_FMT" "CMD_FMT" %sexecute\n", ADPT_ARG(pcmd->adapt), CMD_ARG(pcmd)
				, pcmd->res == H2C_ENQ_HEAD ? "ENQ_HEAD " : (pcmd->res == H2C_ENQ_HEAD_FAIL ? "ENQ_HEAD_FAIL " : ""));

		memcpy(pcmdbuf, pcmd->parmbuf, pcmd->cmdsz);
		ret = cmd_hdl(pcmd->adapt, pcmdbuf);
		pcmd->res = ret;

		pcmdpriv->cmd_seq++;

post_process:

		_enter_critical_mutex(&(pcmd->adapt->cmdpriv.sctx_mutex), NULL);
		if (pcmd->sctx) {
			if (pcmd->res == H2C_SUCCESS)
				rtw_sctx_done(&pcmd->sctx);
			else
				rtw_sctx_done_err(&pcmd->sctx, RTW_SCTX_DONE_CMD_ERROR);
		}
		_exit_critical_mutex(&(pcmd->adapt->cmdpriv.sctx_mutex), NULL);

		cmd_process_time = rtw_get_passing_time_ms(cmd_start_time);
		if (cmd_process_time > 1000) {
			RTW_INFO(ADPT_FMT" "CMD_FMT" process_time=%d\n", ADPT_ARG(pcmd->adapt), CMD_ARG(pcmd), cmd_process_time);
		}

		/* call callback function for post-processed */
		if (pcmd->cmdcode < (sizeof(rtw_cmd_callback) / sizeof(struct _cmd_callback))) {
			pcmd_callback = rtw_cmd_callback[pcmd->cmdcode].callback;
			if (!pcmd_callback) {
				rtw_free_cmd_obj(pcmd);
			} else {
				/* todo: !!! fill rsp_buf to pcmd->rsp if (pcmd->rsp!=NULL) */
				pcmd_callback(pcmd->adapt, pcmd);/* need conider that free cmd_obj in rtw_cmd_callback */
			}
		} else {
			rtw_free_cmd_obj(pcmd);
		}

		flush_signals_thread();

		goto _next;

	}
	/* to avoid enqueue cmd after free all cmd_obj */
	ATOMIC_SET(&(pcmdpriv->cmdthd_running), false);

	/* free all cmd_obj resources */
	do {
		pcmd = rtw_dequeue_cmd(pcmdpriv);
		if (!pcmd)
			break;

		if (pcmd->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
			extra_parm = (struct drvextra_cmd_parm *)pcmd->parmbuf;
			if (extra_parm->pbuf && extra_parm->size > 0)
				rtw_mfree(extra_parm->pbuf, extra_parm->size);
		}

		_enter_critical_mutex(&(pcmd->adapt->cmdpriv.sctx_mutex), NULL);
		if (pcmd->sctx) {
			rtw_sctx_done_err(&pcmd->sctx, RTW_SCTX_DONE_CMD_DROP);
		}
		_exit_critical_mutex(&(pcmd->adapt->cmdpriv.sctx_mutex), NULL);

		rtw_free_cmd_obj(pcmd);
	} while (1);

	RTW_INFO(FUNC_ADPT_FMT " Exit\n", FUNC_ADPT_ARG(adapt));

	rtw_thread_wait_stop();

	return 0;
}


#ifdef CONFIG_EVENT_THREAD_MODE
u32 rtw_enqueue_evt(struct evt_priv *pevtpriv, struct evt_obj *obj)
{
	unsigned long irqL;
	int	res;
	_queue *queue = &pevtpriv->evt_queue;


	res = _SUCCESS;

	if (!obj) {
		res = _FAIL;
		goto exit;
	}

	_enter_critical_bh(&queue->lock, &irqL);

	list_add_tail(&obj->list, &queue->queue);

	_exit_critical_bh(&queue->lock, &irqL);

	/* rtw_evt_notify_isr(pevtpriv); */

exit:


	return res;
}

struct evt_obj *rtw_dequeue_evt(_queue *queue)
{
	unsigned long irqL;
	struct	evt_obj	*pevtobj;


	_enter_critical_bh(&queue->lock, &irqL);

	if (rtw_is_list_empty(&(queue->queue)))
		pevtobj = NULL;
	else {
		pevtobj = container_of(get_next(&(queue->queue)), struct evt_obj, list);
		rtw_list_delete(&pevtobj->list);
	}

	_exit_critical_bh(&queue->lock, &irqL);


	return pevtobj;
}

void rtw_free_evt_obj(struct evt_obj *pevtobj)
{

	if (pevtobj->parmbuf)
		rtw_mfree((unsigned char *)pevtobj->parmbuf, pevtobj->evtsz);

	rtw_mfree((unsigned char *)pevtobj, sizeof(struct evt_obj));

}

void rtw_evt_notify_isr(struct evt_priv *pevtpriv)
{
	pevtpriv->evt_done_cnt++;
	up(&(pevtpriv->evt_notify));
}
#endif


/*
u8 rtw_setstandby_cmd(unsigned char  *adapter)
*/
u8 rtw_setstandby_cmd(struct adapter *adapt, uint action)
{
	struct cmd_obj			*ph2c;
	struct usb_suspend_parm	*psetusbsuspend;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;

	u8 ret = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		ret = _FAIL;
		goto exit;
	}

	psetusbsuspend = (struct usb_suspend_parm *)rtw_zmalloc(sizeof(struct usb_suspend_parm));
	if (!psetusbsuspend) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		ret = _FAIL;
		goto exit;
	}

	psetusbsuspend->action = action;

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetusbsuspend, GEN_CMD_CODE(_SetUsbSuspend));

	ret = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return ret;
}

void rtw_init_sitesurvey_parm(struct adapter *adapt, struct sitesurvey_parm *pparm)
{
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;


	memset(pparm, 0, sizeof(struct sitesurvey_parm));
	pparm->scan_mode = pmlmepriv->scan_mode;
}

/*
rtw_sitesurvey_cmd(~)
	### NOTE:#### (!!!!)
	MUST TAKE CARE THAT BEFORE CALLING THIS FUNC, YOU SHOULD HAVE LOCKED pmlmepriv->lock
*/
u8 rtw_sitesurvey_cmd(struct adapter *adapt, struct sitesurvey_parm *pparm)
{
	u8 res = _FAIL;
	struct cmd_obj		*ph2c;
	struct sitesurvey_parm	*psurveyPara;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;

	if (check_fwstate(pmlmepriv, _FW_LINKED))
		rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_SCAN, 1);

	if (check_fwstate(pmlmepriv, _FW_LINKED))
		p2p_ps_wk_cmd(adapt, P2P_PS_SCAN, 1);

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c)
		return _FAIL;

	psurveyPara = (struct sitesurvey_parm *)rtw_zmalloc(sizeof(struct sitesurvey_parm));
	if (!psurveyPara) {
		rtw_mfree((unsigned char *) ph2c, sizeof(struct cmd_obj));
		return _FAIL;
	}

	if (pparm)
		memcpy(psurveyPara, pparm, sizeof(struct sitesurvey_parm));
	else
		psurveyPara->scan_mode = pmlmepriv->scan_mode;

	rtw_free_network_queue(adapt, false);

	init_h2fwcmd_w_parm_no_rsp(ph2c, psurveyPara, GEN_CMD_CODE(_SiteSurvey));

	set_fwstate(pmlmepriv, _FW_UNDER_SURVEY);

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

	if (res == _SUCCESS) {
		u32 scan_timeout_ms;

		pmlmepriv->scan_start_time = rtw_get_current_time();
		scan_timeout_ms = rtw_scan_timeout_decision(adapt);
		mlme_set_scan_to_timer(pmlmepriv,scan_timeout_ms);

		rtw_led_control(adapt, LED_CTL_SITE_SURVEY);
	} else
		_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY);


	return res;
}

u8 rtw_setdatarate_cmd(struct adapter *adapt, u8 *rateset)
{
	struct cmd_obj			*ph2c;
	struct setdatarate_parm	*pbsetdataratepara;
	struct cmd_priv		*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pbsetdataratepara = (struct setdatarate_parm *)rtw_zmalloc(sizeof(struct setdatarate_parm));
	if (!pbsetdataratepara) {
		rtw_mfree((u8 *) ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pbsetdataratepara, GEN_CMD_CODE(_SetDataRate));
	pbsetdataratepara->mac_id = 5;
	memcpy(pbsetdataratepara->datarates, rateset, NumRates);
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:


	return res;
}

u8 rtw_setbasicrate_cmd(struct adapter *adapt, u8 *rateset)
{
	struct cmd_obj			*ph2c;
	struct setbasicrate_parm	*pssetbasicratepara;
	struct cmd_priv		*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	pssetbasicratepara = (struct setbasicrate_parm *)rtw_zmalloc(sizeof(struct setbasicrate_parm));

	if (!pssetbasicratepara) {
		rtw_mfree((u8 *) ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pssetbasicratepara, _SetBasicRate_CMD_);

	memcpy(pssetbasicratepara->basicrates, rateset, NumRates);

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:


	return res;
}


/*
unsigned char rtw_setphy_cmd(unsigned char  *adapter)

1.  be called only after rtw_update_registrypriv_dev_network( ~) or mp testing program
2.  for AdHoc/Ap mode or mp mode?

*/
u8 rtw_setphy_cmd(struct adapter *adapt, u8 modem, u8 ch)
{
	struct cmd_obj			*ph2c;
	struct setphy_parm		*psetphypara;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	/*	struct mlme_priv			*pmlmepriv = &adapt->mlmepriv;
	 *	struct registry_priv*		pregistry_priv = &adapt->registrypriv; */
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	psetphypara = (struct setphy_parm *)rtw_zmalloc(sizeof(struct setphy_parm));

	if (!psetphypara) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetphypara, _SetPhy_CMD_);


	psetphypara->modem = modem;
	psetphypara->rfchannel = ch;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:
	return res;
}

u8 rtw_getmacreg_cmd(struct adapter *adapt, u8 len, u32 addr)
{
	struct cmd_obj *ph2c;
	struct readMAC_parm *preadmacparm;
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	preadmacparm = (struct readMAC_parm *)rtw_zmalloc(sizeof(struct readMAC_parm));

	if (!preadmacparm) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, preadmacparm, GEN_CMD_CODE(_GetMACReg));

	preadmacparm->len = len;
	preadmacparm->addr = addr;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}

void rtw_usb_catc_trigger_cmd(struct adapter *adapt, const char *caller)
{
	RTW_INFO("%s caller:%s\n", __func__, caller);
	rtw_getmacreg_cmd(adapt, 1, 0x1c4);
}

u8 rtw_setbbreg_cmd(struct adapter *adapt, u8 offset, u8 val)
{
	struct cmd_obj			*ph2c;
	struct writeBB_parm		*pwritebbparm;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;
	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	pwritebbparm = (struct writeBB_parm *)rtw_zmalloc(sizeof(struct writeBB_parm));

	if (!pwritebbparm) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pwritebbparm, GEN_CMD_CODE(_SetBBReg));

	pwritebbparm->offset = offset;
	pwritebbparm->value = val;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:
	return res;
}

u8 rtw_getbbreg_cmd(struct adapter  *adapt, u8 offset, u8 *pval)
{
	struct cmd_obj			*ph2c;
	struct readBB_parm		*prdbbparm;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	prdbbparm = (struct readBB_parm *)rtw_zmalloc(sizeof(struct readBB_parm));

	if (!prdbbparm) {
		rtw_mfree((unsigned char *) ph2c, sizeof(struct	cmd_obj));
		return _FAIL;
	}

	INIT_LIST_HEAD(&ph2c->list);
	ph2c->cmdcode = GEN_CMD_CODE(_GetBBReg);
	ph2c->parmbuf = (unsigned char *)prdbbparm;
	ph2c->cmdsz =  sizeof(struct readBB_parm);
	ph2c->rsp = pval;
	ph2c->rspsz = sizeof(struct readBB_rsp);

	prdbbparm->offset = offset;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:
	return res;
}

u8 rtw_setrfreg_cmd(struct adapter  *adapt, u8 offset, u32 val)
{
	struct cmd_obj			*ph2c;
	struct writeRF_parm		*pwriterfparm;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;
	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	pwriterfparm = (struct writeRF_parm *)rtw_zmalloc(sizeof(struct writeRF_parm));

	if (!pwriterfparm) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pwriterfparm, GEN_CMD_CODE(_SetRFReg));

	pwriterfparm->offset = offset;
	pwriterfparm->value = val;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:
	return res;
}

u8 rtw_getrfreg_cmd(struct adapter  *adapt, u8 offset, u8 *pval)
{
	struct cmd_obj			*ph2c;
	struct readRF_parm		*prdrfparm;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	prdrfparm = (struct readRF_parm *)rtw_zmalloc(sizeof(struct readRF_parm));
	if (!prdrfparm) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	INIT_LIST_HEAD(&ph2c->list);
	ph2c->cmdcode = GEN_CMD_CODE(_GetRFReg);
	ph2c->parmbuf = (unsigned char *)prdrfparm;
	ph2c->cmdsz =  sizeof(struct readRF_parm);
	ph2c->rsp = pval;
	ph2c->rspsz = sizeof(struct readRF_rsp);

	prdrfparm->offset = offset;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;
}

void rtw_getbbrfreg_cmdrsp_callback(struct adapter	*adapt,  struct cmd_obj *pcmd)
{
	/* rtw_free_cmd_obj(pcmd); */
	rtw_mfree((unsigned char *) pcmd->parmbuf, pcmd->cmdsz);
	rtw_mfree((unsigned char *) pcmd, sizeof(struct cmd_obj));
}

void rtw_readtssi_cmdrsp_callback(struct adapter	*adapt,  struct cmd_obj *pcmd)
{
	rtw_mfree((unsigned char *) pcmd->parmbuf, pcmd->cmdsz);
	rtw_mfree((unsigned char *) pcmd, sizeof(struct cmd_obj));
}

static u8 rtw_createbss_cmd(struct adapter  *adapter, int flags, bool adhoc
			    , s16 req_ch, s8 req_bw, s8 req_offset)
{
	struct cmd_obj *cmdobj;
	struct createbss_parm *parm;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	struct submit_ctx sctx;
	u8 res = _SUCCESS;

	if (req_ch > 0 && req_bw >= 0 && req_offset >= 0) {
		if (!rtw_chset_is_chbw_valid(adapter_to_chset(adapter), req_ch, req_bw, req_offset)) {
			res = _FAIL;
			goto exit;
		}
	}

	/* prepare cmd parameter */
	parm = (struct createbss_parm *)rtw_zmalloc(sizeof(*parm));
	if (!parm) {
		res = _FAIL;
		goto exit;
	}

	if (adhoc) {
		/* for now, adhoc doesn't support ch,bw,offset request */
		parm->adhoc = 1;
	} else {
		parm->adhoc = 0;
		parm->req_ch = req_ch;
		parm->req_bw = req_bw;
		parm->req_offset = req_offset;
	}

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != createbss_hdl(adapter, (u8 *)parm))
			res = _FAIL;
		rtw_mfree((u8 *)parm, sizeof(*parm));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (!cmdobj) {
			res = _FAIL;
			rtw_mfree((u8 *)parm, sizeof(*parm));
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(cmdobj, parm, GEN_CMD_CODE(_CreateBss));

		if (flags & RTW_CMDF_WAIT_ACK) {
			cmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 2000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, cmdobj);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				cmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		}
	}

exit:
	return res;
}

inline u8 rtw_create_ibss_cmd(struct adapter *adapter, int flags)
{
	return rtw_createbss_cmd(adapter, flags
		, 1
		, 0, -1, -1 /* for now, adhoc doesn't support ch,bw,offset request */
	);
}

inline u8 rtw_startbss_cmd(struct adapter *adapter, int flags)
{
	return rtw_createbss_cmd(adapter, flags
		, 0
		, 0, -1, -1 /* excute entire AP setup cmd */
	);
}

inline u8 rtw_change_bss_chbw_cmd(struct adapter *adapter, int flags, s16 req_ch, s8 req_bw, s8 req_offset)
{
	return rtw_createbss_cmd(adapter, flags
		, 0
		, req_ch, req_bw, req_offset
	);
}

#ifdef CONFIG_RTW_80211R
static void rtw_ft_validate_akm_type(struct adapter  *adapt,
	struct wlan_network *pnetwork)
{
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	struct ft_roam_info *pft_roam = &(adapt->mlmepriv.ft_roam);
	u32 tmp_len;
	u8 *ptmp;

	/*IEEE802.11-2012 Std. Table 8-101¡XAKM suite selectors*/
	if (rtw_ft_valid_akm(adapt, psecuritypriv->rsn_akm_suite_type)) {
		ptmp = rtw_get_ie(&pnetwork->network.IEs[12], 
				_MDIE_, &tmp_len, (pnetwork->network.IELength-12));
		if (ptmp) {
			pft_roam->mdid = *(u16 *)(ptmp+2);
			pft_roam->ft_cap = *(ptmp+4);

			RTW_INFO("FT: target " MAC_FMT " mdid=(0x%2x), capacity=(0x%2x)\n", 
				MAC_ARG(pnetwork->network.MacAddress), pft_roam->mdid, pft_roam->ft_cap);
			rtw_ft_set_flags(adapt, RTW_FT_PEER_EN);

			if (rtw_ft_otd_roam_en(adapt))
				rtw_ft_set_flags(adapt, RTW_FT_PEER_OTD_EN);
		} else {
			/* Don't use FT roaming if target AP cannot support FT */
			rtw_ft_clr_flags(adapt, (RTW_FT_PEER_EN|RTW_FT_PEER_OTD_EN));
			rtw_ft_reset_status(adapt);
		}
	} else {
		/* It could be a non-FT connection */
		rtw_ft_clr_flags(adapt, (RTW_FT_PEER_EN|RTW_FT_PEER_OTD_EN));
		rtw_ft_reset_status(adapt);
	}	
}
#endif

u8 rtw_joinbss_cmd(struct adapter  *adapt, struct wlan_network *pnetwork)
{
	u8	*auth, res = _SUCCESS;
	uint	t_len = 0;
	struct wlan_bssid_ex		*psecnetwork;
	struct cmd_obj		*pcmd;
	struct cmd_priv		*pcmdpriv = &adapt->cmdpriv;
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct qos_priv		*pqospriv = &pmlmepriv->qospriv;
	struct security_priv	*psecuritypriv = &adapt->securitypriv;
	struct registry_priv	*pregistrypriv = &adapt->registrypriv;
	struct ht_priv			*phtpriv = &pmlmepriv->htpriv;
	enum ndis_802_11_network_infrastructure ndis_network_mode = pnetwork->network.InfrastructureMode;
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u32 tmp_len;
	u8 *ptmp = NULL;

	rtw_led_control(adapt, LED_CTL_START_TO_LINK);

	pcmd = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!pcmd) {
		res = _FAIL;
		goto exit;
	}
	/* for IEs is fix buf size */
	t_len = sizeof(struct wlan_bssid_ex);


	/* for hidden ap to set fw_state here */
	if (!check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_ADHOC_STATE)) {
		switch (ndis_network_mode) {
		case Ndis802_11IBSS:
			set_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
			break;

		case Ndis802_11Infrastructure:
			set_fwstate(pmlmepriv, WIFI_STATION_STATE);
			break;

		default:
			rtw_warn_on(1);
			break;
		}
	}

	pmlmeinfo->assoc_AP_vendor = check_assoc_AP(pnetwork->network.IEs, pnetwork->network.IELength);

	/*
		Modified by Arvin 2015/05/13
		Solution for allocating a new struct wlan_bssid_ex to avoid race condition issue between disconnect and joinbss
	*/
	psecnetwork = (struct wlan_bssid_ex *)rtw_zmalloc(sizeof(struct wlan_bssid_ex));
	if (!psecnetwork) {
		if (pcmd)
			rtw_mfree((unsigned char *)pcmd, sizeof(struct	cmd_obj));

		res = _FAIL;


		goto exit;
	}

	memset(psecnetwork, 0, t_len);

	memcpy(psecnetwork, &pnetwork->network, get_wlan_bssid_ex_sz(&pnetwork->network));

	auth = &psecuritypriv->authenticator_ie[0];
	psecuritypriv->authenticator_ie[0] = (unsigned char)psecnetwork->IELength;

	if ((psecnetwork->IELength - 12) < (256 - 1))
		memcpy(&psecuritypriv->authenticator_ie[1], &psecnetwork->IEs[12], psecnetwork->IELength - 12);
	else
		memcpy(&psecuritypriv->authenticator_ie[1], &psecnetwork->IEs[12], (256 - 1));

	psecnetwork->IELength = 0;
	/* Added by Albert 2009/02/18 */
	/* If the the driver wants to use the bssid to create the connection. */
	/* If not,  we have to copy the connecting AP's MAC address to it so that */
	/* the driver just has the bssid information for PMKIDList searching. */

	if (!pmlmepriv->assoc_by_bssid)
		memcpy(&pmlmepriv->assoc_bssid[0], &pnetwork->network.MacAddress[0], ETH_ALEN);

	/* copy fixed ie */
	memcpy(psecnetwork->IEs, pnetwork->network.IEs, 12);
	psecnetwork->IELength = 12;

	psecnetwork->IELength += rtw_restruct_sec_ie(adapt, psecnetwork->IEs + psecnetwork->IELength);


	pqospriv->qos_option = 0;

	if (pregistrypriv->wmm_enable) {
		tmp_len = rtw_restruct_wmm_ie(adapt, &pnetwork->network.IEs[0], &psecnetwork->IEs[0], pnetwork->network.IELength, psecnetwork->IELength);

		if (psecnetwork->IELength != tmp_len) {
			psecnetwork->IELength = tmp_len;
			pqospriv->qos_option = 1; /* There is WMM IE in this corresp. beacon */
		} else {
			pqospriv->qos_option = 0;/* There is no WMM IE in this corresp. beacon */
		}
	}

	phtpriv->ht_option = false;
	ptmp = rtw_get_ie(&pnetwork->network.IEs[12], _HT_CAPABILITY_IE_, &tmp_len, pnetwork->network.IELength - 12);
	if (pregistrypriv->ht_enable && ptmp && tmp_len > 0) {
		/*	Added by Albert 2010/06/23 */
		/*	For the WEP mode, we will use the bg mode to do the connection to avoid some IOT issue. */
		/*	Especially for Realtek 8192u SoftAP. */
		if ((adapt->securitypriv.dot11PrivacyAlgrthm != _WEP40_) &&
		    (adapt->securitypriv.dot11PrivacyAlgrthm != _WEP104_) &&
		    (adapt->securitypriv.dot11PrivacyAlgrthm != _TKIP_)) {
			rtw_ht_use_default_setting(adapt);

			/* rtw_restructure_ht_ie */
			rtw_restructure_ht_ie(adapt, &pnetwork->network.IEs[12], &psecnetwork->IEs[0],
				pnetwork->network.IELength - 12, &psecnetwork->IELength,
				pnetwork->network.Configuration.DSConfig);
		}
	}

	rtw_append_exented_cap(adapt, &psecnetwork->IEs[0], &psecnetwork->IELength);

#ifdef CONFIG_RTW_80211R
	rtw_ft_validate_akm_type(adapt, pnetwork);
#endif

	pcmd->cmdsz = sizeof(struct wlan_bssid_ex);

	INIT_LIST_HEAD(&pcmd->list);
	pcmd->cmdcode = _JoinBss_CMD_;/* GEN_CMD_CODE(_JoinBss) */
	pcmd->parmbuf = (unsigned char *)psecnetwork;
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;

	res = rtw_enqueue_cmd(pcmdpriv, pcmd);

exit:


	return res;
}

u8 rtw_disassoc_cmd(struct adapter *adapt, u32 deauth_timeout_ms, int flags) /* for sta_mode */
{
	struct cmd_obj *cmdobj = NULL;
	struct disconnect_parm *param = NULL;
	struct cmd_priv *cmdpriv = &adapt->cmdpriv;
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;
	struct submit_ctx sctx;
	u8 res = _SUCCESS;



	/* prepare cmd parameter */
	param = (struct disconnect_parm *)rtw_zmalloc(sizeof(*param));
	if (!param) {
		res = _FAIL;
		goto exit;
	}
	param->deauth_timeout_ms = deauth_timeout_ms;

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (disconnect_hdl(adapt, (u8 *)param) != H2C_SUCCESS)
			res = _FAIL;
		rtw_mfree((u8 *)param, sizeof(*param));

	} else {
		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (!cmdobj) {
			res = _FAIL;
			rtw_mfree((u8 *)param, sizeof(*param));
			goto exit;
		}
		init_h2fwcmd_w_parm_no_rsp(cmdobj, param, _DisConnect_CMD_);
		if (flags & RTW_CMDF_WAIT_ACK) {
			cmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 2000);
		}
		res = rtw_enqueue_cmd(cmdpriv, cmdobj);
		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				cmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		}
	}

exit:


	return res;
}

u8 rtw_setopmode_cmd(struct adapter  *adapter, enum ndis_802_11_network_infrastructure networktype, u8 flags)
{
	struct cmd_obj *cmdobj;
	struct setopmode_parm *parm;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	struct submit_ctx sctx;
	u8 res = _SUCCESS;

	/* prepare cmd parameter */
	parm = (struct setopmode_parm *)rtw_zmalloc(sizeof(*parm));
	if (!parm) {
		res = _FAIL;
		goto exit;
	}
	parm->mode = (u8)networktype;

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != setopmode_hdl(adapter, (u8 *)parm))
			res = _FAIL;
		rtw_mfree((u8 *)parm, sizeof(*parm));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (!cmdobj) {
			res = _FAIL;
			rtw_mfree((u8 *)parm, sizeof(*parm));
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(cmdobj, parm, _SetOpMode_CMD_);

		if (flags & RTW_CMDF_WAIT_ACK) {
			cmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 2000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, cmdobj);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				cmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		}
	}

exit:
	return res;
}

u8 rtw_setstakey_cmd(struct adapter *adapt, struct sta_info *sta, u8 key_type, bool enqueue)
{
	struct cmd_obj			*ph2c;
	struct set_stakey_parm	*psetstakey_para;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	struct set_stakey_rsp		*psetstakey_rsp = NULL;

	struct mlme_priv			*pmlmepriv = &adapt->mlmepriv;
	struct security_priv		*psecuritypriv = &adapt->securitypriv;
	u8	res = _SUCCESS;


	psetstakey_para = (struct set_stakey_parm *)rtw_zmalloc(sizeof(struct set_stakey_parm));
	if (!psetstakey_para) {
		res = _FAIL;
		goto exit;
	}

	memcpy(psetstakey_para->addr, sta->cmn.mac_addr, ETH_ALEN);

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE))
		psetstakey_para->algorithm = (unsigned char) psecuritypriv->dot11PrivacyAlgrthm;
	else
		GET_ENCRY_ALGO(psecuritypriv, sta, psetstakey_para->algorithm, false);

	if (key_type == GROUP_KEY)
		memcpy(&psetstakey_para->key, &psecuritypriv->dot118021XGrpKey[psecuritypriv->dot118021XGrpKeyid].skey, 16);
	else if (key_type == UNICAST_KEY)
		memcpy(&psetstakey_para->key, &sta->dot118021x_UncstKey, 16);
	/* jeff: set this becasue at least sw key is ready */
	adapt->securitypriv.busetkipkey = true;

	if (enqueue) {
		ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
		if (!ph2c) {
			rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
			res = _FAIL;
			goto exit;
		}

		psetstakey_rsp = (struct set_stakey_rsp *)rtw_zmalloc(sizeof(struct set_stakey_rsp));
		if (!psetstakey_rsp) {
			rtw_mfree((u8 *) ph2c, sizeof(struct cmd_obj));
			rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
			res = _FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(ph2c, psetstakey_para, _SetStaKey_CMD_);
		ph2c->rsp = (u8 *) psetstakey_rsp;
		ph2c->rspsz = sizeof(struct set_stakey_rsp);
		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	} else {
		set_stakey_hdl(adapt, (u8 *)psetstakey_para);
		rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
	}
exit:


	return res;
}

u8 rtw_clearstakey_cmd(struct adapter *adapt, struct sta_info *sta, u8 enqueue)
{
	struct cmd_obj			*ph2c;
	struct set_stakey_parm	*psetstakey_para;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	struct set_stakey_rsp		*psetstakey_rsp = NULL;
	s16 cam_id = 0;
	u8	res = _SUCCESS;

	if (!sta) {
		RTW_ERR("%s sta == NULL\n", __func__);
		goto exit;
	}

	if (!enqueue) {
		while ((cam_id = rtw_camid_search(adapt, sta->cmn.mac_addr, -1, -1)) >= 0) {
			RTW_PRINT("clear key for addr:"MAC_FMT", camid:%d\n", MAC_ARG(sta->cmn.mac_addr), cam_id);
			clear_cam_entry(adapt, cam_id);
			rtw_camid_free(adapt, cam_id);
		}
	} else {
		ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
		if (!ph2c) {
			res = _FAIL;
			goto exit;
		}

		psetstakey_para = (struct set_stakey_parm *)rtw_zmalloc(sizeof(struct set_stakey_parm));
		if (!psetstakey_para) {
			rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
			res = _FAIL;
			goto exit;
		}

		psetstakey_rsp = (struct set_stakey_rsp *)rtw_zmalloc(sizeof(struct set_stakey_rsp));
		if (!psetstakey_rsp) {
			rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
			rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
			res = _FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(ph2c, psetstakey_para, _SetStaKey_CMD_);
		ph2c->rsp = (u8 *) psetstakey_rsp;
		ph2c->rspsz = sizeof(struct set_stakey_rsp);

		memcpy(psetstakey_para->addr, sta->cmn.mac_addr, ETH_ALEN);

		psetstakey_para->algorithm = _NO_PRIVACY_;

		res = rtw_enqueue_cmd(pcmdpriv, ph2c);

	}

exit:


	return res;
}

u8 rtw_setrttbl_cmd(struct adapter  *adapt, struct setratable_parm *prate_table)
{
	struct cmd_obj			*ph2c;
	struct setratable_parm	*psetrttblparm;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	psetrttblparm = (struct setratable_parm *)rtw_zmalloc(sizeof(struct setratable_parm));

	if (!psetrttblparm) {
		rtw_mfree((unsigned char *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetrttblparm, GEN_CMD_CODE(_SetRaTable));

	memcpy(psetrttblparm, prate_table, sizeof(struct setratable_parm));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:
	return res;

}

u8 rtw_getrttbl_cmd(struct adapter  *adapt, struct getratable_rsp *pval)
{
	struct cmd_obj			*ph2c;
	struct getratable_parm	*pgetrttblparm;
	struct cmd_priv			*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}
	pgetrttblparm = (struct getratable_parm *)rtw_zmalloc(sizeof(struct getratable_parm));

	if (!pgetrttblparm) {
		rtw_mfree((unsigned char *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	/*	init_h2fwcmd_w_parm_no_rsp(ph2c, psetrttblparm, GEN_CMD_CODE(_SetRaTable)); */

	INIT_LIST_HEAD(&ph2c->list);
	ph2c->cmdcode = GEN_CMD_CODE(_GetRaTable);
	ph2c->parmbuf = (unsigned char *)pgetrttblparm;
	ph2c->cmdsz =  sizeof(struct getratable_parm);
	ph2c->rsp = (u8 *)pval;
	ph2c->rspsz = sizeof(struct getratable_rsp);

	pgetrttblparm->rsvd = 0x0;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:
	return res;

}

u8 rtw_setassocsta_cmd(struct adapter  *adapt, u8 *mac_addr)
{
	struct cmd_priv		*pcmdpriv = &adapt->cmdpriv;
	struct cmd_obj			*ph2c;
	struct set_assocsta_parm	*psetassocsta_para;
	struct set_stakey_rsp		*psetassocsta_rsp = NULL;

	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	psetassocsta_para = (struct set_assocsta_parm *)rtw_zmalloc(sizeof(struct set_assocsta_parm));
	if (!psetassocsta_para) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	psetassocsta_rsp = (struct set_stakey_rsp *)rtw_zmalloc(sizeof(struct set_assocsta_rsp));
	if (!psetassocsta_rsp) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		rtw_mfree((u8 *) psetassocsta_para, sizeof(struct set_assocsta_parm));
		return _FAIL;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetassocsta_para, _SetAssocSta_CMD_);
	ph2c->rsp = (u8 *) psetassocsta_rsp;
	ph2c->rspsz = sizeof(struct set_assocsta_rsp);

	memcpy(psetassocsta_para->addr, mac_addr, ETH_ALEN);

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;
}

u8 rtw_addbareq_cmd(struct adapter *adapt, u8 tid, u8 *addr)
{
	struct cmd_priv		*pcmdpriv = &adapt->cmdpriv;
	struct cmd_obj		*ph2c;
	struct addBaReq_parm	*paddbareq_parm;

	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	paddbareq_parm = (struct addBaReq_parm *)rtw_zmalloc(sizeof(struct addBaReq_parm));
	if (!paddbareq_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	paddbareq_parm->tid = tid;
	memcpy(paddbareq_parm->addr, addr, ETH_ALEN);

	init_h2fwcmd_w_parm_no_rsp(ph2c, paddbareq_parm, GEN_CMD_CODE(_AddBAReq));

	/* RTW_INFO("rtw_addbareq_cmd, tid=%d\n", tid); */

	/* rtw_enqueue_cmd(pcmdpriv, ph2c);	 */
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;
}

u8 rtw_addbarsp_cmd(struct adapter *adapt, u8 *addr, u16 tid, u8 status, u8 size, u16 start_seq)
{
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;
	struct cmd_obj *ph2c;
	struct addBaRsp_parm *paddBaRsp_parm;
	u8 res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	paddBaRsp_parm = (struct addBaRsp_parm *)rtw_zmalloc(sizeof(struct addBaRsp_parm));

	if (!paddBaRsp_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	memcpy(paddBaRsp_parm->addr, addr, ETH_ALEN);
	paddBaRsp_parm->tid = tid;
	paddBaRsp_parm->status = status;
	paddBaRsp_parm->size = size;
	paddBaRsp_parm->start_seq = start_seq;

	init_h2fwcmd_w_parm_no_rsp(ph2c, paddBaRsp_parm, GEN_CMD_CODE(_AddBARsp));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;
}
/* add for CONFIG_IEEE80211W, none 11w can use it */
u8 rtw_reset_securitypriv_cmd(struct adapter *adapt)
{
	struct cmd_obj		*ph2c;
	struct drvextra_cmd_parm  *pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = RESET_SECURITYPRIV;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));


	/* rtw_enqueue_cmd(pcmdpriv, ph2c);	 */
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;

}

u8 rtw_free_assoc_resources_cmd(struct adapter *adapt)
{
	struct cmd_obj		*ph2c;
	struct drvextra_cmd_parm  *pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = FREE_ASSOC_RESOURCES;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));


	/* rtw_enqueue_cmd(pcmdpriv, ph2c);	 */
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;

}

u8 rtw_dynamic_chk_wk_cmd(struct adapter *adapt)
{
	struct cmd_obj		*ph2c;
	struct drvextra_cmd_parm  *pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	/* only  primary adapt does this cmd */

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = DYNAMIC_CHK_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;
	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));


	/* rtw_enqueue_cmd(pcmdpriv, ph2c);	 */
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;

}

u8 rtw_set_chbw_cmd(struct adapter *adapt, u8 ch, u8 bw, u8 ch_offset, u8 flags)
{
	struct cmd_obj *pcmdobj;
	struct set_ch_parm *set_ch_parm;
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;
	struct submit_ctx sctx;
	u8 res = _SUCCESS;


	RTW_INFO(FUNC_NDEV_FMT" ch:%u, bw:%u, ch_offset:%u\n",
		 FUNC_NDEV_ARG(adapt->pnetdev), ch, bw, ch_offset);

	/* check input parameter */

	/* prepare cmd parameter */
	set_ch_parm = (struct set_ch_parm *)rtw_zmalloc(sizeof(*set_ch_parm));
	if (!set_ch_parm) {
		res = _FAIL;
		goto exit;
	}
	set_ch_parm->ch = ch;
	set_ch_parm->bw = bw;
	set_ch_parm->ch_offset = ch_offset;

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != rtw_set_chbw_hdl(adapt, (u8 *)set_ch_parm))
			res = _FAIL;

		rtw_mfree((u8 *)set_ch_parm, sizeof(*set_ch_parm));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		pcmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
		if (!pcmdobj) {
			rtw_mfree((u8 *)set_ch_parm, sizeof(*set_ch_parm));
			res = _FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(pcmdobj, set_ch_parm, GEN_CMD_CODE(_SetChannel));

		if (flags & RTW_CMDF_WAIT_ACK) {
			pcmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 10 * 1000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				pcmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		}
	}

	/* do something based on res... */

exit:

	RTW_INFO(FUNC_NDEV_FMT" res:%u\n", FUNC_NDEV_ARG(adapt->pnetdev), res);


	return res;
}

static u8 _rtw_set_chplan_cmd(struct adapter *adapter, int flags, u8 chplan, const struct country_chplan *country_ent, u8 swconfig)
{
	struct cmd_obj *cmdobj;
	struct	SetChannelPlan_param *parm;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	struct submit_ctx sctx;
	u8 res = _SUCCESS;


	/* check if allow software config */
	if (swconfig && rtw_hal_is_disable_sw_channel_plan(adapter)) {
		res = _FAIL;
		goto exit;
	}

	/* if country_entry is provided, replace chplan */
	if (country_ent)
		chplan = country_ent->chplan;

	/* check input parameter */
	if (!rtw_is_channel_plan_valid(chplan)) {
		res = _FAIL;
		goto exit;
	}

	/* prepare cmd parameter */
	parm = (struct SetChannelPlan_param *)rtw_zmalloc(sizeof(*parm));
	if (!parm) {
		res = _FAIL;
		goto exit;
	}
	parm->country_ent = country_ent;
	parm->channel_plan = chplan;

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != set_chplan_hdl(adapter, (u8 *)parm))
			res = _FAIL;
		rtw_mfree((u8 *)parm, sizeof(*parm));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (!cmdobj) {
			res = _FAIL;
			rtw_mfree((u8 *)parm, sizeof(*parm));
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(cmdobj, parm, GEN_CMD_CODE(_SetChannelPlan));

		if (flags & RTW_CMDF_WAIT_ACK) {
			cmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 2000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, cmdobj);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				cmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		}
	}

exit:


	return res;
}

inline u8 rtw_set_chplan_cmd(struct adapter *adapter, int flags, u8 chplan, u8 swconfig)
{
	return _rtw_set_chplan_cmd(adapter, flags, chplan, NULL, swconfig);
}

inline u8 rtw_set_country_cmd(struct adapter *adapter, int flags, const char *country_code, u8 swconfig)
{
	const struct country_chplan *ent;

	if (!is_alpha(country_code[0])
	    || !is_alpha(country_code[1])
	   ) {
		RTW_PRINT("%s input country_code is not alpha2\n", __func__);
		return _FAIL;
	}

	ent = rtw_get_chplan_from_country(country_code);

	if (!ent) {
		RTW_PRINT("%s unsupported country_code:\"%c%c\"\n", __func__, country_code[0], country_code[1]);
		return _FAIL;
	}

	RTW_PRINT("%s country_code:\"%c%c\" mapping to chplan:0x%02x\n", __func__, country_code[0], country_code[1], ent->chplan);

	return _rtw_set_chplan_cmd(adapter, flags, RTW_CHPLAN_UNSPECIFIED, ent, swconfig);
}

u8 rtw_led_blink_cmd(struct adapter *adapt, void * pLed)
{
	struct	cmd_obj	*pcmdobj;
	struct	LedBlink_param *ledBlink_param;
	struct	cmd_priv   *pcmdpriv = &adapt->cmdpriv;

	u8	res = _SUCCESS;



	pcmdobj = (struct	cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
	if (!pcmdobj) {
		res = _FAIL;
		goto exit;
	}

	ledBlink_param = (struct	LedBlink_param *)rtw_zmalloc(sizeof(struct	LedBlink_param));
	if (!ledBlink_param) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	ledBlink_param->pLed = pLed;

	init_h2fwcmd_w_parm_no_rsp(pcmdobj, ledBlink_param, GEN_CMD_CODE(_LedBlink));
	res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);

exit:


	return res;
}

u8 rtw_set_csa_cmd(struct adapter *adapt, u8 new_ch_no)
{
	struct	cmd_obj	*pcmdobj;
	struct	SetChannelSwitch_param *setChannelSwitch_param;
	struct	cmd_priv   *pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	pcmdobj = (struct	cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
	if (!pcmdobj) {
		res = _FAIL;
		goto exit;
	}

	setChannelSwitch_param = (struct SetChannelSwitch_param *)rtw_zmalloc(sizeof(struct	SetChannelSwitch_param));
	if (!setChannelSwitch_param) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	setChannelSwitch_param->new_ch_no = new_ch_no;

	init_h2fwcmd_w_parm_no_rsp(pcmdobj, setChannelSwitch_param, GEN_CMD_CODE(_SetChannelSwitch));
	res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);

exit:


	return res;
}

u8 rtw_tdls_cmd(struct adapter *adapt, u8 *addr, u8 option)
{
	u8	res = _SUCCESS;

	return res;
}

u8 rtw_enable_hw_update_tsf_cmd(struct adapter *adapt)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = EN_HW_UPDATE_TSF_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}

/* from_timer == 1 means driver is in LPS */
u8 traffic_status_watchdog(struct adapter *adapt, u8 from_timer)
{
	u8	bEnterPS = false;
	u16 BusyThresholdHigh;
	u16	BusyThresholdLow;
	u16	BusyThreshold;
	u8	bBusyTraffic = false, bTxBusyTraffic = false, bRxBusyTraffic = false;
	u8	bHigherBusyTraffic = false, bHigherBusyRxTraffic = false, bHigherBusyTxTraffic = false;
	struct mlme_priv		*pmlmepriv = &(adapt->mlmepriv);

	BusyThresholdHigh = 100;
	BusyThresholdLow = 75;

	BusyThreshold = BusyThresholdHigh;


	/*  */
	/* Determine if our traffic is busy now */
	/*  */
	if ((check_fwstate(pmlmepriv, _FW_LINKED))
	    /*&& !MgntInitAdapterInProgress(pMgntInfo)*/) {
		/* if we raise bBusyTraffic in last watchdog, using lower threshold. */
		if (pmlmepriv->LinkDetectInfo.bBusyTraffic)
			BusyThreshold = BusyThresholdLow;

		if (pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > BusyThreshold ||
		    pmlmepriv->LinkDetectInfo.NumTxOkInPeriod > BusyThreshold) {
			bBusyTraffic = true;

			if (pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > pmlmepriv->LinkDetectInfo.NumTxOkInPeriod)
				bRxBusyTraffic = true;
			else
				bTxBusyTraffic = true;
		}

		/* Higher Tx/Rx data. */
		if (pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > 4000 ||
		    pmlmepriv->LinkDetectInfo.NumTxOkInPeriod > 4000) {
			bHigherBusyTraffic = true;

			if (pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > pmlmepriv->LinkDetectInfo.NumTxOkInPeriod)
				bHigherBusyRxTraffic = true;
			else
				bHigherBusyTxTraffic = true;
		}

		/* check traffic for  powersaving. */
		if (((pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod + pmlmepriv->LinkDetectInfo.NumTxOkInPeriod) > 8) ||
		    (pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod > 4)
		   ) {
			bEnterPS = false;
		} else {
			bEnterPS = true;
		}

#ifdef CONFIG_DYNAMIC_DTIM
		if (pmlmepriv->LinkDetectInfo.LowPowerTransitionCount == 8)
			bEnterPS = false;

		RTW_INFO("LowPowerTransitionCount=%d\n", pmlmepriv->LinkDetectInfo.LowPowerTransitionCount);
#endif /* CONFIG_DYNAMIC_DTIM */

		/* LeisurePS only work in infra mode. */
		if (bEnterPS) {
			if (!from_timer) {
#ifdef CONFIG_DYNAMIC_DTIM
				if (pmlmepriv->LinkDetectInfo.LowPowerTransitionCount < 8)
					adapter_to_pwrctl(adapt)->dtim = 1;
				else
					adapter_to_pwrctl(adapt)->dtim = 3;
#endif /* CONFIG_DYNAMIC_DTIM */
				LPS_Enter(adapt, "TRAFFIC_IDLE");
			} else {
				/* do this at caller */
				/* rtw_lps_ctrl_wk_cmd(adapter, LPS_CTRL_ENTER, 1); */
				/* rtw_hal_dm_watchdog_in_lps(adapt); */
			}
#ifdef CONFIG_DYNAMIC_DTIM
			if (adapter_to_pwrctl(adapt)->bFwCurrentInPSMode)
				pmlmepriv->LinkDetectInfo.LowPowerTransitionCount++;
#endif /* CONFIG_DYNAMIC_DTIM */
		} else {
#ifdef CONFIG_DYNAMIC_DTIM
			if (pmlmepriv->LinkDetectInfo.LowPowerTransitionCount != 8)
				pmlmepriv->LinkDetectInfo.LowPowerTransitionCount = 0;
			else
				pmlmepriv->LinkDetectInfo.LowPowerTransitionCount++;
#endif /* CONFIG_DYNAMIC_DTIM			 */
			if (!from_timer)
				LPS_Leave(adapt, "TRAFFIC_BUSY");
			else {
#ifdef CONFIG_CONCURRENT_MODE
				#ifndef CONFIG_FW_MULTI_PORT_SUPPORT
				if (adapt->hw_port == HW_PORT0)
				#endif
#endif
					rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_TRAFFIC_BUSY, 1);
			}
		}
	} else {
		struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
		int n_assoc_iface = 0;
		int i;

		for (i = 0; i < dvobj->iface_nums; i++) {
			if (check_fwstate(&(dvobj->adapters[i]->mlmepriv), WIFI_ASOC_STATE))
				n_assoc_iface++;
		}

		if (!from_timer && n_assoc_iface == 0)
			LPS_Leave(adapt, "NON_LINKED");
	}

	session_tracker_chk_cmd(adapt, NULL);

	pmlmepriv->LinkDetectInfo.NumRxOkInPeriod = 0;
	pmlmepriv->LinkDetectInfo.NumTxOkInPeriod = 0;
	pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod = 0;
	pmlmepriv->LinkDetectInfo.bBusyTraffic = bBusyTraffic;
	pmlmepriv->LinkDetectInfo.bTxBusyTraffic = bTxBusyTraffic;
	pmlmepriv->LinkDetectInfo.bRxBusyTraffic = bRxBusyTraffic;
	pmlmepriv->LinkDetectInfo.bHigherBusyTraffic = bHigherBusyTraffic;
	pmlmepriv->LinkDetectInfo.bHigherBusyRxTraffic = bHigherBusyRxTraffic;
	pmlmepriv->LinkDetectInfo.bHigherBusyTxTraffic = bHigherBusyTxTraffic;

	return bEnterPS;

}


/* for 11n Logo 4.2.31/4.2.32 */
static void dynamic_update_bcn_check(struct adapter *adapt)
{
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;

	if (!adapt->registrypriv.wifi_spec)
		return;

	if (!MLME_IS_AP(adapt))
		return;

	if (pmlmeext->bstart_bss) {
		/* In 10 * 2 = 20s, there are no legacy AP, update HT info  */
		static u8 count = 1;

		if (count % 10 == 0) {
			count = 1;

			if (false == ATOMIC_READ(&pmlmepriv->olbc)
				&& false == ATOMIC_READ(&pmlmepriv->olbc_ht)) {

				if (rtw_ht_operation_update(adapt) > 0) {
					update_beacon(adapt, _HT_CAPABILITY_IE_, NULL, false);
					update_beacon(adapt, _HT_ADD_INFO_IE_, NULL, true);
				}
			}
		}

		/* In 2s, there are any legacy AP, update HT info, and then reset count  */

		if (false != ATOMIC_READ(&pmlmepriv->olbc)
			&& false != ATOMIC_READ(&pmlmepriv->olbc_ht)) {
					
			if (rtw_ht_operation_update(adapt) > 0) {
				update_beacon(adapt, _HT_CAPABILITY_IE_, NULL, false);
				update_beacon(adapt, _HT_ADD_INFO_IE_, NULL, true);

			}
			ATOMIC_SET(&pmlmepriv->olbc, false);
			ATOMIC_SET(&pmlmepriv->olbc_ht, false);
			count = 0;
		}

		count ++;
	}
}
void rtw_iface_dynamic_chk_wk_hdl(struct adapter *adapt)
{
	if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt))
		expire_timeout_chk(adapt);
	dynamic_update_bcn_check(adapt);

	linked_status_chk(adapt, 0);
	traffic_status_watchdog(adapt, 0);

	/* for debug purpose */
	_linked_info_dump(adapt);
}
static void rtw_dynamic_chk_wk_hdl(struct adapter *adapt)
{
	rtw_mi_dynamic_chk_wk_hdl(adapt);

	dm_DynamicUsbTxAgg(adapt, 0);
	rtw_hal_dm_watchdog(adapt);

	/* BT-Coexist */
	rtw_btcoex_Handler(adapt);
}

void lps_ctrl_wk_hdl(struct adapter *adapt, u8 lps_ctrl_type);
void lps_ctrl_wk_hdl(struct adapter *adapt, u8 lps_ctrl_type)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	u8	mstatus;


	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
	    || (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)))
		return;

	switch (lps_ctrl_type) {
	case LPS_CTRL_SCAN:
		/* RTW_INFO("LPS_CTRL_SCAN\n"); */
		rtw_btcoex_ScanNotify(adapt, true);
		if (check_fwstate(pmlmepriv, _FW_LINKED)) {
			/* connect */
			LPS_Leave(adapt, "LPS_CTRL_SCAN");
		}
		break;
	case LPS_CTRL_JOINBSS:
		/* RTW_INFO("LPS_CTRL_JOINBSS\n"); */
		LPS_Leave(adapt, "LPS_CTRL_JOINBSS");
		break;
	case LPS_CTRL_CONNECT:
		/* RTW_INFO("LPS_CTRL_CONNECT\n"); */
		mstatus = 1;/* connect */
		/* Reset LPS Setting */
		pwrpriv->LpsIdleCount = 0;
		rtw_hal_set_hwreg(adapt, HW_VAR_H2C_FW_JOINBSSRPT, (u8 *)(&mstatus));
		rtw_btcoex_MediaStatusNotify(adapt, mstatus);
		break;
	case LPS_CTRL_DISCONNECT:
		/* RTW_INFO("LPS_CTRL_DISCONNECT\n"); */
		mstatus = 0;/* disconnect */
		rtw_btcoex_MediaStatusNotify(adapt, mstatus);
		LPS_Leave(adapt, "LPS_CTRL_DISCONNECT");
		rtw_hal_set_hwreg(adapt, HW_VAR_H2C_FW_JOINBSSRPT, (u8 *)(&mstatus));
		break;
	case LPS_CTRL_SPECIAL_PACKET:
		/* RTW_INFO("LPS_CTRL_SPECIAL_PACKET\n"); */
		pwrpriv->DelayLPSLastTimeStamp = rtw_get_current_time();
		rtw_btcoex_SpecialPacketNotify(adapt, PACKET_DHCP);
		LPS_Leave(adapt, "LPS_CTRL_SPECIAL_PACKET");
		break;
	case LPS_CTRL_LEAVE:
		LPS_Leave(adapt, "LPS_CTRL_LEAVE");
		break;
	case LPS_CTRL_LEAVE_CFG80211_PWRMGMT:
		LPS_Leave(adapt, "CFG80211_PWRMGMT");
		break;
	case LPS_CTRL_TRAFFIC_BUSY:
		LPS_Leave(adapt, "LPS_CTRL_TRAFFIC_BUSY");
		break;
	case LPS_CTRL_TX_TRAFFIC_LEAVE:
		LPS_Leave(adapt, "LPS_CTRL_TX_TRAFFIC_LEAVE");
		break;
	case LPS_CTRL_RX_TRAFFIC_LEAVE:
		LPS_Leave(adapt, "LPS_CTRL_RX_TRAFFIC_LEAVE");
		break;
	case LPS_CTRL_ENTER:
		LPS_Enter(adapt, "TRAFFIC_IDLE_1");
		break;
	default:
		break;
	}

}

u8 rtw_lps_ctrl_wk_cmd(struct adapter *adapt, u8 lps_ctrl_type, u8 enqueue)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	/* struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt); */
	u8	res = _SUCCESS;


	/* if(!pwrctrlpriv->bLeisurePs) */
	/*	return res; */

	if (enqueue) {
		ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
		if (!ph2c) {
			res = _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
		if (!pdrvextra_cmd_parm) {
			rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
			res = _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm->ec_id = LPS_CTRL_WK_CID;
		pdrvextra_cmd_parm->type = lps_ctrl_type;
		pdrvextra_cmd_parm->size = 0;
		pdrvextra_cmd_parm->pbuf = NULL;

		init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	} else
		lps_ctrl_wk_hdl(adapt, lps_ctrl_type);

exit:


	return res;

}

static void rtw_dm_in_lps_hdl(struct adapter *adapt)
{
	rtw_hal_set_hwreg(adapt, HW_VAR_DM_IN_LPS_LCLK, NULL);
}

u8 rtw_dm_in_lps_wk_cmd(struct adapter *adapt)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = DM_IN_LPS_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:

	return res;
}

static void rtw_lps_change_dtim_hdl(struct adapter *adapt, u8 dtim)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);

	if (dtim <= 0 || dtim > 16)
		return;

	if (rtw_btcoex_IsBtControlLps(adapt))
		return;
	if (pwrpriv->dtim != dtim) {
		RTW_INFO("change DTIM from %d to %d, bFwCurrentInPSMode=%d, ps_mode=%d\n", pwrpriv->dtim, dtim,
			 pwrpriv->bFwCurrentInPSMode, pwrpriv->pwr_mode);

		pwrpriv->dtim = dtim;
	}

	if ((pwrpriv->bFwCurrentInPSMode) && (pwrpriv->pwr_mode > PS_MODE_ACTIVE)) {
		u8 ps_mode = pwrpriv->pwr_mode;

		rtw_hal_set_hwreg(adapt, HW_VAR_H2C_FW_PWRMODE, (u8 *)(&ps_mode));
	}
}

u8 rtw_lps_change_dtim_cmd(struct adapter *adapt, u8 dtim)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;
	/*
	#ifdef CONFIG_CONCURRENT_MODE
		if (adapt->hw_port != HW_PORT0)
			return res;
	#endif
	*/
	{
		ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
		if (!ph2c) {
			res = _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
		if (!pdrvextra_cmd_parm) {
			rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
			res = _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm->ec_id = LPS_CHANGE_DTIM_CID;
		pdrvextra_cmd_parm->type = dtim;
		pdrvextra_cmd_parm->size = 0;
		pdrvextra_cmd_parm->pbuf = NULL;

		init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	}

exit:

	return res;

}

#if (RATE_ADAPTIVE_SUPPORT == 1)
void rpt_timer_setting_wk_hdl(struct adapter *adapt, u16 minRptTime)
{
	rtw_hal_set_hwreg(adapt, HW_VAR_RPT_TIMER_SETTING, (u8 *)(&minRptTime));
}

u8 rtw_rpt_timer_cfg_cmd(struct adapter *adapt, u16 minRptTime)
{
	struct cmd_obj		*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;

	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = RTP_TIMER_CFG_WK_CID;
	pdrvextra_cmd_parm->type = minRptTime;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;
	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:


	return res;

}

#endif

static void rtw_dm_ra_mask_hdl(struct adapter *adapt, struct sta_info *psta)
{
	if (psta)
		set_sta_rate(adapt, psta);
}

u8 rtw_dm_ra_mask_wk_cmd(struct adapter *adapt, u8 *psta)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = DM_RA_MSK_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = psta;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:

	return res;

}

static void power_saving_wk_hdl(struct adapter *adapt)
{
	rtw_ps_processor(adapt);
}

/* add for CONFIG_IEEE80211W, none 11w can use it */
static void reset_securitypriv_hdl(struct adapter *adapt)
{
	rtw_reset_securitypriv(adapt);
}

static void free_assoc_resources_hdl(struct adapter *adapt)
{
	rtw_free_assoc_resources(adapt, 1);
}

u8 p2p_protocol_wk_cmd(struct adapter *adapt, int intCmdType)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct wifidirect_info	*pwdinfo = &(adapt->wdinfo);
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;


	if (rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
		return res;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = P2P_PROTO_WK_CID;
	pdrvextra_cmd_parm->type = intCmdType;	/*	As the command tppe. */
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;		/*	Must be NULL here */

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:


	return res;

}

static u8 _p2p_roch_cmd(struct adapter *adapter
	, u64 cookie, struct wireless_dev *wdev
	, struct ieee80211_channel *ch, enum nl80211_channel_type ch_type
	, unsigned int duration
	, u8 flags
)
{
	struct cmd_obj *cmdobj;
	struct drvextra_cmd_parm *parm;
	struct p2p_roch_parm *roch_parm;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	struct submit_ctx sctx;
	u8 cancel = duration ? 0 : 1;
	u8	res = _SUCCESS;

	roch_parm = (struct p2p_roch_parm *)rtw_zmalloc(sizeof(struct p2p_roch_parm));
	if (!roch_parm) {
		res = _FAIL;
		goto exit;
	}

	roch_parm->cookie = cookie;
	roch_parm->wdev = wdev;
	if (!cancel) {
		memcpy(&roch_parm->ch, ch, sizeof(struct ieee80211_channel));
		roch_parm->ch_type = ch_type;
		roch_parm->duration = duration;
	}

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != p2p_protocol_wk_hdl(adapter, cancel ? P2P_CANCEL_RO_CH_WK : P2P_RO_CH_WK, (u8 *)roch_parm))
			res = _FAIL;
		rtw_mfree((u8 *)roch_parm, sizeof(*roch_parm));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
		if (!parm) {
			rtw_mfree((u8 *)roch_parm, sizeof(*roch_parm));
			res = _FAIL;
			goto exit;
		}

		parm->ec_id = P2P_PROTO_WK_CID;
		parm->type = cancel ? P2P_CANCEL_RO_CH_WK : P2P_RO_CH_WK;
		parm->size = sizeof(*roch_parm);
		parm->pbuf = (u8 *)roch_parm;

		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (!cmdobj) {
			res = _FAIL;
			rtw_mfree((u8 *)roch_parm, sizeof(*roch_parm));
			rtw_mfree((u8 *)parm, sizeof(*parm));
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(cmdobj, parm, GEN_CMD_CODE(_Set_Drv_Extra));

		if (flags & RTW_CMDF_WAIT_ACK) {
			cmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 10 * 1000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, cmdobj);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				cmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status != RTW_SCTX_DONE_SUCCESS)
				res = _FAIL;
		}
	}

exit:
	return res;
}

inline u8 p2p_roch_cmd(struct adapter *adapter
	, u64 cookie, struct wireless_dev *wdev
	, struct ieee80211_channel *ch, enum nl80211_channel_type ch_type
	, unsigned int duration
	, u8 flags
)
{
	return _p2p_roch_cmd(adapter, cookie, wdev, ch, ch_type, duration, flags);
}

inline u8 p2p_cancel_roch_cmd(struct adapter *adapter, u64 cookie, struct wireless_dev *wdev, u8 flags)
{
	return _p2p_roch_cmd(adapter, cookie, wdev, NULL, 0, 0, flags);
}

inline u8 rtw_mgnt_tx_cmd(struct adapter *adapter, u8 tx_ch, u8 no_cck, const u8 *buf, size_t len, int wait_ack, u8 flags)
{
	struct cmd_obj *cmdobj;
	struct drvextra_cmd_parm *parm;
	struct mgnt_tx_parm *mgnt_parm;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	struct submit_ctx sctx;
	u8	res = _SUCCESS;

	mgnt_parm = (struct mgnt_tx_parm *)rtw_zmalloc(sizeof(struct mgnt_tx_parm));
	if (!mgnt_parm) {
		res = _FAIL;
			goto exit;
	}

	mgnt_parm->tx_ch = tx_ch;
	mgnt_parm->no_cck = no_cck;
	mgnt_parm->buf = buf;
	mgnt_parm->len = len;
	mgnt_parm->wait_ack = wait_ack;

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != rtw_mgnt_tx_handler(adapter, (u8 *)mgnt_parm))
			res = _FAIL;
		rtw_mfree((u8 *)mgnt_parm, sizeof(*mgnt_parm));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
		if (!parm) {
			rtw_mfree((u8 *)mgnt_parm, sizeof(*mgnt_parm));
			res = _FAIL;
			goto exit;
		}

		parm->ec_id = MGNT_TX_WK_CID;
		parm->type = 0;
		parm->size = sizeof(*mgnt_parm);
		parm->pbuf = (u8 *)mgnt_parm;

		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (!cmdobj) {
			res = _FAIL;
			rtw_mfree((u8 *)mgnt_parm, sizeof(*mgnt_parm));
			rtw_mfree((u8 *)parm, sizeof(*parm));
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(cmdobj, parm, GEN_CMD_CODE(_Set_Drv_Extra));

		if (flags & RTW_CMDF_WAIT_ACK) {
			cmdobj->sctx = &sctx;
			rtw_sctx_init(&sctx, 10 * 1000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, cmdobj);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				cmdobj->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status != RTW_SCTX_DONE_SUCCESS)
				res = _FAIL;
		}
	}

exit:
	return res;
}

u8 rtw_ps_cmd(struct adapter *adapt)
{
	struct cmd_obj		*ppscmd;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;

	u8	res = _SUCCESS;

#ifdef CONFIG_CONCURRENT_MODE
	if (adapt->adapter_type != PRIMARY_ADAPTER)
		goto exit;
#endif

	ppscmd = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ppscmd) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ppscmd, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = POWER_SAVING_CTRL_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;
	init_h2fwcmd_w_parm_no_rsp(ppscmd, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ppscmd);

exit:


	return res;

}

static void rtw_chk_hi_queue_hdl(struct adapter *adapt)
{
	struct sta_info *psta_bmc;
	struct sta_priv *pstapriv = &adapt->stapriv;
	unsigned long start = rtw_get_current_time();
	u8 empty = false;

	psta_bmc = rtw_get_bcmc_stainfo(adapt);
	if (!psta_bmc)
		return;

	rtw_hal_get_hwreg(adapt, HW_VAR_CHK_HI_QUEUE_EMPTY, &empty);

	while (false == empty && rtw_get_passing_time_ms(start) < rtw_get_wait_hiq_empty_ms()) {
		rtw_msleep_os(100);
		rtw_hal_get_hwreg(adapt, HW_VAR_CHK_HI_QUEUE_EMPTY, &empty);
	}

	if (psta_bmc->sleepq_len == 0) {
		if (empty == _SUCCESS) {
			bool update_tim = false;

			if (rtw_tim_map_is_set(adapt, pstapriv->tim_bitmap, 0))
				update_tim = true;

			rtw_tim_map_clear(adapt, pstapriv->tim_bitmap, 0);
			rtw_tim_map_clear(adapt, pstapriv->sta_dz_bitmap, 0);

			if (update_tim)
				_update_beacon(adapt, _TIM_IE_, NULL, true, "bmc sleepq and HIQ empty");
		} else /* re check again */
			rtw_chk_hi_queue_cmd(adapt);

	}

}

u8 rtw_chk_hi_queue_cmd(struct adapter *adapt)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &adapt->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = CHECK_HIQ_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:

	return res;

}

struct btinfo {
	u8 cid;
	u8 len;

	u8 bConnection:1;
	u8 bSCOeSCO:1;
	u8 bInQPage:1;
	u8 bACLBusy:1;
	u8 bSCOBusy:1;
	u8 bHID:1;
	u8 bA2DP:1;
	u8 bFTP:1;

	u8 retry_cnt:4;
	u8 rsvd_34:1;
	u8 rsvd_35:1;
	u8 rsvd_36:1;
	u8 rsvd_37:1;

	u8 rssi;

	u8 rsvd_50:1;
	u8 rsvd_51:1;
	u8 rsvd_52:1;
	u8 rsvd_53:1;
	u8 rsvd_54:1;
	u8 rsvd_55:1;
	u8 eSCO_SCO:1;
	u8 Master_Slave:1;

	u8 rsvd_6;
	u8 rsvd_7;
};

static void rtw_btinfo_hdl(struct adapter *adapter, u8 *buf, u16 buf_len)
{
#define BTINFO_WIFI_FETCH 0x23
#define BTINFO_BT_AUTO_RPT 0x27
	struct btinfo *info = (struct btinfo *)buf;
	u8 cmd_idx;
	u8 len;

	cmd_idx = info->cid;

	if (info->len > buf_len - 2) {
		rtw_warn_on(1);
		len = buf_len - 2;
	} else
		len = info->len;
	/* transform BT-FW btinfo to WiFI-FW C2H format and notify */
	if (cmd_idx == BTINFO_WIFI_FETCH)
		buf[1] = 0;
	else if (cmd_idx == BTINFO_BT_AUTO_RPT)
		buf[1] = 2;
	rtw_btcoex_BtInfoNotify(adapter , len + 1, &buf[1]);
}

u8 rtw_btinfo_cmd(struct adapter *adapter, u8 *buf, u16 len)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm *pdrvextra_cmd_parm;
	u8 *btinfo;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((u8 *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	btinfo = rtw_zmalloc(len);
	if (!btinfo) {
		rtw_mfree((u8 *)ph2c, sizeof(struct cmd_obj));
		rtw_mfree((u8 *)pdrvextra_cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = BTINFO_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = len;
	pdrvextra_cmd_parm->pbuf = btinfo;

	memcpy(btinfo, buf, len);

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}

u8 rtw_test_h2c_cmd(struct adapter *adapter, u8 *buf, u8 len)
{
	struct cmd_obj *pcmdobj;
	struct drvextra_cmd_parm *pdrvextra_cmd_parm;
	u8 *ph2c_content;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	u8	res = _SUCCESS;

	pcmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!pcmdobj) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	ph2c_content = rtw_zmalloc(len);
	if (!ph2c_content) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		rtw_mfree((u8 *)pdrvextra_cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = TEST_H2C_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = len;
	pdrvextra_cmd_parm->pbuf = ph2c_content;

	memcpy(ph2c_content, buf, len);

	init_h2fwcmd_w_parm_no_rsp(pcmdobj, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);

exit:
	return res;
}

static int rtw_customer_str_cmd_hdl(struct adapter *adapter, u8 write, const u8 *cstr)
{
	int ret = H2C_SUCCESS;

	if (write)
		ret = rtw_hal_h2c_customer_str_write(adapter, cstr);
	else
		ret = rtw_hal_h2c_customer_str_req(adapter);

	return ret == _SUCCESS ? H2C_SUCCESS : H2C_REJECTED;
}

static u8 rtw_customer_str_cmd(struct adapter *adapter, u8 write, const u8 *cstr)
{
	struct cmd_obj *cmdobj;
	struct drvextra_cmd_parm *parm;
	u8 *str = NULL;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	struct submit_ctx sctx;
	u8 res = _SUCCESS;

	parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!parm) {
		res = _FAIL;
		goto exit;
	}

	if (write) {
		str = rtw_zmalloc(RTW_CUSTOMER_STR_LEN);
		if (!str) {
			rtw_mfree((u8 *)parm, sizeof(struct drvextra_cmd_parm));
			res = _FAIL;
			goto exit;
		}
	}

	parm->ec_id = CUSTOMER_STR_WK_CID;
	parm->type = write;
	parm->size = write ? RTW_CUSTOMER_STR_LEN : 0;
	parm->pbuf = write ? str : NULL;

	if (write)
		memcpy(str, cstr, RTW_CUSTOMER_STR_LEN);

	/* need enqueue, prepare cmd_obj and enqueue */
	cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
	if (!cmdobj) {
		res = _FAIL;
		rtw_mfree((u8 *)parm, sizeof(*parm));
		if (write)
			rtw_mfree(str, RTW_CUSTOMER_STR_LEN);
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(cmdobj, parm, GEN_CMD_CODE(_Set_Drv_Extra));

	cmdobj->sctx = &sctx;
	rtw_sctx_init(&sctx, 2 * 1000);

	res = rtw_enqueue_cmd(pcmdpriv, cmdobj);

	if (res == _SUCCESS) {
		rtw_sctx_wait(&sctx, __func__);
		_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		if (sctx.status == RTW_SCTX_SUBMITTED)
			cmdobj->sctx = NULL;
		_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		if (sctx.status != RTW_SCTX_DONE_SUCCESS)
			res = _FAIL;
	}

exit:
	return res;
}

inline u8 rtw_customer_str_req_cmd(struct adapter *adapter)
{
	return rtw_customer_str_cmd(adapter, 0, NULL);
}

inline u8 rtw_customer_str_write_cmd(struct adapter *adapter, const u8 *cstr)
{
	return rtw_customer_str_cmd(adapter, 1, cstr);
}

static u8 rtw_c2h_wk_cmd(struct adapter * adapt, u8 *pbuf, u16 length, u8 type)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm *pdrvextra_cmd_parm;
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;
	u8 *extra_cmd_buf;
	u8 res = _SUCCESS;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!pdrvextra_cmd_parm) {
		rtw_mfree((u8 *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	extra_cmd_buf = rtw_zmalloc(length);
	if (!extra_cmd_buf) {
		rtw_mfree((u8 *)ph2c, sizeof(struct cmd_obj));
		rtw_mfree((u8 *)pdrvextra_cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	memcpy(extra_cmd_buf, pbuf, length);
	pdrvextra_cmd_parm->ec_id = C2H_WK_CID;
	pdrvextra_cmd_parm->type = type;
	pdrvextra_cmd_parm->size = length;
	pdrvextra_cmd_parm->pbuf = extra_cmd_buf;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}

#ifdef CONFIG_FW_C2H_REG
inline u8 rtw_c2h_reg_wk_cmd(struct adapter *adapt, u8 *c2h_evt)
{
	return rtw_c2h_wk_cmd(adapt, c2h_evt, c2h_evt ? C2H_REG_LEN : 0, C2H_TYPE_REG);
}
#endif

inline u8 rtw_c2h_packet_wk_cmd(struct adapter *adapt, u8 *c2h_evt, u16 length)
{
	return rtw_c2h_wk_cmd(adapt, c2h_evt, length, C2H_TYPE_PKT);
}

u8 rtw_run_in_thread_cmd(struct adapter * adapt, void (*func)(void *), void *context)
{
	struct cmd_priv *pcmdpriv;
	struct cmd_obj *ph2c;
	struct RunInThread_param *parm;
	int res = _SUCCESS;

	pcmdpriv = &adapt->cmdpriv;

	ph2c = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!ph2c) {
		res = _FAIL;
		goto exit;
	}

	parm = (struct RunInThread_param *)rtw_zmalloc(sizeof(struct RunInThread_param));
	if (!parm) {
		rtw_mfree((u8 *)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	parm->func = func;
	parm->context = context;
	init_h2fwcmd_w_parm_no_rsp(ph2c, parm, GEN_CMD_CODE(_RunInThreadCMD));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:


	return res;
}

#ifdef CONFIG_FW_C2H_REG
int c2h_evt_hdl(struct adapter *adapter, u8 *c2h_evt, c2h_id_filter filter)
{
	int ret = _FAIL;
	u8 buf[C2H_REG_LEN] = {0};
	u8 id, seq, plen;
	u8 *payload;

	if (!c2h_evt) {
		/* No c2h event in cmd_obj, read c2h event before handling*/
		if (rtw_hal_c2h_evt_read(adapter, buf) != _SUCCESS)
			goto exit;
		c2h_evt = buf;
	}

	rtw_hal_c2h_reg_hdr_parse(adapter, c2h_evt, &id, &seq, &plen, &payload);

	if (filter && !filter(adapter, id, seq, plen, payload))
		goto exit;

	ret = rtw_hal_c2h_handler(adapter, id, seq, plen, payload);

exit:
	return ret;
}
#endif /* CONFIG_FW_C2H_REG */

static u8 session_tracker_cmd(struct adapter *adapter, u8 cmd, struct sta_info *sta, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port)
{
	struct cmd_priv	*cmdpriv = &adapter->cmdpriv;
	struct cmd_obj *cmdobj;
	struct drvextra_cmd_parm *cmd_parm;
	struct st_cmd_parm *st_parm;
	u8	res = _SUCCESS;

	cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (!cmdobj) {
		res = _FAIL;
		goto exit;
	}

	cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (!cmd_parm) {
		rtw_mfree((u8 *)cmdobj, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	st_parm = (struct st_cmd_parm *)rtw_zmalloc(sizeof(struct st_cmd_parm));
	if (!st_parm) {
		rtw_mfree((u8 *)cmdobj, sizeof(struct cmd_obj));
		rtw_mfree((u8 *)cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	st_parm->cmd = cmd;
	st_parm->sta = sta;
	if (cmd != ST_CMD_CHK) {
		memcpy(&st_parm->local_naddr, local_naddr, 4);
		memcpy(&st_parm->local_port, local_port, 2);
		memcpy(&st_parm->remote_naddr, remote_naddr, 4);
		memcpy(&st_parm->remote_port, remote_port, 2);
	}

	cmd_parm->ec_id = SESSION_TRACKER_WK_CID;
	cmd_parm->type = 0;
	cmd_parm->size = sizeof(struct st_cmd_parm);
	cmd_parm->pbuf = (u8 *)st_parm;
	init_h2fwcmd_w_parm_no_rsp(cmdobj, cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));
	cmdobj->no_io = 1;

	res = rtw_enqueue_cmd(cmdpriv, cmdobj);

exit:
	return res;
}

inline u8 session_tracker_chk_cmd(struct adapter *adapter, struct sta_info *sta)
{
	return session_tracker_cmd(adapter, ST_CMD_CHK, sta, NULL, NULL, NULL, NULL);
}

inline u8 session_tracker_add_cmd(struct adapter *adapter, struct sta_info *sta, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port)
{
	return session_tracker_cmd(adapter, ST_CMD_ADD, sta, local_naddr, local_port, remote_naddr, remote_port);
}

inline u8 session_tracker_del_cmd(struct adapter *adapter, struct sta_info *sta, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port)
{
	return session_tracker_cmd(adapter, ST_CMD_DEL, sta, local_naddr, local_port, remote_naddr, remote_port);
}

static void session_tracker_chk_for_sta(struct adapter *adapter, struct sta_info *sta)
{
	struct st_ctl_t *st_ctl = &sta->st_ctl;
	int i;
	unsigned long irqL;
	struct list_head *plist, *phead, *pnext;
	struct list_head dlist;
	struct session_tracker *st = NULL;
	u8 op_wfd_mode = MIRACAST_DISABLED;

	if (DBG_SESSION_TRACKER)
		RTW_INFO(FUNC_ADPT_FMT" sta:%p\n", FUNC_ADPT_ARG(adapter), sta);

	if (!(sta->state & _FW_LINKED))
		goto exit;

	for (i = 0; i < SESSION_TRACKER_REG_ID_NUM; i++) {
		if (st_ctl->reg[i].s_proto != 0)
			break;
	}
	if (i >= SESSION_TRACKER_REG_ID_NUM)
		goto chk_sta;

	INIT_LIST_HEAD(&dlist);

	_enter_critical_bh(&st_ctl->tracker_q.lock, &irqL);

	phead = &st_ctl->tracker_q.queue;
	plist = get_next(phead);
	pnext = get_next(plist);
	while (!rtw_end_of_queue_search(phead, plist)) {
		st = container_of(plist, struct session_tracker, list);
		plist = pnext;
		pnext = get_next(pnext);

		if (st->status != ST_STATUS_ESTABLISH
			&& rtw_get_passing_time_ms(st->set_time) > ST_EXPIRE_MS
		) {
			rtw_list_delete(&st->list);
			list_add_tail(&st->list, &dlist);
		}

		/* TODO: check OS for status update */
		if (st->status == ST_STATUS_CHECK)
			st->status = ST_STATUS_ESTABLISH;

		if (st->status != ST_STATUS_ESTABLISH)
			continue;

		if (ntohs(st->local_port) == adapter->wfd_info.rtsp_ctrlport)
			op_wfd_mode |= MIRACAST_SINK;
		if (ntohs(st->local_port) == adapter->wfd_info.tdls_rtsp_ctrlport)
			op_wfd_mode |= MIRACAST_SINK;
		if (ntohs(st->remote_port) == adapter->wfd_info.peer_rtsp_ctrlport)
			op_wfd_mode |= MIRACAST_SOURCE;
	}

	_exit_critical_bh(&st_ctl->tracker_q.lock, &irqL);

	plist = get_next(&dlist);
	while (!rtw_end_of_queue_search(&dlist, plist)) {
		st = container_of(plist, struct session_tracker, list);
		plist = get_next(plist);
		rtw_mfree((u8 *)st, sizeof(struct session_tracker));
	}

chk_sta:
	if (STA_OP_WFD_MODE(sta) != op_wfd_mode) {
		STA_SET_OP_WFD_MODE(sta, op_wfd_mode);
		rtw_sta_media_status_rpt_cmd(adapter, sta, 1);
	}

exit:
	return;
}

static void session_tracker_chk_for_adapter(struct adapter *adapter)
{
	struct sta_priv *stapriv = &adapter->stapriv;
	struct sta_info *sta;
	int i;
	unsigned long irqL;
	struct list_head *plist, *phead;
	u8 op_wfd_mode = MIRACAST_DISABLED;

	_enter_critical_bh(&stapriv->sta_hash_lock, &irqL);

	for (i = 0; i < NUM_STA; i++) {
		phead = &(stapriv->sta_hash[i]);
		plist = get_next(phead);

		while ((!rtw_end_of_queue_search(phead, plist))) {
			sta = container_of(plist, struct sta_info, hash_list);
			plist = get_next(plist);

			session_tracker_chk_for_sta(adapter, sta);

			op_wfd_mode |= STA_OP_WFD_MODE(sta);
		}
	}

	_exit_critical_bh(&stapriv->sta_hash_lock, &irqL);

	adapter->wfd_info.op_wfd_mode = MIRACAST_MODE_REVERSE(op_wfd_mode);
}

static void session_tracker_cmd_hdl(struct adapter *adapter, struct st_cmd_parm *parm)
{
	u8 cmd = parm->cmd;
	struct sta_info *sta = parm->sta;

	if (cmd == ST_CMD_CHK) {
		if (sta)
			session_tracker_chk_for_sta(adapter, sta);
		else
			session_tracker_chk_for_adapter(adapter);

		goto exit;

	} else if (cmd == ST_CMD_ADD || cmd == ST_CMD_DEL) {
		struct st_ctl_t *st_ctl;
		u32 local_naddr = parm->local_naddr;
		__be16 local_port = cpu_to_be16(parm->local_port);
		u32 remote_naddr = parm->remote_naddr;
		__be16 remote_port = cpu_to_be16(parm->remote_port);
		struct session_tracker *st = NULL;
		unsigned long irqL;
		struct list_head *plist, *phead;
		u8 free_st = 0;
		u8 alloc_st = 0;

		if (DBG_SESSION_TRACKER)
			RTW_INFO(FUNC_ADPT_FMT" cmd:%u, sta:%p, local:"IP_FMT":"PORT_FMT", remote:"IP_FMT":"PORT_FMT"\n"
				, FUNC_ADPT_ARG(adapter), cmd, sta
				, IP_ARG(&local_naddr), PORT_ARG(&local_port)
				, IP_ARG(&remote_naddr), PORT_ARG(&remote_port)
			);

		if (!(sta->state & _FW_LINKED))
			goto exit;

		st_ctl = &sta->st_ctl;

		_enter_critical_bh(&st_ctl->tracker_q.lock, &irqL);

		phead = &st_ctl->tracker_q.queue;
		plist = get_next(phead);
		while (!rtw_end_of_queue_search(phead, plist)) {
			st = container_of(plist, struct session_tracker, list);

			if (st->local_naddr == local_naddr
				&& st->local_port == local_port
				&& st->remote_naddr == remote_naddr
				&& st->remote_port == remote_port)
				break;

			plist = get_next(plist);
		}

		if (rtw_end_of_queue_search(phead, plist))
			st = NULL;

		switch (cmd) {
		case ST_CMD_DEL:
			if (st) {
				rtw_list_delete(plist);
				free_st = 1;
			}
			goto unlock;
		case ST_CMD_ADD:
			if (!st)
				alloc_st = 1;
		}

unlock:
		_exit_critical_bh(&st_ctl->tracker_q.lock, &irqL);

		if (free_st) {
			rtw_mfree((u8 *)st, sizeof(struct session_tracker));
			goto exit;
		}

		if (alloc_st) {
			st = (struct session_tracker *)rtw_zmalloc(sizeof(struct session_tracker));
			if (!st)
				goto exit;

			st->local_naddr = local_naddr;
			st->local_port = local_port;
			st->remote_naddr = remote_naddr;
			st->remote_port = remote_port;
			st->set_time = rtw_get_current_time();
			st->status = ST_STATUS_CHECK;

			_enter_critical_bh(&st_ctl->tracker_q.lock, &irqL);
			list_add_tail(&st->list, phead);
			_exit_critical_bh(&st_ctl->tracker_q.lock, &irqL);
		}
	}

exit:
	return;
}

u8 rtw_drvextra_cmd_hdl(struct adapter *adapt, unsigned char *pbuf)
{
	int ret = H2C_SUCCESS;
	struct drvextra_cmd_parm *pdrvextra_cmd;

	if (!pbuf)
		return H2C_PARAMETERS_ERROR;

	pdrvextra_cmd = (struct drvextra_cmd_parm *)pbuf;

	switch (pdrvextra_cmd->ec_id) {
	case STA_MSTATUS_RPT_WK_CID:
		rtw_sta_media_status_rpt_cmd_hdl(adapt, (struct sta_media_status_rpt_cmd_parm *)pdrvextra_cmd->pbuf);
		break;

	case DYNAMIC_CHK_WK_CID:/*only  primary adapt go to this cmd, but execute dynamic_chk_wk_hdl() for two interfaces */
		rtw_dynamic_chk_wk_hdl(adapt);
		break;
	case POWER_SAVING_CTRL_WK_CID:
		power_saving_wk_hdl(adapt);
		break;
	case LPS_CTRL_WK_CID:
		lps_ctrl_wk_hdl(adapt, (u8)pdrvextra_cmd->type);
		break;
	case DM_IN_LPS_WK_CID:
		rtw_dm_in_lps_hdl(adapt);
		break;
	case LPS_CHANGE_DTIM_CID:
		rtw_lps_change_dtim_hdl(adapt, (u8)pdrvextra_cmd->type);
		break;
#if (RATE_ADAPTIVE_SUPPORT == 1)
	case RTP_TIMER_CFG_WK_CID:
		rpt_timer_setting_wk_hdl(adapt, pdrvextra_cmd->type);
		break;
#endif
	case P2P_PS_WK_CID:
		p2p_ps_wk_hdl(adapt, pdrvextra_cmd->type);
		break;
	case P2P_PROTO_WK_CID:
		/*
		* Commented by Albert 2011/07/01
		* I used the type_size as the type command
		*/
		ret = p2p_protocol_wk_hdl(adapt, pdrvextra_cmd->type, pdrvextra_cmd->pbuf);
		break;
	case CHECK_HIQ_WK_CID:
		rtw_chk_hi_queue_hdl(adapt);
		break;
#ifdef CONFIG_INTEL_WIDI
	case INTEl_WIDI_WK_CID:
		intel_widi_wk_hdl(adapt, pdrvextra_cmd->type, pdrvextra_cmd->pbuf);
		break;
#endif
	/* add for CONFIG_IEEE80211W, none 11w can use it */
	case RESET_SECURITYPRIV:
		reset_securitypriv_hdl(adapt);
		break;
	case FREE_ASSOC_RESOURCES:
		free_assoc_resources_hdl(adapt);
		break;
	case C2H_WK_CID:
		switch (pdrvextra_cmd->type) {
		#ifdef CONFIG_FW_C2H_REG
		case C2H_TYPE_REG:
			c2h_evt_hdl(adapt, pdrvextra_cmd->pbuf, NULL);
			break;
		#endif
		case C2H_TYPE_PKT:
			rtw_hal_c2h_pkt_hdl(adapt, pdrvextra_cmd->pbuf, pdrvextra_cmd->size);
			break;
		default:
			RTW_ERR("unknown C2H type:%d\n", pdrvextra_cmd->type);
			rtw_warn_on(1);
			break;
		}
		break;
	case DM_RA_MSK_WK_CID:
		rtw_dm_ra_mask_hdl(adapt, (struct sta_info *)pdrvextra_cmd->pbuf);
		break;
	case BTINFO_WK_CID:
		rtw_btinfo_hdl(adapt, pdrvextra_cmd->pbuf, pdrvextra_cmd->size);
		break;
	case SESSION_TRACKER_WK_CID:
		session_tracker_cmd_hdl(adapt, (struct st_cmd_parm *)pdrvextra_cmd->pbuf);
		break;
	case EN_HW_UPDATE_TSF_WK_CID:
		rtw_hal_set_hwreg(adapt, HW_VAR_EN_HW_UPDATE_TSF, NULL);
		break;
	case TEST_H2C_CID:
		rtw_hal_fill_h2c_cmd(adapt, pdrvextra_cmd->pbuf[0], pdrvextra_cmd->size - 1, &pdrvextra_cmd->pbuf[1]);
		break;
	case MP_CMD_WK_CID:
		break;
	case CUSTOMER_STR_WK_CID:
		ret = rtw_customer_str_cmd_hdl(adapt, pdrvextra_cmd->type, pdrvextra_cmd->pbuf);
		break;
	case MGNT_TX_WK_CID:
		ret = rtw_mgnt_tx_handler(adapt, pdrvextra_cmd->pbuf);
		break;
	default:
		break;
	}

	if (pdrvextra_cmd->pbuf && pdrvextra_cmd->size > 0)
		rtw_mfree(pdrvextra_cmd->pbuf, pdrvextra_cmd->size);

	return ret;
}

void rtw_survey_cmd_callback(struct adapter	*adapt ,  struct cmd_obj *pcmd)
{
	struct	mlme_priv *pmlmepriv = &adapt->mlmepriv;


	if (pcmd->res == H2C_DROPPED) {
		/* TODO: cancel timer and do timeout handler directly... */
		/* need to make timeout handlerOS independent */
		mlme_set_scan_to_timer(pmlmepriv, 1);
	} else if (pcmd->res != H2C_SUCCESS) {
		mlme_set_scan_to_timer(pmlmepriv, 1);
	}

	/* free cmd */
	rtw_free_cmd_obj(pcmd);

}
void rtw_disassoc_cmd_callback(struct adapter	*adapt,  struct cmd_obj *pcmd)
{
	unsigned long	irqL;
	struct	mlme_priv *pmlmepriv = &adapt->mlmepriv;


	if (pcmd->res != H2C_SUCCESS) {
		_enter_critical_bh(&pmlmepriv->lock, &irqL);
		set_fwstate(pmlmepriv, _FW_LINKED);
		_exit_critical_bh(&pmlmepriv->lock, &irqL);
		goto exit;
	}
	else /* clear bridge database */
		nat25_db_cleanup(adapt);

	/* free cmd */
	rtw_free_cmd_obj(pcmd);

exit:
	return;
}


void rtw_getmacreg_cmdrsp_callback(struct adapter *adapt,  struct cmd_obj *pcmd)
{


	rtw_free_cmd_obj(pcmd);

}

void rtw_joinbss_cmd_callback(struct adapter	*adapt,  struct cmd_obj *pcmd)
{
	struct	mlme_priv *pmlmepriv = &adapt->mlmepriv;


	if (pcmd->res == H2C_DROPPED) {
		/* TODO: cancel timer and do timeout handler directly... */
		/* need to make timeout handlerOS independent */
		_set_timer(&pmlmepriv->assoc_timer, 1);
	} else if (pcmd->res != H2C_SUCCESS)
		_set_timer(&pmlmepriv->assoc_timer, 1);

	rtw_free_cmd_obj(pcmd);

}

void rtw_create_ibss_post_hdl(struct adapter *adapt, int status)
{
	unsigned long irqL;
	struct wlan_network *pwlan = NULL;
	struct	mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct wlan_bssid_ex *pdev_network = &adapt->registrypriv.dev_network;
	struct wlan_network *mlme_cur_network = &(pmlmepriv->cur_network);

	if (status != H2C_SUCCESS)
		_set_timer(&pmlmepriv->assoc_timer, 1);

	_cancel_timer_ex(&pmlmepriv->assoc_timer);

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	{
		unsigned long irqL;

		pwlan = _rtw_alloc_network(pmlmepriv);
		_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		if (!pwlan) {
			pwlan = rtw_get_oldest_wlan_network(&pmlmepriv->scanned_queue);
			if (!pwlan) {
				_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
				goto createbss_cmd_fail;
			}
			pwlan->last_scanned = rtw_get_current_time();
		} else
			list_add_tail(&(pwlan->list), &pmlmepriv->scanned_queue.queue);

		pdev_network->Length = get_wlan_bssid_ex_sz(pdev_network);
		memcpy(&(pwlan->network), pdev_network, pdev_network->Length);
		/* pwlan->fixed = true; */

		/* copy pdev_network information to pmlmepriv->cur_network */
		memcpy(&mlme_cur_network->network, pdev_network, (get_wlan_bssid_ex_sz(pdev_network)));

		_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);
		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		/* we will set _FW_LINKED when there is one more sat to join us (rtw_stassoc_event_callback) */
	}

createbss_cmd_fail:
	_exit_critical_bh(&pmlmepriv->lock, &irqL);
	return;
}

void rtw_setstaKey_cmdrsp_callback(struct adapter	*adapt ,  struct cmd_obj *pcmd)
{

	struct sta_priv *pstapriv = &adapt->stapriv;
	struct set_stakey_rsp *psetstakey_rsp = (struct set_stakey_rsp *)(pcmd->rsp);
	struct sta_info	*psta = rtw_get_stainfo(pstapriv, psetstakey_rsp->addr);


	if (!psta)
		goto exit;
exit:
	rtw_free_cmd_obj(pcmd);
}

void rtw_setassocsta_cmdrsp_callback(struct adapter	*adapt,  struct cmd_obj *pcmd)
{
	unsigned long	irqL;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct set_assocsta_parm *passocsta_parm = (struct set_assocsta_parm *)(pcmd->parmbuf);
	struct set_assocsta_rsp *passocsta_rsp = (struct set_assocsta_rsp *)(pcmd->rsp);
	struct sta_info	*psta = rtw_get_stainfo(pstapriv, passocsta_parm->addr);


	if (!psta) {
		goto exit;
	}

	psta->cmn.aid = psta->cmn.mac_id = passocsta_rsp->cam_id;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	if ((check_fwstate(pmlmepriv, WIFI_MP_STATE)) && (check_fwstate(pmlmepriv, _FW_UNDER_LINKING)))
		_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);

	set_fwstate(pmlmepriv, _FW_LINKED);
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

exit:
	rtw_free_cmd_obj(pcmd);

}

void rtw_getrttbl_cmd_cmdrsp_callback(struct adapter	*adapt,  struct cmd_obj *pcmd);
void rtw_getrttbl_cmd_cmdrsp_callback(struct adapter	*adapt,  struct cmd_obj *pcmd)
{

	rtw_free_cmd_obj(pcmd);
}
