/**
  @file
  @brief
  VFW Kernel


*/
/*
 * ============================================================================
 * History
 * =======
 * 29 Dec 2015 : Created
 *
 * Copyright (C) Bridgetek Pte Ltd
 * ============================================================================
 *
 * This source code ("the Software") is provided by Bridgetek Pte Ltd 
 * ("Bridgetek") subject to the licence terms set out
 * http://brtchip.com/BRTSourceCodeLicenseAgreement/ ("the Licence Terms").
 * You must read the Licence Terms before downloading or using the Software.
 * By installing or using the Software you agree to the Licence Terms. If you
 * do not agree to the Licence Terms then do not download or use the Software.
 *
 * Without prejudice to the Licence Terms, here is a summary of some of the key
 * terms of the Licence Terms (and in the event of any conflict between this
 * summary and the Licence Terms then the text of the Licence Terms will
 * prevail).
 *
 * The Software is provided "as is".
 * There are no warranties (or similar) in relation to the quality of the
 * Software. You use it at your own risk.
 * The Software should not be used in, or for, any medical device, system or
 * appliance. There are exclusions of Bridgetek liability for certain types of loss
 * such as: special loss or damage; incidental loss or damage; indirect or
 * consequential loss or damage; loss of income; loss of business; loss of
 * profits; loss of revenue; loss of contracts; business interruption; loss of
 * the use of money or anticipated savings; loss of information; loss of
 * opportunity; loss of goodwill or reputation; and/or loss of, damage to or
 * corruption of data.
 * There is a monetary cap on Bridgetek's liability.
 * The Software may have subsequently been amended by another user and then
 * distributed by that other user ("Adapted Software").  If so that user may
 * have additional licence terms that apply to those amendments. However, Bridgetek
 * has no liability in relation to those amendments.
 * ============================================================================
 */

#include "stdint.h"
#include <ft900.h>
#include <launcher.h>
#include "assert.h"
#include "tinyprintf.h"
#include "mem_access.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "kernel.h"
#include "tinyprintf.h"
#include "pff.h"
#include <string.h>


#define GPIO_SD_CLK  (19)
#define GPIO_SD_CMD  (20)
#define GPIO_SD_DAT3 (21)
#define GPIO_SD_DAT2 (22)
#define GPIO_SD_DAT1 (23)
#define GPIO_SD_DAT0 (24)
#define GPIO_SD_CD   (25)
#define GPIO_SD_WP   (26)

#define NO_INTERRUPT_ATTACHED 0xFFFFFFFF
#define USER_TASK_BASE_ID		100u // All user created tasks will have a number greater than 100
#define MAX_USER_TASKS			10u
#define MAX_APP_NAME_LEN		configMAX_TASK_NAME_LEN

/* GLOBAL VARIABLES ****************************************************************/

// only one app runs at a time
/*static */volatile int app_index = 0;
typedef struct stAppMgmt {
	TaskHandle_t app_handle;
	char app_name[MAX_APP_NAME_LEN];
	int suspended;
}stAppMgmt;

stAppMgmt applets[MAX_USER_APPS] = {0};

typedef struct interrupt_attached_s
{
	interrupt_t interrupt_no; //unused
	interrupt_callback_t isr;
	int app_index;
}interrupt_attached_s;
static interrupt_attached_s AppAttachedInterrupts[interrupt_max] = {0}; // this should be an array..


void rtc_ISR(void);
void uart1_ISR(void);
void powermanagement_ISR(void);
void i2s_ISR(void);
void gpio_ISR(void);
#if DEMO_MODEM_APPLICATION
isrptr_t const sys_interrupt_handlers[interrupt_max] = {
		NULL, /*powermanagement_ISR,*/  /*!< Power Management */
	    NULL,  /*!< USB Host Interrupt */
		NULL,  /*!< USB Device Interrupt */
	    NULL,  /*!< Ethernet Interrupt */
	    NULL,  /*!< SD Card Interrupt */
	    NULL,  /*!< CAN0 Interrupt */
	    NULL,  /*!< CAN1 Interrupt */
	    NULL,  /*!< Camera Interrupt */
	    NULL,  /*!< SPI Master Interrupt */
	    NULL,  /*!< SPI Slave 0 Interrupt */
	    NULL,  /*!< SPI Slave 1 Interrupt */
	    NULL,  /*!< I2C Master Interrupt */
	    NULL,  /*!< I2C Slave Interrupt */
		NULL,/*uart0ISR,*/  /*!< UART0 Interrupt */
	    NULL,  /*!< UART1 Interrupt */
	    NULL,  /*!< I2S Interrupt */
	    NULL,  /*!< PWM Interrupt */
	    NULL,  /*!< Timers Interrupt */
	    gpio_ISR,  /*!< GPIO Interrupt */
		NULL,  /*!< RTC Interrupt */
	    NULL,  /*!< ADC Interrupt */
	    NULL,  /*!< DAC Interrupt */
	    NULL,  /*!< Slow Clock Timer Interrupt */
};
#else
isrptr_t const sys_interrupt_handlers[interrupt_max] = {0};
#endif

TaskHandle_t KernelTasks[MaxKernelTasks];
TaskHandle_t UserTasks[MAX_USER_APPS][MAX_USER_TASKS];

static UBaseType_t UserTaskCntr[MAX_USER_APPS]; // Keep track of last allocated user task ID. Must be cleared when a new app is loaded
// A flag used to allow kernel tasks to force allocation on user heap. Mainly used to allocate the stack+tcb of first user task,
// as this is done from the kernel context. All subsequent user allocations are done from the user context and can be handled
// by checking for the task ID
static bool ForceUseUserHeap;

user_hooks user_suspend_callback[MAX_USER_APPS] = {NULL};
user_hooks user_resume_callback[MAX_USER_APPS] = {NULL};

/* FUNCTION PROTOTYPES *************************************************************/

void tfp_putc(void* p, char c);
void kernel_switch_to_user_heap(bool val);

/** @brief Reset kernel internal variables / state
 *
 *
 *  @param None
 *  @returns None
 *
 */
void kernel_sw_init(void) {
	K_printf("Kernel Initialized!\r\n");
	app_index = 0;
	memset(&applets,0,sizeof(stAppMgmt)*MAX_USER_APPS);

	for (int i=0; i<interrupt_max; i++)
	{
		AppAttachedInterrupts[i].interrupt_no = NO_INTERRUPT_ATTACHED;
		AppAttachedInterrupts[i].app_index = MAX_USER_APPS;
	}
	extern void xPortResetUserHeap(void);
	xPortResetUserHeap();
}


/** @brief To be called when we want to suspending all the tasks running in the current App
 *
 *  suspends all the tasks in the loaded App
 *  @param None
 *  @returns None
 *
 */
void kernel_suspend_app(TaskHandle_t app)
{
	if (app != NULL)
	{
		//taskENTER_CRITICAL();
	    kernel_switch_to_user_heap(true);
#if 0
		for (int i=0; i<interrupt_max; i++)
		{
			if ((AppAttachedInterrupts[i].app_index == app_index) && (i == interrupt_usb_device))
			{
				CRITICAL_SECTION_BEGIN
				{
					USBD_REG(epie) = 0;
					USBD_REG(cmie) = 0;
				}
				CRITICAL_SECTION_END;
			}
		}
#endif
#if 1
		for (int i=0; i<interrupt_max; i++)
		{
			if ((AppAttachedInterrupts[i].app_index == app_index) && (i == interrupt_i2s))
			{
				CRITICAL_SECTION_BEGIN
				{
				    /* disable I2S ISRs... */
				    i2s_disable_int(MASK_I2S_IE_FIFO_TX_EMPTY | MASK_I2S_IE_FIFO_TX_HALF_FULL);
				    i2s_clear_int_flag(0xFFFF);
				}
				CRITICAL_SECTION_END;
			}
		}
#endif
	    //UBaseType_t uxHighWaterMark;
        /* Inspect our own high water mark on entering the task. */
	    //uxHighWaterMark = uxTaskGetStackHighWaterMark( app );
	    //tfp_printf("bef suspend:%d\n", uxHighWaterMark);
		// suspend all tasks allocated by the user
		if (user_suspend_callback[app_index] != NULL)
		{
			user_suspend_callback[app_index]();
		}
		for(int i=0; i < UserTaskCntr[app_index]; i++){
			K_printf("suspending %08lX\n", (uint32_t)UserTasks[app_index][i]);
			vTaskSuspend(UserTasks[app_index][i]);
		}
        /* Inspect our own high water mark on entering the task. */
	    //uxHighWaterMark = uxTaskGetStackHighWaterMark( app );
	    //tfp_printf("aft suspend:%d\n", uxHighWaterMark);
		kernel_switch_to_user_heap(false);
		//taskEXIT_CRITICAL();
		applets[app_index].suspended = 1;
	}
}

#define AUD_SAMPLE_FILL_16 8	//16 bit subframe: 8 bytes is 2 samples
/** @brief To be called when we want to suspending all the tasks running in the current App
 *
 *  suspends all the tasks in the loaded App
 *  @param None
 *  @returns None
 *
 */
void kernel_resume_app(TaskHandle_t app)
{
	if (app != NULL)
	{
	    //UBaseType_t uxHighWaterMark;
        /* Inspect our own high water mark on entering the task. */
	    //uxHighWaterMark = uxTaskGetStackHighWaterMark( app );
	    //tfp_printf("bef res:%d\n", uxHighWaterMark);
		//taskENTER_CRITICAL();
		// resume all tasks allocated by the user
		for(int i=0; i < UserTaskCntr[app_index]; i++){
			K_printf("resuming %08lX\n", (uint32_t)UserTasks[app_index][i]);
			vTaskResume(UserTasks[app_index][i]);
		}
		if (user_resume_callback[app_index] != NULL)
		{
			user_resume_callback[app_index]();
		}
        /* Inspect our own high water mark on entering the task. */
	    //uxHighWaterMark = uxTaskGetStackHighWaterMark( app );
	    //tfp_printf("aft res:%d\n", uxHighWaterMark);
#if 0
		for (int i=0; i<interrupt_max; i++)
		{
			if ((AppAttachedInterrupts[i].app_index == app_index) && (i == interrupt_usb_device))
			{
				CRITICAL_SECTION_BEGIN
				{
					USBD_REG(cmie) = MASK_USBD_CMIE_ALL;
					USBD_REG(epie) = (MASK_USBD_EPIE_EP0IE |
							((1 << (USBD_MAX_ENDPOINT_COUNT)) - 2));
				}
				CRITICAL_SECTION_END;
			}
		}
#endif
#if 1
		for (int i=0; i<interrupt_max; i++)
		{
			if ((AppAttachedInterrupts[i].app_index == app_index) && (i == interrupt_i2s))
			{
				CRITICAL_SECTION_BEGIN
				{
				    i2s_clear_int_flag(0xFFFF);
				    /* enable back I2S ISRs... */
				    i2s_enable_int(MASK_I2S_IE_FIFO_TX_EMPTY | MASK_I2S_IE_FIFO_TX_HALF_FULL);
				    static uint8_t dac_buffer[AUD_SAMPLE_FILL_16] __attribute__((aligned(4)));
				    /* Start streaming audio */
				    i2s_start_tx();
		    		/* Send a small number of samples to the DAC to fill in more
		    		 * data is received. This must be a multiple of a longword.
		    		 */
		    		i2s_write(dac_buffer, AUD_SAMPLE_FILL_16);
				}
				CRITICAL_SECTION_END;
			}
		}
#endif
		//taskEXIT_CRITICAL();
		applets[app_index].suspended = 0;
	}
}

/** @brief To be called when we want to quit the currently running App
 *
 *  Deletes the currently loaded App
 *  @param None
 *  @returns None
 *
 */
void kernel_exit_app(int code) {
	K_printf("App exited with code %d", code);
	taskENTER_CRITICAL();
    kernel_switch_to_user_heap(true);
	if (applets[app_index].app_handle != NULL)
	{
		if (user_suspend_callback[app_index] != NULL)
		{
			user_suspend_callback[app_index]();
		}

		user_suspend_callback[app_index] = NULL;
		user_resume_callback[app_index] = NULL;
		// delete all tasks allocated by the user
		vTaskDelete(applets[app_index].app_handle);
		for(int i=0; i < UserTaskCntr[app_index]; i++){
			K_printf("deleting %08lX\n", (uint32_t)UserTasks[app_index][i]);
			vTaskDelete(UserTasks[app_index][i]);
		}
		UserTaskCntr[app_index] = 0;
		applets[app_index].app_handle = NULL;

#if 0
		// There's no easy way to disable all peripheral interrupts so we just disable all preipherals we are not using
		for (sys_device_t per = sys_device_camera; per < sys_device_dac1; per++) {
			if (per != sys_device_timer_wdt)
				sys_disable(per);
		}
		// When we kill an app, we can by default detach all attached interrupts, except for what the loader uses (eg: Tick)
		for (interrupt_t isr = interrupt_usb_host; isr < interrupt_slowclock; isr++) {
			if (isr != interrupt_timers)
				interrupt_detach(isr);
		}
#endif
		/* re-init modules we use except for timer (which is handled in freeRTOS)*/
		//kernel_hw_init();
		//launcher_hw_init();
	}
#if 0
	for (int i=0; i<interrupt_max; i++)
	{
		if (AppAttachedInterrupts[i].app_index == app_index)
		{
			AppAttachedInterrupts[i].interrupt_no = NO_INTERRUPT_ATTACHED;
			AppAttachedInterrupts[i].isr = NULL;
			AppAttachedInterrupts[i].app_index = MAX_USER_APPS;
		}
	}
#endif
	kernel_switch_to_user_heap(false);
	taskEXIT_CRITICAL();
}


/** @brief To be called from Apps when they want to create new FreeRTOS Tasks
 *
 *	All VFW user tasks **MUST** be created via this interface and not the xTaskCreate() interface.
 *	Kernel tasks should never be created via this interface.
 *  This function is instrumental in keeping the Kernel and the User Heap separate. It does this by
 *  assigning a task ID > 100 for all user tasks. Then whenever memory is allocated via Heap_4, the memory manager
 *  checks the current task ID and allocates the memory either on the User or Kernel heap accordingly.
 *
 * @param pvTaskCode Pointer to the task entry function.  Tasks
 * must be implemented to never return (i.e. continuous loop).
 *
 * @param pcName A descriptive name for the task.  This is mainly used to
 * facilitate debugging.  Max length defined by configMAX_TASK_NAME_LEN - default
 * is 16.
 *
 * @param usStackDepth The size of the task stack specified as the number of
 * variables the stack can hold - not the number of bytes.  For example, if
 * the stack is 16 bits wide and usStackDepth is defined as 100, 200 bytes
 * will be allocated for stack storage.
 *
 * @param pvParameters Pointer that will be used as the parameter for the task
 * being created.
 *
 * @param uxPriority The priority at which the task should run.  Systems that
 * include MPU support can optionally create tasks in a privileged (system)
 * mode by setting bit portPRIVILEGE_BIT of the priority parameter.  For
 * example, to create a privileged task at priority 2 the uxPriority parameter
 * should be set to ( 2 | portPRIVILEGE_BIT ).
 *
 * @param pvCreatedTask Used to pass back a handle by which the created task
 * can be referenced.
 *
 * @return pdPASS if the task was successfully created and added to a ready
 * list, otherwise an error code defined in the file projdefs.h
 *
 */
uint32_t kernel_create_task(TaskFunction_t pxTaskCode, const char * const pcName, const uint16_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask )
{

	TaskHandle_t UserTask; // We need a handle to write the task number

	if(UserTaskCntr[app_index] == MAX_USER_TASKS)
	{
		K_printe("User tasks exceeded limit");
		return pdFREERTOS_ERRNO_EACCES;
	}

	K_printf("Creating task #%lu\n", UserTaskCntr[app_index]);
    //kernel_switch_to_user_heap(true);
	uint32_t ret = xTaskCreate((TaskFunction_t )pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, &UserTask);
	// set the id of the task to be in the user range
	vTaskSetTaskNumber(UserTask, UserTaskCntr[app_index] + USER_TASK_BASE_ID + (app_index*USER_TASK_BASE_ID));
	// return the handle to the user if he wants it
	if(pxCreatedTask != NULL)
		*pxCreatedTask = UserTask;

	UserTasks[app_index][UserTaskCntr[app_index]] = UserTask;
	K_printf("Created User Task - ID:%lu, Handle:%08lX, ret:%lu\n", UserTaskCntr[app_index] + USER_TASK_BASE_ID, (uint32_t)UserTasks[UserTaskCntr[app_index]], ret);
	UserTaskCntr[app_index]++;
    //kernel_switch_to_user_heap(false);

	return (ret);
}

void kernel_delete_task(TaskFunction_t pxTaskCode){
// remove any internal references that the kernel keeps - none as of now
	vTaskDelete( pxTaskCode );
}


/** @brief To be called when creating the main task for the user App.
 *
 *  It's assumed that this is called from within the kernel context
 *
 * @param func Pointer to the task entry function.  Tasks
 * must be implemented to never return (i.e. continuous loop).
 *
 * @param app_name A descriptive name for the task.  This is mainly used to
 * facilitate debugging.  Max length defined by configMAX_TASK_NAME_LEN - default
 * is 16.
 *
 * @param stack_size The size of the task stack specified as the number of
 * variables the stack can hold - not the number of bytes.  For example, if
 * the stack is 16 bits wide and usStackDepth is defined as 100, 200 bytes
 * will be allocated for stack storage.
 *
 * @param param Pointer that will be used as the parameter for the task
 * being created.
 *
 * @param priority The priority at which the task should run.
 *
 * @return TRUE if the task was successfully created, FALSE on errors.
 *
 */
// Used to create the main task of a lodable app
bool kernel_start_app(TaskFunction_t func, uint32_t priority, uint32_t stack_size, const char* app_name, void * param) {
	bool ret = false;
	tfp_printf("Trying to create App..");
	if (priority >= configMAX_PRIORITIES)
		priority = configMAX_PRIORITIES - 2;

	/*find free app handle
	for (int i = 0; i < MAX_USER_APPS; i++)
	{
		if (AppHandle[i] == NULL)
		{
			app_index = i;
			break;
		}
	}*/

	if (applets[app_index].app_handle == NULL)
	{
		taskENTER_CRITICAL();

	    kernel_switch_to_user_heap(true);
	    //tfp_printf("free user heap: %08X \r\n", xPortGetFreeHeapSize());
		if(xTaskCreate(func, app_name, stack_size, param, priority,&applets[app_index].app_handle))
		{
			kernel_switch_to_user_heap(false);
			TaskHandle_t app_handle = applets[app_index].app_handle;
			tfp_printf("Created App %s, Handle: %08X\n", app_name,(unsigned int) app_handle);
			// set the id of the task to be in the user range
			vTaskSetTaskNumber(app_handle, UserTaskCntr[app_index] + USER_TASK_BASE_ID + (app_index*USER_TASK_BASE_ID) );
			UserTasks[app_index][UserTaskCntr[app_index]] = app_handle;
			UserTaskCntr[app_index]++;
			ret = true;
		}
		else
		{
			K_printf("ERROR: Couldn't create App\n");
		}
		taskEXIT_CRITICAL();

	} else {
		K_printf("App exists with handle: %08X\n", (unsigned int) applets[app_index].app_handle);
	}

	return ret;
}

/** @brief interface for Apps to get their main Task handle
 *
 *  @param None
 *  @returns None
 *
 */
TaskHandle_t kernel_get_app_handle(void) {
	return (applets[app_index].app_handle);
}

char* kernel_get_app_name(void) {
	return (applets[app_index].app_name);
}

int kernel_get_free_index(const char * const app_name)
{
	int index = 0;
	char buffer[MAX_APP_NAME_LEN];

	for(int x = 0; x < MAX_APP_NAME_LEN; x++ )
	{
		buffer[ x ] = app_name[ x ];

		/* Don't copy all MAX_APP_NAME_LEN if the string is shorter than
		configMAX_TASK_NAME_LEN characters just in case the memory after the
		string is not accessible (extremely unlikely). */
		if( app_name[ x ] == ( char ) 0x00 )
		{
			break;
		}
	}

	/* Ensure the name string is terminated in the case that the string length
	was greater or equal to MAX_APP_NAME_LEN. */
	buffer[ MAX_APP_NAME_LEN - 1 ] = '\0';

	for (;index < MAX_USER_APPS; index++)
	{
		int app_exists = (strncmp(applets[index].app_name,buffer,MAX_APP_NAME_LEN) == 0)?1:0;
		if ((!app_exists) && (applets[index].app_handle == NULL))
		{
			/* Store the task name in the TCB. */
			if( app_name != NULL )
			{
				/* Start by copying the entire string. */
				strncpy(applets[index].app_name, buffer,MAX_APP_NAME_LEN);
			}
			break;
		}
		else
		{
			if (app_exists)
			{
				break; //return with the index number
			}

		}
	}
	return index;
}
int kernel_app_suspend_status(TaskHandle_t app_handle)
{
	int status = -1;
	for (int index = 0;index < MAX_USER_APPS; index++)
	{
		if (applets[index].app_handle == app_handle)
		{
			status = applets[index].suspended;
			break;
		}
	}
	return status;
}

int kernel_app_put_to_suspend(TaskHandle_t app_handle)
{
	int status = -1;
	for (int index = 0;index < MAX_USER_APPS; index++)
	{
		if (applets[index].app_handle == app_handle)
		{
			applets[index].suspended = 1;
			break;
		}
	}
	return status;
}

void kernel_register_suspend_resume_hooks(user_hooks suspend_hook,user_hooks resume_hook)
{
	user_suspend_callback[app_index] = suspend_hook;
	user_resume_callback[app_index] = resume_hook;
}

uint8_t kernel_interrupt_detach(interrupt_t interrupt)
{
	K_printf("detaching ISR %d..\n",interrupt);
	AppAttachedInterrupts[interrupt].isr = NULL;
	if (interrupt != interrupt_timers)
			interrupt_detach(interrupt);
	return (0);
}

/** @brief Interface for applications to attach interrupts.
 *  All user interrupts **MUST** be registered via this interface.
 *  This is mainly required to allow the kernel to manage interrupts shared between user and kernel space.
 *
 *  @param interrupt The interrupt vector to attach to
 *  @param priority The priority to give the interrupt
 *  @param func The function to call when interrupted
 *  @returns 0 on a success or -1 for a failure
 *
 */
uint8_t kernel_interrupt_attach(interrupt_t interrupt, uint8_t priority, interrupt_callback_t func) {
	K_printf("Registering ISR %d..\n",interrupt);
	if ((interrupt == interrupt_timers) || (interrupt == interrupt_usb_device) || (interrupt == interrupt_ethernet) || (interrupt == interrupt_uart0)) {
		AppAttachedInterrupts[interrupt].isr = func;
		AppAttachedInterrupts[interrupt].app_index = app_index;
		if ((interrupt == interrupt_usb_device) || (interrupt == interrupt_ethernet) || (interrupt == interrupt_uart0))
			interrupt_attach(interrupt, priority,func);
		return (0);
	} else {
		if (sys_interrupt_handlers[interrupt] != NULL)
		{
			AppAttachedInterrupts[interrupt].isr = func;
			AppAttachedInterrupts[interrupt].app_index = app_index;
			return (interrupt_attach(interrupt, priority, sys_interrupt_handlers[interrupt]));
		}
		else
			return -1;
	}
}

/** @brief Internal callback to handle timer interrupts used by user space
 *
 *  @param None
 *  @returns None
 *
 */
void vApplicationTickHook(void)
{
	int param = 0;
	interrupt_callback_t timerISR = AppAttachedInterrupts[interrupt_timers].isr;
	if ((timerISR != NULL) && (AppAttachedInterrupts[interrupt_timers].app_index == app_index))
	{
		timerISR(&param);
	}
}

// Kernel Utilities


/** @brief Call before allocating memory for user applications
 *
 * This must be called from a critical section, else all new tasks may have their heaps allocated in the user section
 * To be called from the loader before creating the first loadable task
 * Task switching must be suspended before setting this flag, and the flag must be cleared before resume.
 *
 *  @param None
 *  @returns None
 *
 */
void kernel_switch_to_user_heap(bool val)
{
	tfp_printf("switch:%d",val);
	ForceUseUserHeap = val;
}


/** @brief A wrapper to let the memory manager know if the request is from the user or kernel context
 *
 * Check the task ID to know if the currently executing task is a user task or a kernel task
 *
 *  @param None
 *  @returns TRUE if the current task is a user task, FALSE otherwise
 *
 */
bool kernel_is_user_request(void) {
// Get number of current executing task.
	UBaseType_t id = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
	if (id >= USER_TASK_BASE_ID) {
		//K_printf("User req:%lu\n", id);
		return true;
	} else {
		//K_printf("Kernel req:%lu\n", id);
		return false;
	}
}


/** @brief A callback to be executed from the memory manager (vPortMalloc()) before allocating memory
 *	If it's a user app request, will extract the start-of-user-heap address from the AppInfo, and switch to the user heap.
 *  If it's a kernel request, it will switch to kernel heap
 *
 *  @param None
 *  @returns None
 *
 */
void kernel_do_on_malloc_entry(void){
	extern bool xPortSwitchHeap(e_HeapTypes heap, uint32_t heapStartAdr);

	if(ForceUseUserHeap || (kernel_is_user_request() == true))
	{
		s_AppProperties* pApp = ld_get_app_info();
		xPortSwitchHeap(User_Heap,pApp->HeapStart & FT32_RAM_ADDRESS_MASK);
	}
	else
	{
		xPortSwitchHeap(Kernel_Heap,(uint32_t)NULL);
	}
}


/** @brief A callback to be executed from the memory manager (vPortFree()) before free'ing memory.
 *	If it's a user app request, will extract the start-of-user-heap address from the AppInfo, and switch to the user heap.
 *  If it's a kernel request, it will switch to kernel heap
 *
 *  @param None
 *  @returns None
 *
 */
void kernel_do_on_free_entry(uint32_t address)
{
	extern bool xPortSwitchHeap(e_HeapTypes heap, uint32_t heapStartAdr);

	if(ForceUseUserHeap || (kernel_is_user_request() == true))
	{
		s_AppProperties* pApp = ld_get_app_info();
		if((pApp->HeapStart & FT32_RAM_ADDRESS_MASK) <= address)
		{

			xPortSwitchHeap(User_Heap,pApp->HeapStart & FT32_RAM_ADDRESS_MASK);
		}
		else
		{
			xPortSwitchHeap(Kernel_Heap,(uint32_t)NULL);
		}
	}
	else
	{
#if 1
		uint32_t kernelHeapEndAddr = (uint32_t)xPortKernelHeapMaxAddr();
		if (address <= kernelHeapEndAddr)
		{
			xPortSwitchHeap(Kernel_Heap,(uint32_t)NULL);
		}
		else
		{
			s_AppProperties* pApp = ld_get_app_info();
			if((pApp->HeapStart & FT32_RAM_ADDRESS_MASK) <= address)
			{
				tfp_printf("Error");
				xPortSwitchHeap(User_Heap,pApp->HeapStart & FT32_RAM_ADDRESS_MASK);
			}
		}
#else
#if 0
		xPortSwitchHeap(Kernel_Heap,(uint32_t)NULL);
#else
		uint32_t kernelHeapEndAddr = (uint32_t)xPortKernelHeapMaxAddr();
		if (address <= kernelHeapEndAddr)
		{
			xPortSwitchHeap(Kernel_Heap,(uint32_t)NULL);
		}
		else
		{
			tfp_printf("Error");
		}
#endif
#endif
	}
}


/** @brief Start the heartbeat task
 *
 *  @param None
 *  @returns None
 *
 */
void kernel_start_heartbeat(void)
{
	xTaskCreate(kernel_heartbeat, /* Pointer to the function that implements the task. */
	"Heartbeat", /* Text name for the task */
	200, /* Stack depth  */
	NULL, /* We are not using the task parameter. */
	1, /* Priority */
	&KernelTasks[KernelHeartbeat]); /* We are not going to use the task handle. */
}

/** @brief A simple heartbeat task for the kernel that delays for 5s and prints out it's name
 *
 *  @param Not used
 *  @returns None
 *
 */
void kernel_heartbeat(void *pvParameters)
{
    char *pcTaskName = "*Heartbeat*\r\n";

	for(;;)
	{
		/* Print out the name of this task. */
		K_printf(pcTaskName);

		vTaskDelay( pdMS_TO_TICKS(1000) );
	}
}

/** @brief Hardware init for the kernel - Uart for logging, sdio for reading the SD Card
 *
 *  @param None
 *  @returns None
 *
 */
void kernel_hw_init(void){
	/* Enable the UART Device... */
	sys_enable(sys_device_uart0);
	/* Make GPIO48 function as UART0_TXD and GPIO49 function as UART0_RXD... */
	gpio_function(48, pad_uart0_txd); /* UART0 TXD */
	gpio_function(49, pad_uart0_rxd); /* UART0 RXD */
	uart_open(UART0, 1,
	UART_DIVIDER_115200_BAUD, uart_data_bits_8, uart_parity_none,
			uart_stop_bits_1);

	/* Enable tfp_printf() functionality... */
	init_printf(UART0, tfp_putc);
	//kernel_dump_core();
	/* Start up the SD Card */
	sys_enable(sys_device_sd_card);

	gpio_function(GPIO_SD_CLK, pad_sd_clk);
	gpio_pull(GPIO_SD_CLK, pad_pull_none);
	gpio_function(GPIO_SD_CMD, pad_sd_cmd);
	gpio_pull(GPIO_SD_CMD, pad_pull_pullup);
	gpio_function(GPIO_SD_DAT3, pad_sd_data3);
	gpio_pull(GPIO_SD_DAT3, pad_pull_pullup);
	gpio_function(GPIO_SD_DAT2, pad_sd_data2);
	gpio_pull(GPIO_SD_DAT2, pad_pull_pullup);
	gpio_function(GPIO_SD_DAT1, pad_sd_data1);
	gpio_pull(GPIO_SD_DAT1, pad_pull_pullup);
	gpio_function(GPIO_SD_DAT0, pad_sd_data0);
	gpio_pull(GPIO_SD_DAT0, pad_pull_pullup);
	gpio_function(GPIO_SD_CD, pad_sd_cd);
	gpio_pull(GPIO_SD_CD, pad_pull_pullup);
	gpio_function(GPIO_SD_WP, pad_sd_wp);
	gpio_pull(GPIO_SD_WP, pad_pull_pullup);

	sdhost_init(); /* Need to do this before assigning IO to allow the peripheral
	 to start up properly */

	/* Check to see if a card is inserted */
	uart_puts(UART0, "Please Insert SD Card\r\n");
	while (sdhost_card_detect() != SDHOST_CARD_INSERTED)
		;
	uart_puts(UART0, "SD Card Inserted\r\n");
}



/* wrapper functions
void kernel_USBD_attach(void)
{
	USBD_attach();
}*/

/** tfp_printf putc
 *  @param p Parameters
 *  @param c The character to write */
void tfp_putc(void* p, char c) {
	uart_write((ft900_uart_regs_t*) p, (uint8_t) c);
}


//Interrupt processing functions

void gpio_ISR(void)
{
#define GPIO_INPUT  38
	uint8_t int_status = 0;
	  static bool prog_debug = true;
	  if (gpio_is_interrupted(9))
	  {
	      prog_debug ^=1;
	      if(!prog_debug){
	    	  K_printf("USB->Programmer");
	      }else{
	    	  K_printf("USB->Debugger");
	      }
	      gpio_write(8,prog_debug);
	  }
    if (gpio_is_interrupted(GPIO_INPUT))
    {
        interrupt_callback_t gpio_callback = AppAttachedInterrupts[interrupt_gpio].isr;
        if ((gpio_callback != NULL) && (AppAttachedInterrupts[interrupt_gpio].app_index == app_index))
        {
        	gpio_callback(&int_status);
        }
    }
}

#if 0
void rtc_ISR(void)
{
    struct tm tim = {0};
    struct tm read_time = {0};
    rtc_alarm_type_t type;
    uint32_t seconds = 0;

    if (sys_check_ft900_revB()) //90x series rev B
    {
    	type = rtc_legacy_interrupt_alarm;
    }
    else
    {
    	type = rtc_interrupt_alarm1;
    }

    if (rtc_is_interrupted(type))
    {
        /* Read the time... */
        rtc_read(&tim);
        read_time = tim;
        if (sys_check_ft900_revB()) //90x series rev B
        {
        	seconds = (tim.tm_sec >> 5);
            /* Set the new alarm */
            tim.tm_sec = (seconds + 1)<<5;
            rtc_set_alarm(1,  &tim, rtc_legacy_alarm);
        }
        else
        {
            /* Set the new alarm */
            tim.tm_sec = (tim.tm_sec + 1) % 60;
            rtc_set_alarm(1,  &tim, rtc_alarm_match_sec);
        }
        interrupt_callback_t rtc_callback = AppAttachedInterrupts[interrupt_rtc].isr;
        if ((rtc_callback != NULL) && (AppAttachedInterrupts[interrupt_rtc].app_index == app_index))
        {
        	rtc_callback(&read_time);
        }
#if SLEEP_MODE
        seconds_to_sleep = seconds;
#endif
    }
}

void uart1_ISR(void)
{
	uint8_t int_status = 0;

	enum UART_INTR_STATUS {
		UART_TX_EMPTY = 0b0010,
		UART_RX_TIMEOUT = 0b1100,
		UART_RX_FULL = 0b0100,
	};
    if (uart_is_interrupted(UART1, uart_interrupt_tx))
    {
    	int_status |= UART_TX_EMPTY;
    }
    if (uart_is_interrupted(UART1, uart_interrupt_rx))
    {
    	int_status |= UART_RX_FULL;
    }
    if (uart_is_interrupted(UART1, uart_interrupt_rx_time_out))
    {
    	int_status |= UART_RX_TIMEOUT;
    }
    if (int_status)
    {
        interrupt_callback_t uart_callback = AppAttachedInterrupts[interrupt_uart1].isr;
        if ((uart_callback != NULL) && (AppAttachedInterrupts[interrupt_uart1].app_index == app_index))
        {
        	uart_callback(&int_status);
        }
    }
}

void i2s_ISR(void)
{
	uint16_t interrupts = I2S->I2S_IRQ_PEND & (MASK_I2S_PEND_FIFO_TX_EMPTY | MASK_I2S_PEND_FIFO_TX_HALF_FULL);
    if (i2s_is_interrupted(MASK_I2S_PEND_FIFO_TX_EMPTY) || i2s_is_interrupted(MASK_I2S_PEND_FIFO_TX_HALF_FULL))
    {
        interrupt_callback_t i2s_callback = AppAttachedInterrupts[interrupt_i2s].isr;
        if ((i2s_callback != NULL) && (AppAttachedInterrupts[interrupt_i2s].app_index == app_index))
        {
        	i2s_callback(&interrupts);
        }
    }
}


void USBD_pipe_isr(uint16_t pipe_bitfields)
{
    interrupt_callback_t usb_callback = AppAttachedInterrupts[interrupt_usb_device].isr;
    if ((usb_callback != NULL) && (AppAttachedInterrupts[interrupt_usb_device].app_index == app_index))
    {
    	usb_callback(&pipe_bitfields);
    }
}

void powermanagement_ISR(void)
{
	if (SYS->PMCFG_H & MASK_SYS_PMCFG_DEV_CONN_DEV)
	{
		// Clear connection interrupt
		SYS->PMCFG_H = MASK_SYS_PMCFG_DEV_CONN_DEV;
		USBD_attach();
	}

	if (SYS->PMCFG_H & MASK_SYS_PMCFG_DEV_DIS_DEV)
	{
		// Clear disconnection interrupt
		SYS->PMCFG_H = MASK_SYS_PMCFG_DEV_DIS_DEV;
		USBD_detach();
	}

	if (SYS->PMCFG_H & MASK_SYS_PMCFG_HOST_RST_DEV)
	{
		// Clear Host Reset interrupt
		SYS->PMCFG_H = MASK_SYS_PMCFG_HOST_RST_DEV;
		USBD_resume();
	}

	if (SYS->PMCFG_H & MASK_SYS_PMCFG_HOST_RESUME_DEV)
	{
		// Clear Host Resume interrupt
		SYS->PMCFG_H = MASK_SYS_PMCFG_HOST_RESUME_DEV;
		if(! (SYS->MSC0CFG & MASK_SYS_MSC0CFG_DEV_RMWAKEUP))
		{
			// If we are driving K-state on Device USB port;
			// We must maintain the 1ms requirement before resuming the phy
			USBD_resume();
		}
	}
}
#endif
