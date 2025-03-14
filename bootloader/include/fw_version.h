/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief firmware version interface
 */

#ifndef __FW_VERSION_H__
#define __FW_VERSION_H__

#define FIRMWARE_VERSION_MAGIC	0x52455646   /* 'FVER' */
#define FIRMWARE_VERSION	"FW_VERS"   /* 'FVER' */
#define DEFAULT_VER_CODE	(0x10000)
#define DEFAULT_VER_RES	(0x10000)

struct fw_version {
	uint32_t magic;
	uint32_t version_code;
	uint32_t version_res;
	uint32_t system_version_code;
	char version_name[64];
	char board_name[32];
	uint8_t reserved1[12];
	uint32_t checksum;
};
struct code_res_version {
	uint32_t version_code;
	uint32_t version_res;
};
const struct fw_version *fw_version_get_current(void);
void fw_version_dump(const struct fw_version *ver);
int fw_version_check(const struct fw_version *ver);

static inline void fw_version_dump_current(void)
{
	fw_version_dump(fw_version_get_current());
}

static inline uint32_t fw_version_get_code(void)
{
	return fw_version_get_current()->version_code;
}

static inline uint32_t fw_version_get_res(void)
{
	return fw_version_get_current()->version_res;
}

static inline const char *fw_version_get_name(void)
{
	return fw_version_get_current()->version_name;
}

static inline const char *fw_version_get_board_name(void)
{
	return fw_version_get_current()->board_name;
}



#endif /* __FW_VERSION_H__ */
