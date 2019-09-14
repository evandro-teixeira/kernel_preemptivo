/*
 * kernel.h
 *
 *  Created on: 30/08/2019
 *      Author: evandro
 */

#ifndef KERNEL_KERNEL_H_
#define KERNEL_KERNEL_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "stm32f0xx_hal.h"
#include "stm32f0xx.h"

#define KERNEL_CONFIG_MAX_TASKS		8
#define KERNEL_CONFIG_DEBUG 		0

#define KERNEL_ID_IDLE				0

#define KERNEL_DISABLE_INTERRUPTS()	__asm volatile 	( " cpsid i " )
#define KERNEL_ENABLE_INTERRUPTS()	__asm volatile 	( " cpsie i " )

#ifndef KERNEL_NULL
#define KERNEL_NULL ((void *)0)
#endif
/*
#ifndef uint32_t
typedef unsigned long int uint32_t;
#endif

#ifndef uint8_t
typedef unsigned long int uint8_t;
#endif
*/
#ifndef ptrTask
typedef void (*ptrTask)(void *p_params) ;
#endif
/*
#ifndef false
#define false (char)0;
#endif

#ifndef true
#define true (char)1;
#endif
*/
typedef enum
{
	Running = 1,
	Waiting,
}TaskStatus;

typedef enum
{
	Ready = 0,
	Blocked,
	Paused,
}TaskState;

typedef enum
{
	Idle = 0,
	Low,
	Medium,
	High,
}TaskPriority;

typedef struct
{
	volatile uint32_t sp;
	void (*handler)(void *p_params);
	void *p_params;
	volatile TaskStatus status;
	TaskPriority priority;
	uint32_t ticks;
	TaskState state;
}TaskStr;

typedef enum
{
	KERNEL_STATE_DEFAULT = 1,
	KERNEL_STATE_INITIALIZED,
	KERNEL_STATE_TASKS_INITIALIZED,
	KERNEL_STATE_STARTED,
}KernelState;

typedef struct
{
	TaskStr tasks[KERNEL_CONFIG_MAX_TASKS];
	volatile uint32_t current_task;
	uint32_t size; // numero de task
}KernelStr;

bool Kernel_Init(void);
//bool Kernel_Add_Task(void (*handler)(void *p_params), void *p_task_params,uint32_t size);
bool Kernel_Add_Task(void (*handler)(void *p_params), void *p_task_params,uint32_t size,TaskPriority priority);
bool Kernel_Start(uint32_t systick_ticks);
void Kernel_PendSV_Callback(void);
void Kernel_Systick_Callback(void);
void Kernel_SVC_Callback(void);
void Kernel_Release(void);
void kernel_add_task_idle(ptrTask task);
void Kernel_Delay(uint32_t tick);

#endif /* KERNEL_KERNEL_H_ */
