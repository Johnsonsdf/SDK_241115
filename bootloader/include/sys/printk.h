/* printk.h - low-level debug output */

/*
 * Copyright (c) 2010-2012, 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_SYS_PRINTK_H_
#define ZEPHYR_INCLUDE_SYS_PRINTK_H_

#include <toolchain.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>
#if defined(CONFIG_LOG_PRINTK) && defined(CONFIG_LOG2)
#include <logging/log.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief Print kernel debugging message.
 *
 * This routine prints a kernel debugging message to the system console.
 * Output is send immediately, without any mutual exclusion or buffering.
 *
 * A basic set of conversion specifier characters are supported:
 *   - signed decimal: \%d, \%i
 *   - unsigned decimal: \%u
 *   - unsigned hexadecimal: \%x (\%X is treated as \%x)
 *   - pointer: \%p
 *   - string: \%s
 *   - character: \%c
 *   - percent: \%\%
 *
 * Field width (with or without leading zeroes) is supported.
 * Length attributes h, hh, l, ll and z are supported. However, integral
 * values with %lld and %lli are only printed if they fit in a long
 * otherwise 'ERR' is printed. Full 64-bit values may be printed with %llx.
 * Flags and precision attributes are not supported.
 *
 * @param fmt Format string.
 * @param ... Optional list of format arguments.
 *
 * @return N/A
 */
#ifdef CONFIG_PRINTK

#if defined(CONFIG_LOG_PRINTK) && defined(CONFIG_LOG2)
#define printk(...) Z_LOG_PRINTK(__VA_ARGS__)
static inline __printf_like(1, 0) void vprintk(const char *fmt, va_list ap)
{
	z_log_msg2_runtime_vcreate(CONFIG_LOG_DOMAIN_ID, NULL,
				   LOG_LEVEL_INTERNAL_RAW_STRING, NULL, 0,
				   fmt, ap);
}
#else
extern __printf_like(1, 2) void printk(const char *fmt, ...);
extern __printf_like(1, 0) void vprintk(const char *fmt, va_list ap);
#endif /* defined(CONFIG_LOG_PRINTK) && defined(CONFIG_LOG) */

#else
static inline __printf_like(1, 2) void printk(const char *fmt, ...)
{
	ARG_UNUSED(fmt);
}

static inline __printf_like(1, 0) void vprintk(const char *fmt, va_list ap)
{
	ARG_UNUSED(fmt);
	ARG_UNUSED(ap);
}
#endif

extern __printf_like(3, 4) int snprintk(char *str, size_t size,
					const char *fmt, ...);
extern __printf_like(3, 0) int vsnprintk(char *str, size_t size,
					  const char *fmt, va_list ap);



#ifdef CONFIG_ACTIONS_PRINTK_DMA
void trace_set_panic(void);
void printk_dma_switch(int sw_dma);
int uart_dma_send_buf(const uint8_t *buf, int len);
#endif


#ifdef __cplusplus
}
#endif

#endif
