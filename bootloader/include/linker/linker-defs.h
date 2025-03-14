/*
 * Copyright (c) 2013-2014, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DESCRIPTION
 * Platform independent, commonly used macros and defines related to linker
 * script.
 *
 * This file may be included by:
 * - Linker script files: for linker section declarations
 * - C files: for external declaration of address or size of linker section
 * - Assembly files: for external declaration of address or size of linker
 *   section
 */

#ifndef ZEPHYR_INCLUDE_LINKER_LINKER_DEFS_H_
#define ZEPHYR_INCLUDE_LINKER_LINKER_DEFS_H_

#include <toolchain.h>
#include <toolchain/common.h>
#include <linker/sections.h>
#include <sys/util.h>
#include <offsets.h>

/* We need to dummy out DT_NODE_HAS_STATUS when building the unittests.
 * Including devicetree.h would require generating dummy header files
 * to match what gen_defines creates, so it's easier to just dummy out
 * DT_NODE_HAS_STATUS.
 */
#ifdef ZTEST_UNITTEST
#define DT_NODE_HAS_STATUS(node, status) 0
#else
#include <linker/devicetree_reserved.h>
#include <devicetree.h>
#endif

#ifdef _LINKER

/**
 * @addtogroup iterable_section_apis
 * @{
 */

#define Z_LINK_ITERABLE(struct_type) \
	_CONCAT(_##struct_type, _list_start) = .; \
	KEEP(*(SORT_BY_NAME(._##struct_type.static.*))); \
	_CONCAT(_##struct_type, _list_end) = .

#define Z_LINK_ITERABLE_ALIGNED(struct_type, align) \
	. = ALIGN(align); \
	Z_LINK_ITERABLE(struct_type);

#define Z_LINK_ITERABLE_GC_ALLOWED(struct_type) \
	_CONCAT(_##struct_type, _list_start) = .; \
	*(SORT_BY_NAME(._##struct_type.static.*)); \
	_CONCAT(_##struct_type, _list_end) = .

/**
 * @brief Define a read-only iterable section output.
 *
 * @details
 * Define an output section which will set up an iterable area
 * of equally-sized data structures. For use with STRUCT_SECTION_ITERABLE().
 * Input sections will be sorted by name, per ld's SORT_BY_NAME.
 *
 * This macro should be used for read-only data.
 *
 * Note that this keeps the symbols in the image even though
 * they are not being directly referenced. Use this when symbols
 * are indirectly referenced by iterating through the section.
 */
#define ITERABLE_SECTION_ROM(struct_type, subalign) \
	SECTION_PROLOGUE(struct_type##_area,,SUBALIGN(subalign)) \
	{ \
		Z_LINK_ITERABLE(struct_type); \
	} GROUP_ROM_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)

#define Z_ITERABLE_SECTION_ROM(struct_type, subalign) \
	ITERABLE_SECTION_ROM(struct_type, subalign)

/**
 * @brief Define a garbage collectable read-only iterable section output.
 *
 * @details
 * Define an output section which will set up an iterable area
 * of equally-sized data structures. For use with STRUCT_SECTION_ITERABLE().
 * Input sections will be sorted by name, per ld's SORT_BY_NAME.
 *
 * This macro should be used for read-only data.
 *
 * Note that the symbols within the section can be garbage collected.
 */
#define ITERABLE_SECTION_ROM_GC_ALLOWED(struct_type, subalign) \
	SECTION_PROLOGUE(struct_type##_area,,SUBALIGN(subalign)) \
	{ \
		Z_LINK_ITERABLE_GC_ALLOWED(struct_type); \
	} GROUP_LINK_IN(ROMABLE_REGION)

#define Z_ITERABLE_SECTION_ROM_GC_ALLOWED(struct_type, subalign) \
	ITERABLE_SECTION_ROM_GC_ALLOWED(struct_type, subalign)

/**
 * @brief Define a read-write iterable section output.
 *
 * @details
 * Define an output section which will set up an iterable area
 * of equally-sized data structures. For use with STRUCT_SECTION_ITERABLE().
 * Input sections will be sorted by name, per ld's SORT_BY_NAME.
 *
 * This macro should be used for read-write data that is modified at runtime.
 *
 * Note that this keeps the symbols in the image even though
 * they are not being directly referenced. Use this when symbols
 * are indirectly referenced by iterating through the section.
 */
#define ITERABLE_SECTION_RAM(struct_type, subalign) \
	SECTION_DATA_PROLOGUE(struct_type##_area,,SUBALIGN(subalign)) \
	{ \
		Z_LINK_ITERABLE(struct_type); \
	} GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)

#define Z_ITERABLE_SECTION_RAM(struct_type, subalign) \
	ITERABLE_SECTION_RAM(struct_type, subalign)

/**
 * @brief Define a garbage collectable read-write iterable section output.
 *
 * @details
 * Define an output section which will set up an iterable area
 * of equally-sized data structures. For use with STRUCT_SECTION_ITERABLE().
 * Input sections will be sorted by name, per ld's SORT_BY_NAME.
 *
 * This macro should be used for read-write data that is modified at runtime.
 *
 * Note that the symbols within the section can be garbage collected.
 */
#define ITERABLE_SECTION_RAM_GC_ALLOWED(struct_type, subalign) \
	SECTION_DATA_PROLOGUE(struct_type##_area,,SUBALIGN(subalign)) \
	{ \
		Z_LINK_ITERABLE_GC_ALLOWED(struct_type); \
	} GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)

#define Z_ITERABLE_SECTION_RAM_GC_ALLOWED(struct_type, subalign) \
	ITERABLE_SECTION_RAM_GC_ALLOWED(struct_type, subalign)

/**
 * @}
 */ /* end of struct_section_apis */

/*
 * generate a symbol to mark the start of the objects array for
 * the specified object and level, then link all of those objects
 * (sorted by priority). Ensure the objects aren't discarded if there is
 * no direct reference to them
 */
#define CREATE_OBJ_LEVEL(object, level)				\
		__##object##_##level##_start = .;		\
		KEEP(*(SORT(.z_##object##_##level[0-9]_*)));		\
		KEEP(*(SORT(.z_##object##_##level[1-9][0-9]_*)));

/*
 * link in shell initialization objects for all modules that use shell and
 * their shell commands are automatically initialized by the kernel.
 */

#elif defined(_ASMLANGUAGE)

/* Assembly FILES: declaration defined by the linker script */
GDATA(__bss_start)
GDATA(__bss_num_words)
#ifdef CONFIG_XIP
GDATA(__data_region_load_start)
GDATA(__data_region_start)
GDATA(__data_region_num_words)
#endif

#else /* ! _ASMLANGUAGE */

#include <zephyr/types.h>
/*
 * Memory owned by the kernel, to be used as shared memory between
 * application threads.
 *
 * The following are extern symbols from the linker. This enables
 * the dynamic k_mem_domain and k_mem_partition creation and alignment
 * to the section produced in the linker.

 * The policy for this memory will be to initially configure all of it as
 * kernel / supervisor thread accessible.
 */
extern char _app_smem_start[];
extern char _app_smem_end[];
extern char _app_smem_size[];
extern char _app_smem_rom_start[];
extern char _app_smem_num_words[];

#ifdef CONFIG_LINKER_USE_PINNED_SECTION
extern char _app_smem_pinned_start[];
extern char _app_smem_pinned_end[];
extern char _app_smem_pinned_size[];
extern char _app_smem_pinned_num_words[];
#endif

/* Memory owned by the kernel. Start and end will be aligned for memory
 * management/protection hardware for the target architecture.
 *
 * Consists of all kernel-side globals, all kernel objects, all thread stacks,
 * and all currently unused RAM.
 *
 * Except for the stack of the currently executing thread, none of this memory
 * is normally accessible to user threads unless specifically granted at
 * runtime.
 */
extern char __kernel_ram_start[];
extern char __kernel_ram_end[];
extern char __kernel_ram_size[];

extern char __kernel_ram_save_end[];


/* Used by z_bss_zero or arch-specific implementation */
extern char __bss_start[];
extern char __bss_end[];

/* Used by z_psram_bss_zero or arch-specific implementation */
extern char __psram_bss_start[];
extern char __psram_bss_end[];

/* Used by z_sram_bss_zero or arch-specific implementation */
extern char __sram_bss_start[];
extern char __sram_bss_end[];

/* Used by z_data_copy() or arch-specific implementation */
#ifdef CONFIG_XIP
extern char __data_region_load_start[];
extern char __data_region_start[];
extern char __data_region_end[];
#endif /* CONFIG_XIP */

#ifdef CONFIG_MMU
/* Virtual addresses of page-aligned kernel image mapped into RAM at boot */
extern char z_mapped_start[];
extern char z_mapped_end[];
#endif /* CONFIG_MMU */

/* Includes text and rodata */
extern char __rom_region_start[];
extern char __rom_region_end[];
extern char __rom_region_size[];

/* Includes all ROMable data, i.e. the size of the output image file. */
extern char _flash_used[];

/* datas, bss, noinit */
extern char _image_ram_start[];
extern char _image_ram_end[];

extern char __text_region_start[];
extern char __text_region_end[];
extern char __text_region_size[];

extern char __rodata_region_start[];
extern char __rodata_region_end[];
extern char __rodata_region_size[];

extern char _vector_start[];
extern char _vector_end[];

/* Used by soc_sleep for saving power through shutting down sram */
extern char _sleep_shutdown_ram_start[];
extern char _sleep_shutdown_ram_end[];

#if DT_NODE_HAS_STATUS(_NODE_RESERVED, okay)
LINKER_DT_RESERVED_MEM_SYMBOLS()
#endif

#ifdef CONFIG_SW_VECTOR_RELAY
extern char __vector_relay_table[];
#endif

#ifdef CONFIG_COVERAGE_GCOV
extern char __gcov_bss_start[];
extern char __gcov_bss_end[];
extern char __gcov_bss_size[];
#endif	/* CONFIG_COVERAGE_GCOV */

/* end address of image, used by newlib for the heap */
extern char _end[];

/* The total size used in flash device */
extern char _flash_used[];

#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_ccm), okay)
extern char __ccm_data_rom_start[];
extern char __ccm_start[];
extern char __ccm_data_start[];
extern char __ccm_data_end[];
extern char __ccm_bss_start[];
extern char __ccm_bss_end[];
extern char __ccm_noinit_start[];
extern char __ccm_noinit_end[];
extern char __ccm_end[];
#endif

#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_itcm), okay)
extern char __itcm_start[];
extern char __itcm_end[];
extern char __itcm_size[];
extern char __itcm_load_start[];
#endif

#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_dtcm), okay)
extern char __dtcm_data_start[];
extern char __dtcm_data_end[];
extern char __dtcm_bss_start[];
extern char __dtcm_bss_end[];
extern char __dtcm_noinit_start[];
extern char __dtcm_noinit_end[];
extern char __dtcm_data_load_start[];
extern char __dtcm_start[];
extern char __dtcm_end[];
#endif

/* Used by the Security Attribution Unit to configure the
 * Non-Secure Callable region.
 */
#ifdef CONFIG_ARM_FIRMWARE_HAS_SECURE_ENTRY_FUNCS
extern char __sg_start[];
extern char __sg_end[];
extern char __sg_size[];
#endif /* CONFIG_ARM_FIRMWARE_HAS_SECURE_ENTRY_FUNCS */

/*
 * Non-cached kernel memory region, currently only available on ARM Cortex-M7
 * with a MPU. Start and end will be aligned for memory management/protection
 * hardware for the target architecture.
 *
 * All the functions with '__nocache' keyword will be placed into this
 * section.
 */
#ifdef CONFIG_NOCACHE_MEMORY
extern char _nocache_ram_start[];
extern char _nocache_ram_end[];
extern char _nocache_ram_size[];
#endif /* CONFIG_NOCACHE_MEMORY */

/* Memory owned by the kernel. Start and end will be aligned for memory
 * management/protection hardware for the target architecture.
 *
 * All the functions with '__ramfunc' keyword will be placed into this
 * section, stored in RAM instead of FLASH.
 */
#ifdef CONFIG_ARCH_HAS_RAMFUNC_SUPPORT
extern char __ramfunc_start[];
extern char __ramfunc_end[];
extern char __ramfunc_size[];
extern char __ramfunc_load_start[];
#endif /* CONFIG_ARCH_HAS_RAMFUNC_SUPPORT */

#ifdef CONFIG_SLEEP_FUNC_IN_SRAM
#define __sleepfunc	__attribute__((noinline))			\
			__attribute__((long_call, section(".sleepfunc")))
extern char _sram_data_start[];
extern char _sram_data_end[];
extern char _sram_data_ram_size[];
extern char _sram_data_rom_start[];
#else
#define __sleepfunc __ramfunc
#endif

#define __de_func	__attribute__((noinline))			\
			__attribute__((long_call, section(".defunc")))

#define __lvgl_func	__attribute__((noinline))			\
			__attribute__((long_call, section(".lvglfunc")))

#ifdef CONFIG_SIM_FLASH_ACTS
extern char __sim_flash_ram_start[];
extern char __sim_flash_ram_end[];
#endif

/* Memory owned by the kernel. Memory region for thread privilege stack buffers,
 * currently only applicable on ARM Cortex-M architecture when building with
 * support for User Mode.
 *
 * All thread privilege stack buffers will be placed into this section.
 */
#ifdef CONFIG_USERSPACE
extern char z_priv_stacks_ram_start[];
extern char z_priv_stacks_ram_end[];
extern char z_user_stacks_start[];
extern char z_user_stacks_end[];
extern char z_kobject_data_begin[];
#endif /* CONFIG_USERSPACE */

#ifdef CONFIG_THREAD_LOCAL_STORAGE
extern char __tdata_start[];
extern char __tdata_end[];
extern char __tdata_size[];
extern char __tdata_align[];
extern char __tbss_start[];
extern char __tbss_end[];
extern char __tbss_size[];
extern char __tbss_align[];
extern char __tls_start[];
extern char __tls_end[];
extern char __tls_size[];
#endif /* CONFIG_THREAD_LOCAL_STORAGE */

#ifdef CONFIG_LINKER_USE_BOOT_SECTION
/* lnkr_boot_start[] and lnkr_boot_end[]
 * must encapsulate all the boot sections.
 */
extern char lnkr_boot_start[];
extern char lnkr_boot_end[];

extern char lnkr_boot_text_start[];
extern char lnkr_boot_text_end[];
extern char lnkr_boot_text_size[];
extern char lnkr_boot_data_start[];
extern char lnkr_boot_data_end[];
extern char lnkr_boot_data_size[];
extern char lnkr_boot_rodata_start[];
extern char lnkr_boot_rodata_end[];
extern char lnkr_boot_rodata_size[];
extern char lnkr_boot_bss_start[];
extern char lnkr_boot_bss_end[];
extern char lnkr_boot_bss_size[];
extern char lnkr_boot_noinit_start[];
extern char lnkr_boot_noinit_end[];
extern char lnkr_boot_noinit_size[];
#endif /* CONFIG_LINKER_USE_BOOT_SECTION */

#ifdef CONFIG_LINKER_USE_PINNED_SECTION
/* lnkr_pinned_start[] and lnkr_pinned_end[] must encapsulate
 * all the pinned sections as these are used by
 * the MMU code to mark the physical page frames with
 * Z_PAGE_FRAME_PINNED.
 */
extern char lnkr_pinned_start[];
extern char lnkr_pinned_end[];

extern char lnkr_pinned_text_start[];
extern char lnkr_pinned_text_end[];
extern char lnkr_pinned_text_size[];
extern char lnkr_pinned_data_start[];
extern char lnkr_pinned_data_end[];
extern char lnkr_pinned_data_size[];
extern char lnkr_pinned_rodata_start[];
extern char lnkr_pinned_rodata_end[];
extern char lnkr_pinned_rodata_size[];
extern char lnkr_pinned_bss_start[];
extern char lnkr_pinned_bss_end[];
extern char lnkr_pinned_bss_size[];
extern char lnkr_pinned_noinit_start[];
extern char lnkr_pinned_noinit_end[];
extern char lnkr_pinned_noinit_size[];

__pinned_func
static inline bool lnkr_is_pinned(uint8_t *addr)
{
	if ((addr >= (uint8_t *)lnkr_pinned_start) &&
	    (addr < (uint8_t *)lnkr_pinned_end)) {
		return true;
	} else {
		return false;
	}
}

__pinned_func
static inline bool lnkr_is_region_pinned(uint8_t *addr, size_t sz)
{
	if ((addr >= (uint8_t *)lnkr_pinned_start) &&
	    ((addr + sz) < (uint8_t *)lnkr_pinned_end)) {
		return true;
	} else {
		return false;
	}
}

#endif /* CONFIG_LINKER_USE_PINNED_SECTION */

#endif /* ! _ASMLANGUAGE */

#endif /* ZEPHYR_INCLUDE_LINKER_LINKER_DEFS_H_ */
