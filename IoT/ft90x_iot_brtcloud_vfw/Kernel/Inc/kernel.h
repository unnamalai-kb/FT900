#ifndef KERNEL_H_
#define KERNEL_H_

#include "FreeRTOS.h"
#include "task.h"
#include "kernel-public.h"

#define KERNEL_DEBUG 0
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

void kernel_sw_init(void);
void kernel_exit_app(int code);
bool kernel_start_app(TaskFunction_t func,uint32_t priority, uint32_t stack_size, const char* app_name, void * param );
void kernel_timer(void);
//bool kernel_is_user_request(void);
void kernel_do_on_malloc_entry(void);
void kernel_heartbeat(void *pvParameters);
void kernel_do_on_free_entry(uint32_t address);
void kernel_hw_init(void);
void kernel_start_heartbeat(void);

// SRAM addresses are 16bit, although the liker generates them with a prefix of 0x8000xxxx
#define FT32_RAM_ADDRESS_MASK		0xFFFF
#define FT32_SRAM_END_ADDRESS 		(uint32_t)(0xFFFF)

typedef enum{
	Kernel_Heap,
	User_Heap,
	MAX_HEAP_TYPES
}e_HeapTypes;

typedef enum {
  KernelHeartbeat, AppLauncher, Network, LWIP, MaxKernelTasks=10
} e_KernelTasks;

#if (KERNEL_DEBUG == 1)
#define K_INFO	VFW Info:
#define K_ERROR	VFW Error:
#define K_printf(x,...)              	tfp_printf(x, ##__VA_ARGS__)
#define K_printi(x,...)              	tfp_printf(STR(LD_INFO) x, ##__VA_ARGS__)
#define K_printe(x,...)              	tfp_printf(STR(LD_ERROR) x, ##__VA_ARGS__)
#else
#define K_printf(x,...)
#define K_printi(x,...)
#define K_printe(x,...)
#endif

#endif /* KERNEL_H_ */
