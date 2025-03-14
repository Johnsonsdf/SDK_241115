/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <SEGGER_SYSVIEW.h>
#include <ksched.h>

extern const SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI;

#if CONFIG_THREAD_MAX_NAME_LEN
#define THREAD_NAME_LEN CONFIG_THREAD_MAX_NAME_LEN
#else
#define THREAD_NAME_LEN 20
#endif


static void set_thread_name(char *name, struct k_thread *thread)
{
	const char *tname = k_thread_name_get(thread);

	if (tname != NULL && tname[0] != '\0') {
		memcpy(name, tname, THREAD_NAME_LEN);
		name[THREAD_NAME_LEN - 1] = '\0';
	} else {
		snprintk(name, THREAD_NAME_LEN, "T%pE%p",
		thread, &thread->entry);
	}
}

void sys_trace_thread_info(struct k_thread *thread)
{
	char name[THREAD_NAME_LEN];

	set_thread_name(name, thread);

	SEGGER_SYSVIEW_TASKINFO Info;

	Info.TaskID = (uint32_t)(uintptr_t)thread;
	Info.sName = name;
	Info.Prio = thread->base.prio;
	Info.StackBase = thread->stack_info.size;
	Info.StackSize = thread->stack_info.start;
	SEGGER_SYSVIEW_SendTaskInfo(&Info);
}


#if defined(CONFIG_SOC_LARK)
#define INT_ID_DESC1	"I#18=T0,I#19=T1,I#23=RTC,I#34=I2C0,I#35=I2C1," \
						"I#36=DSP,I#38=UART0"
#define INT_ID_DESC2	"I#53=GPIO,I#59=LCD,I#65=DE,I#67=LRADC,I#68=PMU," \
						"I#71=DACFIFO,I#72=BT,I#78=IIC0MT,I#79=IIC1MT"
#elif defined(CONFIG_SOC_LEOPARD)
#define INT_ID_DESC1	"I#2=T0,I#3=T1,I#4=T2,I#5=T3,I#6=T4,I#7=RTC," \
						"I#18=I2C0,I#19=I2C1,I#20=DSP,I#21=DSP1,I#22=UART0"
#define INT_ID_DESC2	"I#53=GPIO,I#55=SDMA1,I#59=LCD,I#65=DE,I#73=JPEG," \
						"I#67=LRADC,I#68=PMU,I#71=DACFIFO,I#72=BT,I#75=GPU,"\
						"I#76=SPI0MT,I#77=SPI1MT,I#78=IIC0MT,I#79=IIC1MT"
#else
#define INT_ID_DESC1	""
#define INT_ID_DESC2	""
#endif

static void cbSendSystemDesc(void)
{
	SEGGER_SYSVIEW_SendSysDesc("N=ZephyrSysView");
	SEGGER_SYSVIEW_SendSysDesc("D=" CONFIG_BOARD " "
				   CONFIG_SOC_SERIES " " CONFIG_ARCH);
	SEGGER_SYSVIEW_SendSysDesc("O=Zephyr");
	SEGGER_SYSVIEW_SendSysDesc(INT_ID_DESC1);
	SEGGER_SYSVIEW_SendSysDesc(INT_ID_DESC2);
}

static void send_task_list_cb(void)
{
	struct k_thread *thread;

	for (thread = _kernel.threads; thread; thread = thread->next_thread) {
		char name[THREAD_NAME_LEN];

		if (z_is_idle_thread_object(thread)) {
			continue;
		}

		set_thread_name(name, thread);

		SEGGER_SYSVIEW_SendTaskInfo(&(SEGGER_SYSVIEW_TASKINFO) {
			.TaskID = (uint32_t)(uintptr_t)thread,
			.sName = name,
			.StackSize = thread->stack_info.size,
			.StackBase = thread->stack_info.start,
			.Prio = thread->base.prio,
		});
	}
}


static U64 get_time_cb(void)
{
	return (U64)k_cycle_get_32();
}


const SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI = {
	get_time_cb,
	send_task_list_cb,
};



void SEGGER_SYSVIEW_Conf(void)
{
	SEGGER_SYSVIEW_Init(sys_clock_hw_cycles_per_sec(),
			    sys_clock_hw_cycles_per_sec(),
			    &SYSVIEW_X_OS_TraceAPI, cbSendSystemDesc);
#if DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_sram), okay)
	SEGGER_SYSVIEW_SetRAMBase(DT_REG_ADDR(DT_CHOSEN(zephyr_sram)));
#else
	/* Setting RAMBase is just an optimization: this value is subtracted
	 * from all pointers in order to save bandwidth.  It's not an error
	 * if a platform does not set this value.
	 */
#endif
}
