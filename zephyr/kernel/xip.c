/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr.h>
#include <kernel.h>
#include <string.h>
#include <linker/linker-defs.h>

#ifdef CONFIG_STACK_CANARIES
extern volatile uintptr_t __stack_chk_guard;
#endif /* CONFIG_STACK_CANARIES */

/**
 *
 * @brief Copy the data section from ROM to RAM
 *
 * This routine copies the data section from ROM to RAM.
 *
 * @return N/A
 */
void z_data_copy(void)
{
	(void)memcpy(&__data_region_start, &__data_region_load_start,
		 (uintptr_t)&__data_region_end - (uintptr_t)&__data_region_start);

#ifdef CONFIG_ARCH_HAS_RAMFUNC_SUPPORT
	(void)memcpy(&__ramfunc_start, &__ramfunc_load_start,
		 (uintptr_t) &__ramfunc_size);
#endif /* CONFIG_ARCH_HAS_RAMFUNC_SUPPORT */

#ifdef CONFIG_SLEEP_FUNC_IN_SRAM
	(void)memcpy(&_sram_data_start, &_sram_data_rom_start,
		 (uintptr_t) &_sram_data_ram_size);
#endif /* CONFIG_SLEEP_FUNC_IN_SRAM */

#ifdef CONFIG_SENSOR_ALGO
	if (__sensor_func_size > 0) {
		(void)memcpy(&__sensor_func_start, &__sensor_func_load_start,
		 	(uint32_t) &__sensor_func_size);
	}
	if (__sensor_data_size > 0) {
		(void)memcpy(&__sensor_data_start, &__sensor_data_load_start,
		 	(uint32_t) &__sensor_data_size);
	}
#endif

#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_ccm), okay)
	(void)memcpy(&__ccm_data_start, &__ccm_data_rom_start,
		 __ccm_data_end - __ccm_data_start);
#endif
#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_itcm), okay)
	(void)memcpy(&__itcm_start, &__itcm_load_start,
		 (uintptr_t) &__itcm_size);
#endif
#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_dtcm), okay)
	(void)memcpy(&__dtcm_data_start, &__dtcm_data_load_start,
		 __dtcm_data_end - __dtcm_data_start);
#endif
#ifdef CONFIG_CODE_DATA_RELOCATION
	extern void data_copy_xip_relocation(void);

	data_copy_xip_relocation();
#endif	/* CONFIG_CODE_DATA_RELOCATION */
#ifdef CONFIG_USERSPACE
#ifdef CONFIG_STACK_CANARIES
	/* stack canary checking is active for all C functions.
	 * __stack_chk_guard is some uninitialized value living in the
	 * app shared memory sections. Preserve it, and don't make any
	 * function calls to perform the memory copy. The true canary
	 * value gets set later in z_cstart().
	 */
	uintptr_t guard_copy = __stack_chk_guard;
	uint8_t *src = (uint8_t *)&_app_smem_rom_start;
	uint8_t *dst = (uint8_t *)&_app_smem_start;
	uint32_t count = _app_smem_end - _app_smem_start;

	guard_copy = __stack_chk_guard;
	while (count > 0) {
		*(dst++) = *(src++);
		count--;
	}
	__stack_chk_guard = guard_copy;
#else
	(void)memcpy(&_app_smem_start, &_app_smem_rom_start,
		 _app_smem_end - _app_smem_start);
#endif /* CONFIG_STACK_CANARIES */
#endif /* CONFIG_USERSPACE */
}
