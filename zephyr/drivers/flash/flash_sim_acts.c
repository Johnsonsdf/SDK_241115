/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/flash.h>
#include <init.h>
#include <kernel.h>
#include <sys/util.h>
#include <random/rand32.h>
#include <stats/stats.h>
#include <string.h>

#ifdef CONFIG_ARCH_POSIX

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "cmdline.h"
#include "soc.h"

#endif /* CONFIG_ARCH_POSIX */
#include <board_cfg.h>
#include <linker/linker-defs.h>

#define FLASH_SIMULATOR_BASE_OFFSET 0x0
#define FLASH_SIMULATOR_ERASE_UNIT 0x1000
#define FLASH_SIMULATOR_PROG_UNIT 0x1
#define FLASH_SIMULATOR_FLASH_SIZE CONFIG_SIM_FLASH_SIZE

#define FLASH_SIMULATOR_ERASE_VALUE 0xff

#define FLASH_SIMULATOR_PAGE_COUNT (FLASH_SIMULATOR_FLASH_SIZE / \
				    FLASH_SIMULATOR_ERASE_UNIT)

#if (FLASH_SIMULATOR_ERASE_UNIT % FLASH_SIMULATOR_PROG_UNIT)
#error "Erase unit must be a multiple of program unit"
#endif

#define MOCK_FLASH(addr) (mock_flash + (addr) - FLASH_SIMULATOR_BASE_OFFSET)

/* maximum number of pages that can be tracked by the stats module */
#define STATS_PAGE_COUNT_THRESHOLD 256

#define STATS_SECT_EC(N, _) STATS_SECT_ENTRY32(erase_cycles_unit##N)
#define STATS_NAME_EC(N, _) STATS_NAME(flash_sim_stats, erase_cycles_unit##N)

#define STATS_SECT_DIRTYR(N, _) STATS_SECT_ENTRY32(dirty_read_unit##N)
#define STATS_NAME_DIRTYR(N, _) STATS_NAME(flash_sim_stats, dirty_read_unit##N)

#ifdef CONFIG_FLASH_SIMULATOR_STATS
/* increment a unit erase cycles counter */
#define ERASE_CYCLES_INC(U)						     \
	do {								     \
		if (U < STATS_PAGE_COUNT_THRESHOLD) {			     \
			(*(&flash_sim_stats.erase_cycles_unit0 + (U)) += 1); \
		}							     \
	} while (0)

#if (CONFIG_FLASH_SIMULATOR_STAT_PAGE_COUNT > STATS_PAGE_COUNT_THRESHOLD)
       /* Limitation above is caused by used UTIL_REPEAT                    */
       /* Using FLASH_SIMULATOR_FLASH_PAGE_COUNT allows to avoid terrible   */
       /* error logg at the output and work with the stats module partially */
       #define FLASH_SIMULATOR_FLASH_PAGE_COUNT STATS_PAGE_COUNT_THRESHOLD
#else
#define FLASH_SIMULATOR_FLASH_PAGE_COUNT CONFIG_FLASH_SIMULATOR_STAT_PAGE_COUNT
#endif

/* simulator statistics */
STATS_SECT_START(flash_sim_stats)
STATS_SECT_ENTRY32(bytes_read)		/* total bytes read */
STATS_SECT_ENTRY32(bytes_written)       /* total bytes written */
STATS_SECT_ENTRY32(double_writes)       /* num. of writes to non-erased units */
STATS_SECT_ENTRY32(flash_read_calls)    /* calls to flash_read() */
STATS_SECT_ENTRY32(flash_read_time_us)  /* time spent in flash_read() */
STATS_SECT_ENTRY32(flash_write_calls)   /* calls to flash_write() */
STATS_SECT_ENTRY32(flash_write_time_us) /* time spent in flash_write() */
STATS_SECT_ENTRY32(flash_erase_calls)   /* calls to flash_erase() */
STATS_SECT_ENTRY32(flash_erase_time_us) /* time spent in flash_erase() */
/* -- per-unit statistics -- */
/* erase cycle count for unit */
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_SECT_EC))
/* number of read operations on worn out erase units */
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_SECT_DIRTYR))
STATS_SECT_END;

STATS_SECT_DECL(flash_sim_stats) flash_sim_stats;
STATS_NAME_START(flash_sim_stats)
STATS_NAME(flash_sim_stats, bytes_read)
STATS_NAME(flash_sim_stats, bytes_written)
STATS_NAME(flash_sim_stats, double_writes)
STATS_NAME(flash_sim_stats, flash_read_calls)
STATS_NAME(flash_sim_stats, flash_read_time_us)
STATS_NAME(flash_sim_stats, flash_write_calls)
STATS_NAME(flash_sim_stats, flash_write_time_us)
STATS_NAME(flash_sim_stats, flash_erase_calls)
STATS_NAME(flash_sim_stats, flash_erase_time_us)
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_NAME_EC))
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_NAME_DIRTYR))
STATS_NAME_END(flash_sim_stats);

/* simulator dynamic thresholds */
STATS_SECT_START(flash_sim_thresholds)
STATS_SECT_ENTRY32(max_write_calls)
STATS_SECT_ENTRY32(max_erase_calls)
STATS_SECT_ENTRY32(max_len)
STATS_SECT_END;

STATS_SECT_DECL(flash_sim_thresholds) flash_sim_thresholds;
STATS_NAME_START(flash_sim_thresholds)
STATS_NAME(flash_sim_thresholds, max_write_calls)
STATS_NAME(flash_sim_thresholds, max_erase_calls)
STATS_NAME(flash_sim_thresholds, max_len)
STATS_NAME_END(flash_sim_thresholds);

#define FLASH_SIM_STATS_INC(group__, var__) STATS_INC(group__, var__)
#define FLASH_SIM_STATS_INCN(group__, var__, n__) STATS_INCN(group__, var__, n__)
#define FLASH_SIM_STATS_INIT_AND_REG(group__, size__, name__) \
	STATS_INIT_AND_REG(group__, size__, name__)


#else

#define ERASE_CYCLES_INC(U) do {} while (0)
#define FLASH_SIM_STATS_INC(group__, var__)
#define FLASH_SIM_STATS_INCN(group__, var__, n__)
#define FLASH_SIM_STATS_INIT_AND_REG(group__, size__, name__)

#endif /* CONFIG_FLASH_SIMULATOR_STATS */


#ifdef CONFIG_ARCH_POSIX
static uint8_t *mock_flash;
static int flash_fd = -1;
static const char *flash_file_path;
static const char default_flash_file_path[] = "flash.bin";
#else
static uint8_t mock_flash[FLASH_SIMULATOR_FLASH_SIZE] __in_section_unique(sram.noinit.actlog);
#endif /* CONFIG_ARCH_POSIX */

static const struct flash_driver_api flash_sim_api;

static const struct flash_parameters flash_sim_parameters = {
	.write_block_size = 0x1000,
	.erase_value = FLASH_SIMULATOR_ERASE_VALUE
};
void print_buffer1(const uint8_t *addr, int width, int count, int linelen, unsigned long disp_addr);
int mock_flash_checksum_get(void)
{
    int i, checksum = 0;

    //print_buffer1(mock_flash, 4, 0x1000, 16, -1);
    uint32_t *pdata = (uint32_t *)mock_flash;
    for(i = 0; i < (FLASH_SIMULATOR_FLASH_SIZE / 4); i++){
        checksum += *pdata;
        pdata++;
    }

    return checksum;
}

static int flash_range_is_valid(const struct device *dev, uint64_t offset,
				uint64_t len)
{
	ARG_UNUSED(dev);
	if ((offset + len > FLASH_SIMULATOR_FLASH_SIZE +
			    FLASH_SIMULATOR_BASE_OFFSET) ||
	    (offset < FLASH_SIMULATOR_BASE_OFFSET)) {
		return 0;
	}

	return 1;
}

static int flash_sim_read(const struct device *dev, const uint64_t offset,
			  void *data,
			  const uint64_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	if (!IS_ENABLED(CONFIG_FLASH_SIMULATOR_UNALIGNED_READ)) {
		if ((offset % FLASH_SIMULATOR_PROG_UNIT) ||
		    (len % FLASH_SIMULATOR_PROG_UNIT)) {
			return -EINVAL;
		}
	}

	FLASH_SIM_STATS_INC(flash_sim_stats, flash_read_calls);

	memcpy(data, MOCK_FLASH(offset), len);
	FLASH_SIM_STATS_INCN(flash_sim_stats, bytes_read, len);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
	FLASH_SIM_STATS_INCN(flash_sim_stats, flash_read_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
#endif

	return 0;
}

static int flash_sim_write(const struct device *dev, const uint64_t offset,
			   const void *data, const uint64_t len)
{
	uint8_t buf[FLASH_SIMULATOR_PROG_UNIT];
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	if ((offset % FLASH_SIMULATOR_PROG_UNIT) ||
	    (len % FLASH_SIMULATOR_PROG_UNIT)) {
		return -EINVAL;
	}

	FLASH_SIM_STATS_INC(flash_sim_stats, flash_write_calls);

	/* check if any unit has been already programmed */
	memset(buf, FLASH_SIMULATOR_ERASE_VALUE, sizeof(buf));
	for (uint32_t i = 0; i < len; i += FLASH_SIMULATOR_PROG_UNIT) {
		if (memcmp(buf, MOCK_FLASH(offset + i), sizeof(buf))) {
			FLASH_SIM_STATS_INC(flash_sim_stats, double_writes);
#if !CONFIG_FLASH_SIMULATOR_DOUBLE_WRITES
			return -EIO;
#endif
		}
	}

#ifdef CONFIG_FLASH_SIMULATOR_STATS
	bool data_part_ignored = false;

	if (flash_sim_thresholds.max_write_calls != 0) {
		if (flash_sim_stats.flash_write_calls >
			flash_sim_thresholds.max_write_calls) {
			return 0;
		} else if (flash_sim_stats.flash_write_calls ==
				flash_sim_thresholds.max_write_calls) {
			if (flash_sim_thresholds.max_len == 0) {
				return 0;
			}

			data_part_ignored = true;
		}
	}
#endif

	for (uint32_t i = 0; i < len; i++) {
#ifdef CONFIG_FLASH_SIMULATOR_STATS
		if (data_part_ignored) {
			if (i >= flash_sim_thresholds.max_len) {
				return 0;
			}
		}
#endif /* CONFIG_FLASH_SIMULATOR_STATS */

		/* only pull bits to zero */
#if FLASH_SIMULATOR_ERASE_VALUE == 0xFF
		*(MOCK_FLASH(offset + i)) &= *((uint8_t *)data + i);
#else
		*(MOCK_FLASH(offset + i)) = *((uint8_t *)data + i);
#endif
	}

	FLASH_SIM_STATS_INCN(flash_sim_stats, bytes_written, len);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	/* wait before returning */
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
	FLASH_SIM_STATS_INCN(flash_sim_stats, flash_write_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
#endif

	return 0;
}

static void unit_erase(const uint32_t unit)
{
	const off_t unit_addr = FLASH_SIMULATOR_BASE_OFFSET +
				(unit * FLASH_SIMULATOR_ERASE_UNIT);

	/* erase the memory unit by setting it to erase value */
	memset(MOCK_FLASH(unit_addr), FLASH_SIMULATOR_ERASE_VALUE,
	       FLASH_SIMULATOR_ERASE_UNIT);
}

static int flash_sim_erase(const struct device *dev, const uint64_t offset,
			   const uint64_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	/* erase operation must be aligned to the erase unit boundary */
	if ((offset % FLASH_SIMULATOR_ERASE_UNIT) ||
	    (len % FLASH_SIMULATOR_ERASE_UNIT)) {
		return -EINVAL;
	}

	FLASH_SIM_STATS_INC(flash_sim_stats, flash_erase_calls);

#ifdef CONFIG_FLASH_SIMULATOR_STATS
	if ((flash_sim_thresholds.max_erase_calls != 0) &&
	    (flash_sim_stats.flash_erase_calls >=
		flash_sim_thresholds.max_erase_calls)){
		return 0;
	}
#endif
	/* the first unit to be erased */
	uint32_t unit_start = (offset - FLASH_SIMULATOR_BASE_OFFSET) /
			   FLASH_SIMULATOR_ERASE_UNIT;

	/* erase as many units as necessary and increase their erase counter */
	for (uint32_t i = 0; i < len / FLASH_SIMULATOR_ERASE_UNIT; i++) {
		ERASE_CYCLES_INC(unit_start + i);
		unit_erase(unit_start + i);
	}

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	/* wait before returning */
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
	FLASH_SIM_STATS_INCN(flash_sim_stats, flash_erase_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
#endif

	return 0;
}

#ifdef CONFIG_FLASH_PAGE_LAYOUT
static const struct flash_pages_layout flash_sim_pages_layout = {
	.pages_count = FLASH_SIMULATOR_PAGE_COUNT,
	.pages_size = FLASH_SIMULATOR_ERASE_UNIT,
};

static void flash_sim_page_layout(const struct device *dev,
				  const struct flash_pages_layout **layout,
				  size_t *layout_size)
{
	*layout = &flash_sim_pages_layout;
	*layout_size = 1;
}
#endif

static const struct flash_parameters *
flash_sim_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &flash_sim_parameters;
}

static const struct flash_driver_api flash_sim_api = {
	.read = flash_sim_read,
	.write = flash_sim_write,
	.erase = flash_sim_erase,
	.get_parameters = flash_sim_get_parameters,
#ifdef CONFIG_FLASH_PAGE_LAYOUT
	.page_layout = flash_sim_page_layout,
#endif
};

#ifdef CONFIG_ARCH_POSIX

static int flash_mock_init(const struct device *dev)
{
	struct stat f_stat;
	int rc;

	if (flash_file_path == NULL) {
		flash_file_path = default_flash_file_path;
	}

	flash_fd = open(flash_file_path, O_RDWR | O_CREAT, (mode_t)0600);
	if (flash_fd == -1) {
		posix_print_warning("Failed to open flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	rc = fstat(flash_fd, &f_stat);
	if (rc) {
		posix_print_warning("Failed to get status of flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	if (ftruncate(flash_fd, FLASH_SIMULATOR_FLASH_SIZE) == -1) {
		posix_print_warning("Failed to resize flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	mock_flash = mmap(NULL, FLASH_SIMULATOR_FLASH_SIZE,
			  PROT_WRITE | PROT_READ, MAP_SHARED, flash_fd, 0);
	if (mock_flash == MAP_FAILED) {
		posix_print_warning("Failed to mmap flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	if (f_stat.st_size == 0) {
		/* erase the memory unit by pulling all bits to one */
		(void)memset(mock_flash, FLASH_SIMULATOR_ERASE_VALUE,
			     FLASH_SIMULATOR_FLASH_SIZE);
	}

	return 0;
}

#else
static int flash_mock_init(const struct device *dev)
{  
	unsigned int val = 0;
	soc_pstore_get(SOC_PSTORE_TAG_SYS_PANIC,&val);

    printk("panic=%d, flash data checksum %x\n", val, mock_flash_checksum_get());
    if(!val){
        memset(mock_flash, FLASH_SIMULATOR_ERASE_VALUE, ARRAY_SIZE(mock_flash));        
    }
    printk("flash init data checksum %x\n", mock_flash_checksum_get()); 
    
	return 0;
}

#endif /* CONFIG_ARCH_POSIX */

static int flash_init(const struct device *dev)
{
	FLASH_SIM_STATS_INIT_AND_REG(flash_sim_stats, STATS_SIZE_32, "flash_sim_stats");
	FLASH_SIM_STATS_INIT_AND_REG(flash_sim_thresholds, STATS_SIZE_32,
			   "flash_sim_thresholds");
	return flash_mock_init(dev);
}

DEVICE_DEFINE(sim_flash_acts, CONFIG_SIM_FLASH_NAME, flash_init, NULL,
		    NULL, NULL, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &flash_sim_api);            

#ifdef CONFIG_ARCH_POSIX

static void flash_native_posix_cleanup(void)
{
	if ((mock_flash != MAP_FAILED) && (mock_flash != NULL)) {
		munmap(mock_flash, FLASH_SIMULATOR_FLASH_SIZE);
	}

	if (flash_fd != -1) {
		close(flash_fd);
	}
}

static void flash_native_posix_options(void)
{
	static struct args_struct_t flash_options[] = {
		{ .manual = false,
		  .is_mandatory = false,
		  .is_switch = false,
		  .option = "flash",
		  .name = "path",
		  .type = 's',
		  .dest = (void *)&flash_file_path,
		  .call_when_found = NULL,
		  .descript = "Path to binary file to be used as flash" },
		ARG_TABLE_ENDMARKER
	};

	native_add_command_line_opts(flash_options);
}


NATIVE_TASK(flash_native_posix_options, PRE_BOOT_1, 1);
NATIVE_TASK(flash_native_posix_cleanup, ON_EXIT, 1);

#endif /* CONFIG_ARCH_POSIX */

/* Extension to generic flash driver API */
void *z_impl_flash_simulator_get_memory(const struct device *dev,
					size_t *mock_size)
{
	ARG_UNUSED(dev);

	*mock_size = FLASH_SIMULATOR_FLASH_SIZE;
	return mock_flash;
}

#ifdef CONFIG_USERSPACE

#include <syscall_handler.h>

void *z_vrfy_flash_simulator_get_memory(const struct device *dev,
				      size_t *mock_size)
{
	Z_OOPS(Z_SYSCALL_SPECIFIC_DRIVER(dev, K_OBJ_DRIVER_FLASH, &flash_sim_api));

	return z_impl_flash_simulator_get_memory(dev, mock_size);
}

#include <syscalls/flash_simulator_get_memory_mrsh.c>

#endif /* CONFIG_USERSPACE */
