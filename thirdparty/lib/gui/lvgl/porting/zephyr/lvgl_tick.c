#include "lvgl_tick.h"
#include <zephyr.h>

static uint32_t sys_cycle;
static uint32_t sys_time_ms;

uint32_t lvgl_tick_get()
{
	uint32_t cycle = k_cycle_get_32();
	uint32_t delta_ms = k_cyc_to_ms_floor32(cycle - sys_cycle);

	sys_cycle += delta_ms * (sys_clock_hw_cycles_per_sec() / 1000);
	sys_time_ms += delta_ms;

	return sys_time_ms;
}
