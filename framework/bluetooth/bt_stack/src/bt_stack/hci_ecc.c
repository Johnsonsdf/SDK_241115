/**
 * @file hci_ecc.c
 * HCI ECC emulation
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/atomic.h>
#include <debug/stack.h>
#include <sys/byteorder.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/utils.h>
#include <tinycrypt/ecc.h>
#include <tinycrypt/ecc_dh.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/hci.h>
#include <drivers/bluetooth/hci_driver.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_CORE)
#define LOG_MODULE_NAME bt_hci_ecc
#include "common/log.h"

#include "hci_ecc.h"
#ifdef CONFIG_BT_HCI_RAW
#include <acts_bluetooth/hci_raw.h>
#include "hci_raw_internal.h"
#else
#include "hci_core.h"
#endif

#define HCI_ECC_DYNAMIC_THREAD		1

#if	HCI_ECC_DYNAMIC_THREAD
#include "common_internal.h"

#define ECC_STACK_SIZE		(CONFIG_BT_HCI_ECC_STACK_SIZE + sizeof(struct k_thread))
static uint8_t *ecc_thread_stack;

static int ecc_start_thread(void);
#else
static struct k_thread ecc_thread_data;
static K_KERNEL_STACK_DEFINE(ecc_thread_stack, CONFIG_BT_HCI_ECC_STACK_SIZE);
#endif

/* based on Core Specification 4.2 Vol 3. Part H 2.3.5.6.1 */
static const uint8_t debug_private_key_be[32] = {
	0x3f, 0x49, 0xf6, 0xd4, 0xa3, 0xc5, 0x5f, 0x38,
	0x74, 0xc9, 0xb3, 0xe3, 0xd2, 0x10, 0x3f, 0x50,
	0x4a, 0xff, 0x60, 0x7b, 0xeb, 0x40, 0xb7, 0x99,
	0x58, 0x99, 0xb8, 0xa6, 0xcd, 0x3c, 0x1a, 0xbd,
};

#if defined(CONFIG_BT_USE_DEBUG_KEYS)
static const uint8_t debug_public_key[64] = {
	0xe6, 0x9d, 0x35, 0x0e, 0x48, 0x01, 0x03, 0xcc, 0xdb, 0xfd, 0xf4, 0xac,
	0x11, 0x91, 0xf4, 0xef, 0xb9, 0xa5, 0xf9, 0xe9, 0xa7, 0x83, 0x2c, 0x5e,
	0x2c, 0xbe, 0x97, 0xf2, 0xd2, 0x03, 0xb0, 0x20, 0x8b, 0xd2, 0x89, 0x15,
	0xd0, 0x8e, 0x1c, 0x74, 0x24, 0x30, 0xed, 0x8f, 0xc2, 0x45, 0x63, 0x76,
	0x5c, 0x15, 0x52, 0x5a, 0xbf, 0x9a, 0x32, 0x63, 0x6d, 0xeb, 0x2a, 0x65,
	0x49, 0x9c, 0x80, 0xdc
};
#endif

enum {
	PENDING_PUB_KEY,
	PENDING_DHKEY,
	PENDING_EXIT,
	ECC_DEINIT,
	/* Total number of flags - must be at the end of the enum */
	NUM_FLAGS,
};

static ATOMIC_DEFINE(flags, NUM_FLAGS);

#if	HCI_ECC_DYNAMIC_THREAD
static void hci_ecc_finish_work(struct k_work *work);
K_WORK_DEFINE(ecc_finish_work, hci_ecc_finish_work);
#else
static K_SEM_DEFINE(cmd_sem, 0, 1);
#endif

static struct {
	uint8_t private_key_be[32];

	union {
		uint8_t public_key_be[64];
		uint8_t dhkey_be[32];
	};
} ecc;

static void send_cmd_status(uint16_t opcode, uint8_t status)
{
	struct bt_hci_evt_cmd_status *evt;
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;

	BT_DBG("opcode %x status %x", opcode, status);

	buf = bt_buf_get_evt(BT_HCI_EVT_CMD_STATUS, false, K_FOREVER);
	bt_buf_set_type(buf, BT_BUF_EVT);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = BT_HCI_EVT_CMD_STATUS;
	hdr->len = sizeof(*evt);

	evt = net_buf_add(buf, sizeof(*evt));
	evt->ncmd = 1U;
	evt->opcode = sys_cpu_to_le16(opcode);
	evt->status = status;

	if (IS_ENABLED(CONFIG_BT_RECV_IS_RX_THREAD)) {
		bt_recv_prio(buf);
	} else {
		bt_recv(buf);
	}
}

static uint8_t generate_keys(void)
{
#if !defined(CONFIG_BT_USE_DEBUG_KEYS)
	do {
		int rc;

		rc = uECC_make_key(ecc.public_key_be, ecc.private_key_be,
				   &curve_secp256r1);
		if (rc == TC_CRYPTO_FAIL) {
			BT_ERR("Failed to create ECC public/private pair");
			return BT_HCI_ERR_UNSPECIFIED;
		}

	/* make sure generated key isn't debug key */
	} while (memcmp(ecc.private_key_be, debug_private_key_be, 32) == 0);
#else
	sys_memcpy_swap(ecc.public_key_be, debug_public_key, 32);
	sys_memcpy_swap(&ecc.public_key_be[32], &debug_public_key[32], 32);
	memcpy(ecc.private_key_be, debug_private_key_be, 32);
#endif
	return 0;
}

static void emulate_le_p256_public_key_cmd(void)
{
	struct bt_hci_evt_le_p256_public_key_complete *evt;
	struct bt_hci_evt_le_meta_event *meta;
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;
	uint8_t status;

	BT_DBG("");

	status = generate_keys();

	buf = bt_buf_get_rx(BT_BUF_EVT, K_FOREVER);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = BT_HCI_EVT_LE_META_EVENT;
	hdr->len = sizeof(*meta) + sizeof(*evt);

	meta = net_buf_add(buf, sizeof(*meta));
	meta->subevent = BT_HCI_EVT_LE_P256_PUBLIC_KEY_COMPLETE;

	evt = net_buf_add(buf, sizeof(*evt));
	evt->status = status;

	if (status) {
		(void)memset(evt->key, 0, sizeof(evt->key));
	} else {
		/* Convert X and Y coordinates from big-endian (provided
		 * by crypto API) to little endian HCI.
		 */
		sys_memcpy_swap(evt->key, ecc.public_key_be, 32);
		sys_memcpy_swap(&evt->key[32], &ecc.public_key_be[32], 32);
	}

	atomic_clear_bit(flags, PENDING_PUB_KEY);

	bt_recv(buf);
}

static void emulate_le_generate_dhkey(void)
{
	struct bt_hci_evt_le_generate_dhkey_complete *evt;
	struct bt_hci_evt_le_meta_event *meta;
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;
	int ret;

	ret = uECC_valid_public_key(ecc.public_key_be, &curve_secp256r1);
	if (ret < 0) {
		BT_ERR("public key is not valid (ret %d)", ret);
		ret = TC_CRYPTO_FAIL;
	} else {
		ret = uECC_shared_secret(ecc.public_key_be, ecc.private_key_be,
					 ecc.dhkey_be, &curve_secp256r1);
	}

	buf = bt_buf_get_rx(BT_BUF_EVT, K_FOREVER);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = BT_HCI_EVT_LE_META_EVENT;
	hdr->len = sizeof(*meta) + sizeof(*evt);

	meta = net_buf_add(buf, sizeof(*meta));
	meta->subevent = BT_HCI_EVT_LE_GENERATE_DHKEY_COMPLETE;

	evt = net_buf_add(buf, sizeof(*evt));

	if (ret == TC_CRYPTO_FAIL) {
		evt->status = BT_HCI_ERR_UNSPECIFIED;
		(void)memset(evt->dhkey, 0xff, sizeof(evt->dhkey));
	} else {
		evt->status = 0U;
		/* Convert from big-endian (provided by crypto API) to
		 * little-endian HCI.
		 */
		sys_memcpy_swap(evt->dhkey, ecc.dhkey_be, sizeof(ecc.dhkey_be));
	}

	atomic_clear_bit(flags, PENDING_DHKEY);

	bt_recv(buf);
}

#ifndef	HCI_ECC_DYNAMIC_THREAD
static void ecc_thread(void *p1, void *p2, void *p3)
{
	while (true) {
		k_sem_take(&cmd_sem, K_FOREVER);

		if (atomic_test_bit(flags, PENDING_EXIT)) {
			atomic_clear_bit(flags, PENDING_EXIT);
			return;
		} else if (atomic_test_bit(flags, PENDING_PUB_KEY)) {
			emulate_le_p256_public_key_cmd();
		} else if (atomic_test_bit(flags, PENDING_DHKEY)) {
			emulate_le_generate_dhkey();
		} else {
			__ASSERT(0, "Unhandled ECC command");
		}
	}
}
#endif

static void clear_ecc_events(struct net_buf *buf)
{
	struct bt_hci_cp_le_set_event_mask *cmd;

	cmd = (void *)(buf->data + sizeof(struct bt_hci_cmd_hdr));

	/*
	 * don't enable controller ECC events as those will be generated from
	 * emulation code
	 */
	cmd->events[0] &= ~0x80; /* LE Read Local P-256 PKey Compl */
	cmd->events[1] &= ~0x01; /* LE Generate DHKey Compl Event */
}

static void le_gen_dhkey(struct net_buf *buf)
{
	struct bt_hci_cp_le_generate_dhkey *cmd;
	uint8_t status;

	if (atomic_test_bit(flags, PENDING_PUB_KEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}

	if (buf->len < sizeof(struct bt_hci_cp_le_generate_dhkey)) {
		status = BT_HCI_ERR_INVALID_PARAM;
		goto send_status;
	}

	if (atomic_test_and_set_bit(flags, PENDING_DHKEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}

#if	HCI_ECC_DYNAMIC_THREAD
	if (ecc_start_thread()) {
		atomic_clear_bit(flags, PENDING_DHKEY);
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}
#endif

	cmd = (void *)buf->data;
	/* Convert X and Y coordinates from little-endian HCI to
	 * big-endian (expected by the crypto API).
	 */
	sys_memcpy_swap(ecc.public_key_be, cmd->key, 32);
	sys_memcpy_swap(&ecc.public_key_be[32], &cmd->key[32], 32);
#if	(HCI_ECC_DYNAMIC_THREAD == 0)
	k_sem_give(&cmd_sem);
#endif
	status = BT_HCI_ERR_SUCCESS;

send_status:
	net_buf_unref(buf);
	send_cmd_status(BT_HCI_OP_LE_GENERATE_DHKEY, status);
}

static void le_p256_pub_key(struct net_buf *buf)
{
	uint8_t status;

	net_buf_unref(buf);

	if (atomic_test_bit(flags, PENDING_DHKEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
	} else if (atomic_test_and_set_bit(flags, PENDING_PUB_KEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
	} else {
#if	HCI_ECC_DYNAMIC_THREAD
		if (ecc_start_thread()) {
			atomic_clear_bit(flags, PENDING_PUB_KEY);
			status = BT_HCI_ERR_CMD_DISALLOWED;
		} else {
			status = BT_HCI_ERR_SUCCESS;
		}
#else
		k_sem_give(&cmd_sem);
		status = BT_HCI_ERR_SUCCESS;
#endif
	}

	send_cmd_status(BT_HCI_OP_LE_P256_PUBLIC_KEY, status);
}

int bt_hci_ecc_send(struct net_buf *buf)
{
	if (bt_buf_get_type(buf) == BT_BUF_CMD) {
		struct bt_hci_cmd_hdr *chdr = (void *)buf->data;

		switch (sys_le16_to_cpu(chdr->opcode)) {
		case BT_HCI_OP_LE_P256_PUBLIC_KEY:
			net_buf_pull(buf, sizeof(*chdr));
			le_p256_pub_key(buf);
			return 0;
		case BT_HCI_OP_LE_GENERATE_DHKEY:
			net_buf_pull(buf, sizeof(*chdr));
			le_gen_dhkey(buf);
			return 0;
		case BT_HCI_OP_LE_SET_EVENT_MASK:
			clear_ecc_events(buf);
			break;
		default:
			break;
		}
	}

	hci_data_log_debug(true, buf);
	return bt_dev.drv->send(buf);
}

int default_CSPRNG(uint8_t *dst, unsigned int len)
{
	return !bt_rand(dst, len);
}

void bt_hci_ecc_init(void)
{
#if	HCI_ECC_DYNAMIC_THREAD
	atomic_clear_bit(flags, ECC_DEINIT);
#else
	k_thread_create(&ecc_thread_data, ecc_thread_stack,
			K_KERNEL_STACK_SIZEOF(ecc_thread_stack), ecc_thread,
			NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
	k_thread_name_set(&ecc_thread_data, "BT ECC");
#endif
}

/* Actions add start */

#if	HCI_ECC_DYNAMIC_THREAD
static void hci_ecc_finish_work(struct k_work *work)
{
	bt_free(ecc_thread_stack);
	ecc_thread_stack = NULL;

	atomic_clear_bit(flags, PENDING_EXIT);
}

static void ecc_thread(void *p1, void *p2, void *p3)
{
	if (atomic_test_bit(flags, PENDING_PUB_KEY)) {
		emulate_le_p256_public_key_cmd();
		atomic_clear_bit(flags, PENDING_PUB_KEY);
	} else if (atomic_test_bit(flags, PENDING_DHKEY)) {
		emulate_le_generate_dhkey();
		atomic_clear_bit(flags, PENDING_DHKEY);
	} else {
		__ASSERT(0, "Unhandled ECC command");
	}

	k_thread_priority_set(k_current_get(), -CONFIG_NUM_COOP_PRIORITIES);
	k_work_submit(&ecc_finish_work);
}

static int ecc_start_thread(void)
{
	if (atomic_test_bit(flags, ECC_DEINIT)) {
		return -EIO;
	}

	ecc_thread_stack = bt_malloc(ECC_STACK_SIZE);
	if (!ecc_thread_stack) {
		return -ENOMEM;
	}

	atomic_set_bit(flags, PENDING_EXIT);
	k_thread_create((struct k_thread *)ecc_thread_stack, (k_thread_stack_t *)&ecc_thread_stack[sizeof(struct k_thread)],
			(ECC_STACK_SIZE - sizeof(struct k_thread)), ecc_thread,
			NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

	return 0;
}
#endif

void bt_hci_ecc_deinit(void)
{
#if	HCI_ECC_DYNAMIC_THREAD
	atomic_set_bit(flags, ECC_DEINIT);
#else
	atomic_set_bit(flags, PENDING_EXIT);
	k_sem_give(&cmd_sem);
#endif

	while (atomic_test_bit(flags, PENDING_EXIT)) {
		k_sleep(K_MSEC(50));
	}
}

/* Actions add end */
