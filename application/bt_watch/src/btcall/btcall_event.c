/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt call status
 */

#define LOG_MODULE_CUSTOMER

#include <msg_manager.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include "btcall.h"
#ifdef CONFIG_UI_MANAGER
#include <ui_manager.h>
#endif
#include "tts_manager.h"

LOG_MODULE_REGISTER(btcall, LOG_LEVEL_INF);

#define BTCALL_ONGOING_WAIT_ESCO_TIMEOUT (500)

static void _bt_call_hfp_disconnected(void)
{
	SYS_LOG_INF(" enter\n");
}

static void _bt_call_sco_connected(io_stream_t upload_stream)
{
	struct btcall_app_t *btcall = btcall_get_app();

	btcall->mic_mute = false;
	btcall->upload_stream = upload_stream;
	btcall->upload_stream_outer = upload_stream ? 1 : 0;
#ifdef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
	if ((btcall->hfp_ongoing && !btcall->playing)
		 || btcall->siri_mode) {
		bt_call_start_play();
		btcall->playing = 1;
	}
#else
	bt_call_start_play();
	btcall->playing = 1;
#endif
}

static void _bt_call_sco_disconnected(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	bt_call_stop_play();
	btcall->playing = 0;
	btcall->switch_sound_source = 1;
}

static void _bt_call_hfp_active_dev_changed(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	if (btcall->playing) {
		bt_call_stop_play();
		btcall->playing = 0;
	}
}

void btcall_bt_event_proc(struct app_msg *msg)
{
	struct btcall_app_t *btcall = btcall_get_app();

	SYS_LOG_INF("btcall: bt cmd %d\n", msg->cmd);

	if (!btcall && msg->cmd != BT_HFP_CALL_STATE_START)
		return;

	switch (msg->cmd) {
	case BT_HFP_CALL_STATE_START:
		btcall_proc_start();
		break;
	case BT_HFP_CALL_STATE_EXIT:
		btcall_proc_exit();
		break;
	case BT_HFP_CALL_RING_STATR_EVENT:
	#ifdef CONFIG_UI_MANAGER
		btcall_set_phone_num(msg->ptr, false);
	#endif
		if (!btcall->phonenum_played) {
		#ifdef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
			btcall_ring_start(msg->ptr, strlen(msg->ptr));
		#endif
			btcall->phonenum_played = 1;
		}
		break;
	case BT_HFP_CALL_CCWA_EVENT:
		SYS_LOG_INF("BT_HFP_CALL_CCWA_EVENT\n");
	#ifdef CONFIG_UI_MANAGER
		btcall_set_3way_phone_num(msg->ptr);
	#endif
		break;

	case BT_HFP_CALL_RING_STOP_EVENT:
	#ifndef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
		btcall_ring_stop();
	#else

		if (btcall->hfp_ongoing && !btcall->playing) {
			bt_call_start_play();
			btcall->playing = 1;
		}
	#endif
		break;

	case BT_HFP_ACTIVEDEV_CHANGE_EVENT:
	{
		_bt_call_hfp_active_dev_changed();
		break;
	}

	case BT_HFP_CALL_OUTGOING:
	{
		if (!btcall)
			break;

		btcall->phonenum_played = 1;

	#ifdef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
		btcall_ring_stop();
	#endif
	#ifdef CONFIG_UI_MANAGER
		if (btcall->sco_established)
			btcall_set_outgoing_view();
	#endif
		if (btcall && btcall->player) {
			media_player_set_hfp_connected(btcall->player, false);
		}
		break;
	}
	case BT_HFP_CALL_INCOMING:
	{
		SYS_LOG_INF("BT_HFP_CALL_INCOMING\n");
		btcall->phonenum_played = 0;
	#ifdef CONFIG_UI_MANAGER
		btcall_set_incoming_view();
	#endif
		if (btcall && btcall->player) {
			media_player_set_hfp_connected(btcall->player, false);
		}
		break;
	}
	case BT_HFP_CALL_CLCC_INFO:
	{
		struct btsrv_hfp_clcc_info *clcc = (struct btsrv_hfp_clcc_info *)msg->ptr;

		SYS_LOG_INF("outgoing phone num %s\n", clcc->number);
	#ifdef CONFIG_UI_MANAGER
		btcall_set_phone_num(clcc->number, false);
	#endif
		if (!btcall->phonenum_played) {
		#ifdef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
			btcall_ring_start(clcc->number, strlen(clcc->number));
		#endif
			btcall->phonenum_played = 1;
		}
		break;
	}
	case BT_HFP_CALL_ONGOING:
	{
		SYS_LOG_INF("BT_HFP_CALL_ONGOING\n");
		btcall->hfp_ongoing = 1;
		btcall->phonenum_played = 1;
		btcall_ring_stop();
		if (btcall->hfp_ongoing
			&& !btcall->playing
			&& btcall->sco_established) {
			bt_call_start_play();
			btcall->playing = 1;
		}
		if (btcall && btcall->player) {
			media_player_set_hfp_connected(btcall->player, true);
		}

		if (btcall->sco_established) {
	#ifdef CONFIG_UI_MANAGER
			btcall_sync_state_to_view(false);
	#endif
		} else {
	#ifdef CONFIG_UI_MANAGER
			btcall_set_ongoing_start_time();
	#endif
			thread_timer_start(&btcall->timeout_wait_esco, BTCALL_ONGOING_WAIT_ESCO_TIMEOUT, 0);
			//btcall_exit_view();
		}

		if (btcall && btcall->player) {
			media_player_set_hfp_connected(btcall->player, true);
		}
		break;
	}
	case BT_HFP_CALL_SIRI_MODE:
		btcall->siri_mode = 1;
		btcall->phonenum_played = 1;
		SYS_LOG_INF("siri_mode %d\n", btcall->siri_mode);
		break;
	case BT_HFP_CALL_HUNGUP:
	{
		break;
	}

	case BT_HFP_DISCONNECTION_EVENT:
	{
		btcall->need_resume_play = 0;
		_bt_call_hfp_disconnected();
		break;
	}
	case BT_HFP_ESCO_ESTABLISHED_EVENT:
	{
		io_stream_t upload_stream = NULL;

		if (msg->ptr)
			memcpy(&upload_stream, msg->ptr, sizeof(upload_stream));

		btcall->sco_established = 1;
		_bt_call_sco_connected(upload_stream);

		if (btcall->player) {
			media_player_set_hfp_connected(btcall->player, true);
		}

		if (!btcall->hfp_ongoing && btcall->siri_mode) {
	#ifdef CONFIG_UI_MANAGER
			btcall_sync_state_to_view(true);
	#endif
		} else {
			thread_timer_stop(&btcall->timeout_wait_esco);
	#ifdef CONFIG_UI_MANAGER
			btcall_switch_voice_source(false);
	#endif
		}
		break;
	}
	case BT_HFP_ESCO_RELEASED_EVENT:
	{
		_bt_call_sco_disconnected();
		btcall->sco_established = 0;
	#ifdef CONFIG_UI_MANAGER
		btcall_switch_voice_source(true);
	#endif
		break;
	}

	case BT_RMT_VOL_SYNC_EVENT:
	{
		audio_system_set_stream_volume(AUDIO_STREAM_VOICE, msg->value);

		if (btcall && btcall->player) {
			media_player_set_volume(btcall->player, msg->value, msg->value);
		}
		break;
	}

	case BT_REQ_RESTART_PLAY:
	{
		bt_call_stop_play();
		if(btcall->sco_established)
		{
			bt_call_start_play();
		}
		break;
	}

	default:
		break;
	}
}

#ifdef CONFIG_BT_HFP_HF
static void _btcall_key_func_switch_mic_mute(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	btcall->mic_mute ^= 1;

	audio_system_mute_microphone(btcall->mic_mute);

	if (btcall->mic_mute)
		sys_event_notify(SYS_EVENT_MIC_MUTE_ON);
	else
		sys_event_notify(SYS_EVENT_MIC_MUTE_OFF);
}
#endif

static void _btcall_key_func_volume_adjust(int updown)
{
	int volume = 0;
	struct btcall_app_t *btcall = btcall_get_app();

	if (updown) {
		volume = system_volume_up(AUDIO_STREAM_VOICE, 1);
	} else {
		volume = system_volume_down(AUDIO_STREAM_VOICE, 1);
	}

	if (btcall->player) {
		media_player_set_volume(btcall->player, volume, volume);
	}
}

void btcall_input_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("btcall: input cmd %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_BT_CALL_VOLUP:
	{
		_btcall_key_func_volume_adjust(1);
		break;
	}
	case MSG_BT_CALL_VOLDOWN:
	{
		_btcall_key_func_volume_adjust(0);
		break;
	}
#ifdef CONFIG_BT_HFP_HF
	case MSG_BT_ACCEPT_CALL:
	{
		bt_manager_hfp_accept_call();
		break;
	}
	case MSG_BT_REJECT_CALL:
	{
		bt_manager_hfp_reject_call();
		break;
	}
	case MSG_BT_HANGUP_CALL:
	{
		bt_manager_hfp_hangup_call();
		break;
	}
	case MSG_BT_HANGUP_ANOTHER:
	{
		bt_manager_hfp_hangup_another_call();
		break;
	}
	case MSG_BT_HOLD_CURR_ANSWER_ANOTHER:
	{
		bt_manager_hfp_holdcur_answer_call();
		break;
	}
	case MSG_BT_HANGUP_CURR_ANSER_ANOTHER:
	{
		bt_manager_hfp_hangupcur_answer_call();
		break;
	}
	case MSG_BT_CALL_SWITCH_CALLOUT:
	{
		bt_manager_hfp_switch_sound_source();
		break;
	}
	case MSG_BT_CALL_SWITCH_MICMUTE:
	{
		_btcall_key_func_switch_mic_mute();
		break;
	}
	case MSG_BT_CALL_LAST_NO:
	{
		bt_manager_hfp_dial_number(NULL);
		break;
	}
	case MSG_BT_SIRI_START:
	{
		bt_manager_hfp_start_siri();
		break;
	}
	case MSG_BT_SIRI_STOP:
	{
		if (bt_manager_hfp_get_status() == BT_STATUS_SIRI)
			bt_manager_hfp_stop_siri();
		break;
	}
#endif
	case MSG_BT_HID_START:
	default:
		break;
	}
}

bool btcall_key_event_proc(uint32_t event)
{
	struct btcall_app_t *btcall = btcall_get_app();
	struct app_msg msg = { .type = MSG_INPUT_EVENT, .reserve = BTCALL_PLAYER };
	bool res = false;

	switch (event) {
#ifdef CONFIG_BT_HFP_HF
	case (KEY_POWER | KEY_TYPE_SHORT_UP):
		/* stop siri process */
		switch(bt_manager_hfp_get_status()) {
			case BT_STATUS_SIRI:
				SYS_LOG_INF("in siri");
				msg.cmd = MSG_BT_SIRI_STOP;
				res = true;
				break;
			case BT_STATUS_INCOMING:
				btcall_accept_call();
				break;
			case BT_STATUS_ONGOING:
			case BT_STATUS_OUTGOING:
				if(btcall->sco_established)
					btcall_handup_call();
				break;
		}
		break;
#endif

	case (KEY_TBD | KEY_TYPE_LONG_DOWN):
		/* start siri process */
		msg.cmd = MSG_BT_SIRI_START;
		res = true;
		break;

	default:
		break;
	}

	if (res) {
		send_async_msg(APP_ID_LAUNCHER, &msg);
	}

	return res;
}

void btcall_tts_event_proc(struct app_msg *msg)
{
	switch (msg->value) {
	case TTS_EVENT_NEXT_PLAY:
   		btcall_ring_check_next_play();
		break;
	default:
		break;
	}
}

void btcall_accept_call(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_ACCEPT_CALL,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("accept call\n");
}

void btcall_reject_call(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_REJECT_CALL,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("reject call\n");
}

void btcall_handup_call(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_HANGUP_CALL,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("handup call\n");
}

void btcall_vol_adjust(bool is_up)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_CALL_VOLDOWN,
		.reserve = BTCALL_PLAYER,
	};

	if (is_up)
		msg.cmd = MSG_BT_CALL_VOLUP;
	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("%d\n", is_up);
}

void btcall_switch_micmute(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_CALL_SWITCH_MICMUTE,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("switch micmute\n");
}

void btcall_hangupcur_answer_another(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_HANGUP_CURR_ANSER_ANOTHER,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("hangupcur answer another\n");
}

void btcall_holdcur_answer_another(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_HOLD_CURR_ANSWER_ANOTHER,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("holdcur answer another\n");
}

void btcall_hangup_another(void)
{
	struct app_msg msg = {
		.type = MSG_INPUT_EVENT,
		.cmd = MSG_BT_HANGUP_ANOTHER,
		.reserve = BTCALL_PLAYER,
	};

	send_async_msg(app_manager_get_current_app(), &msg);

	SYS_LOG_INF("hangup another\n");
}

bool btcall_sco_is_established(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	if (btcall && btcall->sco_established)
		return true;

	return false;
}

void btcall_play_next_tts(void)
{
	struct app_msg msg = {
		.type = MSG_TTS_EVENT,
		.value = TTS_EVENT_NEXT_PLAY,
	};

	send_async_msg(APP_ID_LAUNCHER, &msg);

	SYS_LOG_INF("play next \n");
}
