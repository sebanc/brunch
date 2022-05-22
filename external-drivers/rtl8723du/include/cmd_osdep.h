/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __CMD_OSDEP_H_
#define __CMD_OSDEP_H_

extern int _rtw_init_cmd_priv(struct	cmd_priv *pcmdpriv);
extern int _rtw_init_evt_priv(struct evt_priv *pevtpriv);
extern void _rtw_free_evt_priv(struct	evt_priv *pevtpriv);
extern void _rtw_free_cmd_priv(struct	cmd_priv *pcmdpriv);
extern int _rtw_enqueue_cmd(struct __queue *queue, struct cmd_obj *obj, bool to_head);
extern struct cmd_obj *_rtw_dequeue_cmd(struct __queue *queue);

#endif
