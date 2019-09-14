/*
 * kernel.c
 *
 *  Created on: 30/08/2019
 *      Author: evandro
 */

#include "kernel.h"

/**
 *
 *
 */
static KernelStr m_task_table;
static KernelState m_state = KERNEL_STATE_DEFAULT;
volatile TaskStr *kernel_curr_task;
volatile TaskStr *kernel_next_task;
ptrTask idleTask = KERNEL_NULL;
static uint32_t kernel_tick;
uint32_t Kernel_IncVar(uint32_t var, uint32_t lim);

/**
 *
 *
 */
void Kernel_Context_Switch(void);
void Kernel_Task_Idle(void *parameters);
uint32_t Kernel_Ticks_Get(void);
volatile TaskStr *Kernel_scheduler(void);
/**
 *
 *
 */
bool Kernel_Init(void)
{
	if (m_state != KERNEL_STATE_DEFAULT)
		return false;

	memset(&m_task_table, 0, sizeof(m_task_table));
	m_state = KERNEL_STATE_INITIALIZED;

	Kernel_Add_Task(&Kernel_Task_Idle,NULL,(uint32_t)(128*2),Idle);

	return true;
}

/**
 *
 *
 */
bool Kernel_Add_Task(void (*handler)(void *p_params), void *p_task_params,uint32_t size,TaskPriority priority)
{
	if (m_state != KERNEL_STATE_INITIALIZED && m_state != KERNEL_STATE_TASKS_INITIALIZED)
		return false;

	if (m_task_table.size >= KERNEL_CONFIG_MAX_TASKS-1)
		return false;

	uint32_t *stack_size;

	stack_size 					= malloc(size * sizeof(uint32_t));
	uint32_t stack_offset 		= (size * sizeof(uint32_t));
	TaskStr *p_task 			= &m_task_table.tasks[m_task_table.size];
	p_task->handler 			= handler;
	p_task->p_params 			= p_task_params;
	p_task->sp 					= (uint32_t)(stack_size+stack_offset-16);
	p_task->status 				= Waiting;
	p_task->priority            = priority;
	p_task->ticks 				= 0;
	p_task->state				= Ready;
	stack_size[stack_offset-1] 	= 0x1000000;
	stack_size[stack_offset-2] 	= (uint32_t)handler;
	stack_size[stack_offset-8] 	= (uint32_t)p_task_params;

#if KERNEL_CONFIG_DEBUG
	uint32_t base = (m_task_table.size+1)*1000;
	p_stack[stack_offset-4] = base+12;  /* R12 */
	p_stack[stack_offset-5] = base+3;   /* R3  */
	p_stack[stack_offset-6] = base+2;   /* R2  */
	p_stack[stack_offset-7] = base+1;   /* R1  */
	/* p_stack[stack_offset-8] is R0 */
	p_stack[stack_offset-9] = base+7;   /* R7  */
	p_stack[stack_offset-10] = base+6;  /* R6  */
	p_stack[stack_offset-11] = base+5;  /* R5  */
	p_stack[stack_offset-12] = base+4;  /* R4  */
	p_stack[stack_offset-13] = base+11; /* R11 */
	p_stack[stack_offset-14] = base+10; /* R10 */
	p_stack[stack_offset-15] = base+9;  /* R9  */
	p_stack[stack_offset-16] = base+8;  /* R8  */
#endif /* KERNEL_CONFIG_DEBUG */

	m_state = KERNEL_STATE_TASKS_INITIALIZED;
	m_task_table.size++;

	return true;
}

/**
 *
 *
 */
bool Kernel_Start(uint32_t systick_ticks)
{
	if (m_state != KERNEL_STATE_TASKS_INITIALIZED)
	{
		return false;
	}

	NVIC_SetPriority(PendSV_IRQn, 0xff);
	NVIC_SetPriority(SysTick_IRQn, 0x00);

	//kernel_curr_task = &m_task_table.tasks[m_task_table.current_task];

	kernel_curr_task = Kernel_scheduler();
	m_state = KERNEL_STATE_STARTED;

	__set_PSP(kernel_curr_task->sp+64);
	__set_CONTROL(0x03);
	__ISB();

	kernel_curr_task->handler(kernel_curr_task->p_params);

	return true;
}

/**
 *
 *
 */
void Kernel_Context_Switch(void)
{
	kernel_curr_task = &m_task_table.tasks[m_task_table.current_task];
//	kernel_curr_task->status = Waiting;

//	// Select next task:
//	m_task_table.current_task++;
//	if (m_task_table.current_task >= m_task_table.size)
//	{
//		m_task_table.current_task = 0;
//	}
//	kernel_next_task = &m_task_table.tasks[m_task_table.current_task];

	kernel_next_task = Kernel_scheduler();
//	kernel_next_task->status = Waiting;

	// Trigger PendSV which performs the actual context switch:
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

/**
 *
 *
 */
void Kernel_Release(void)
{
	/* call SVC  */
	__asm volatile("SVC 0\n\t");
}

/**
 *
 */
uint32_t Kernel_IncVar(uint32_t var, uint32_t lim)
{
	if(var < lim)
	{
		var++;
	}
	else
	{
		var = 1;
	}
	return var;
}

/**
 *
 *
 */
volatile TaskStr *Kernel_scheduler(void)
{
	static uint8_t index_high = 0;
	static uint8_t index_Medium = 0;
	static uint8_t index_Low = 0;
	volatile TaskStr *task;
	uint32_t index = 0;

	// Busca tarefa de Alta prioridade pronta para ser executada
	for(index=1; index < m_task_table.size; index++)
	{
		index_high = (uint8_t)Kernel_IncVar(index_high, m_task_table.size);
		if((m_task_table.tasks[index_high].state == Ready) &&
		   (m_task_table.tasks[index_high].priority == High) &&
		   (m_task_table.tasks[index_high].status == Waiting ))
		{
			m_task_table.tasks[index_high].status = Running;
			index = index_high;
			break;
		}
		//index_high++;
	}
	if(index >= (m_task_table.size))
	{
		// Busca tarefa de Media prioridade pronta para ser executada
		for(index=1; index < m_task_table.size; index++)
		{
			index_Medium = (uint8_t)Kernel_IncVar(index_Medium, m_task_table.size);
			if((m_task_table.tasks[index_Medium].state == Ready) &&
			   (m_task_table.tasks[index_Medium].priority == Medium) &&
			   (m_task_table.tasks[index_Medium].status == Waiting ))
			{
				m_task_table.tasks[index_Medium].status = Running;
				index = index_Medium;
				break;
			}
		}
		if(index >= (m_task_table.size))
		{
			// Busca tarefa de Baixa prioridade pronta para ser executada
			for(index=1; index < m_task_table.size; index++)
			{
				index_Low = (uint8_t)Kernel_IncVar(index_Low, m_task_table.size);
				if((m_task_table.tasks[index_Low].state == Ready) &&
				   (m_task_table.tasks[index_Low].priority == Low) &&
				   (m_task_table.tasks[index_Low].status == Waiting ))
				{
					m_task_table.tasks[index_Low].status = Running;
					index = index_Low;
					break;
				}
			}
			if(index >= (m_task_table.size))
			{
				index = KERNEL_ID_IDLE;
			}
			else
			{
				m_task_table.tasks[index].status = Waiting;
			}
		}
		else
		{
			m_task_table.tasks[index].status = Waiting;
		}
	}
	else
	{
		m_task_table.tasks[index].status = Waiting;
	}

	//m_task_table.tasks[m_task_table.current_task].status = Waiting;
	m_task_table.current_task = index;
	task =&m_task_table.tasks[m_task_table.current_task];
	return task;
}


/**
 *
 *
 */
void kernel_add_task_ilde(ptrTask task)
{
	if(task != KERNEL_NULL)
	{
		idleTask = task;
	}
}

/**
 *
 *
 */
void Kernel_Task_Idle(void *parameters)
{
	while(1)
	{
		if(idleTask != KERNEL_NULL)
		{
			idleTask(KERNEL_NULL);
		}
		Kernel_Release();
	}
}

/**
 *
 *
 */
void Kernel_Delay(uint32_t tick)
{
	m_task_table.tasks[m_task_table.current_task].ticks = tick;
	m_task_table.tasks[m_task_table.current_task].state = Paused;
	Kernel_Release();
}

/**
 *
 *
 */
void Kernel_Systick_Callback(void)
{
	uint32_t index;
	kernel_tick++;

	for(index=1; index < m_task_table.size; index++)
	{
		if(m_task_table.tasks[index].ticks > 0)
		{
			m_task_table.tasks[index].ticks--;
		}
		else
		{
			m_task_table.tasks[index].state = Ready;
		}
	}
}


/**
 *
 *
 */
uint32_t Kernel_Ticks_Get(void)
{
	return kernel_tick;
}

/**
 *
 *
 */
void Kernel_PendSV_Callback(void)
{
	__asm volatile(
	"DSB 							\n"
	"ISB 							\n"
	"CPSID I						\n"
	"MRS R2, PSP					\n"
	"SUB R2, #16					\n"
	"STMIA R2!, {R4-R7}				\n"
	"MOV R7, R11					\n"
	"MOV R6, R10					\n"
	"MOV R5, R9						\n"
	"MOV R4, R8						\n"
	"SUB R2, #32					\n"
	"STMIA R2!, {R4-R7}				\n"
	"SUB R2, #16					\n"
	"LDR R0, =[kernel_curr_task]	\n"
	"LDR R1, [R0]					\n"
	"STR R2, [R1]					\n"
	"LDR R1, =[kernel_next_task]	\n"
	"LDR R1, [R1]					\n"
	"LDR R2, [R1]					\n"
	"LDMIA R2!, {R4-R7}				\n"
	"MOV R11, R7					\n"
	"MOV R10, R6					\n"
	"MOV R9, R5						\n"
	"MOV R8, R4						\n"
	"LDMIA R2!, {R4-R7}				\n"
	"MSR PSP, R2					\n"
	"STR R1, [R0]					\n"
	"CPSIE I						\n"
	"BX LR							\n"
	);
}

/**
 *
 *
 */
void Kernel_SVC_Callback(void)
{
	Kernel_Context_Switch();
}
