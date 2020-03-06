/*
 * kernel-public.h
 */

#ifndef INC_KERNEL_PUBLIC_H_
#define INC_KERNEL_PUBLIC_H_

#define MAX_USER_APPS			4u

typedef void(*interrupt_callback_t)(void *param);
typedef void (*user_hooks)(void);

uint32_t kernel_create_task(TaskFunction_t pxTaskCode, const char * const pcName, const uint16_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask );
TaskHandle_t kernel_get_app_handle(void);
uint8_t kernel_interrupt_attach(interrupt_t interrupt, uint8_t priority, interrupt_callback_t func);
uint8_t kernel_interrupt_detach(interrupt_t interrupt);
void kernel_suspend_app(TaskHandle_t app);
void kernel_resume_app(TaskHandle_t app);
void kernel_register_suspend_resume_hooks(user_hooks suspend_hook,user_hooks resume_hook);


/* wrapper functions */
void kernel_USBD_attach(void);

//void tfp_printf(char *fmt, ...);

#endif /* INC_KERNEL_PUBLIC_H_ */
