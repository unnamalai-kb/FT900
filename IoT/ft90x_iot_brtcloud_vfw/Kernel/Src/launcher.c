/**
  @file
  @brief
  App Launcher - launch apps from UART commands


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


#include <ft900.h>
#include <launcher.h>
#include "string.h"
#include "pff.h"
#include "tinyprintf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "kernel.h"
#include "mem_access.h"
#include "assert.h"

#define RX_BUFF_SIZE 256u

/* To optimise various options (load, kill, suspend, resume, etc) to work with user apps */
#define LAUNCHER_OPTIM_LOAD_COMMANDS 0

// A circular buffer
static uint8_t RxBuffer[RX_BUFF_SIZE];
static volatile uint16_t BuffHead;
static uint16_t BuffTail;
#define Z_NUMBYTES()		((BuffHead - BuffTail) & (RX_BUFF_SIZE - 1))
#define Z_ISBUFFULL()		(((BuffHead + 1 ) & (RX_BUFF_SIZE-1)) == ((BuffTail & (RX_BUFF_SIZE-1))) ? (true): (false))
#define Z_ISBUFEMPTY()		((BuffHead) == (BuffTail) ? (true): (false))


#if _USE_LFN
char Lfname[_MAX_LFN + 1];
#endif

static uint8_t Line[128];
FILINFO Finfo;
DIR Dir; /* Directory object */
FIL fapp;
FATFS fs;

volatile TaskHandle_t prevAppHandle = NULL;
volatile TaskHandle_t nextAppHandle = NULL;

extern TaskHandle_t KernelTasks[MaxKernelTasks];
extern int app_index;

typedef void (*APP_ENTRY_FUNC)(uint32_t argc, uint8_t *argv[]);

void uart0ISR();
void gpio_ISR(void);
void launcher_task(void *pvParameters);
void launcher_swap(const char * const app_name);
static void put_rc(FRESULT rc);
void get_line(char *buff, int len);

/** @brief Initialized the terminal based Application Launcher
 *
 *  The launcher has a main task that monitors the UART for commands.
 *  Applications can be loaded/killed/suspended/resumed via this interface
 *  @param None
 *  @returns None
 *
 */
void launcher_init(void) {

	/* Initialise FatFS */
	ASSERT_P(FR_OK, pf_mount(&fs), "Unable to mount File System");
#if DEMO_MODEM_APPLICATION

	/* Mode button */
    gpio_function(9,pad_gpio0);
    gpio_dir(9,pad_dir_input);
    /* Attach an interrupt */
    interrupt_attach(interrupt_gpio, (uint8_t)interrupt_gpio, gpio_ISR);
    gpio_interrupt_enable(9, gpio_int_edge_raising);
    /****************/

	char *param = "certs.app";
	// We don't enable global interrupts here coz that will be done once the scheduler is setup
	xTaskCreate(launcher_task, /* Pointer to the function that implements the task. */
	"Launcher", /* Text name for the task */
	1500, /* Stack depth  */
	(void *)param,
	configMAX_PRIORITIES - 1, /* Priority */
	&KernelTasks[AppLauncher]); /* A handle to the task */
#else
	launcher_hw_init();
	// We don't enable global interrupts here coz that will be done once the scheduler is setup
	xTaskCreate(launcher_task, /* Pointer to the function that implements the task. */
	"Launcher", /* Text name for the task */
	/*2000*/500, /* Stack depth  */
	NULL, /* We are not using the task parameter. */
	configMAX_PRIORITIES - 1, /* Priority */
	&KernelTasks[AppLauncher]); /* A handle to the task */
#endif
}

#if !DEMO_MODEM_APPLICATION
void launcher_hw_init(void){
	/* Attach the interrupt so it can be called... */
	interrupt_attach(interrupt_uart0, (uint8_t) interrupt_uart0, uart0ISR);
	/* Enable the UART to fire interrupts when receiving data... */
	uart_enable_interrupt(UART0, uart_interrupt_rx);
}
#endif

/** @brief Main task for launcher
 *
 * The job of the loader task is to monitor user input, waiting till an App is to be loaded.
 * Once a request is received, a new task is created for the App and added to the task list.
 *
 *  @param Not used
 *  @returns None
 *
 */
#if DEMO_MODEM_APPLICATION
void launcher_task(void *pvParameters)
{

	char *ptr = pvParameters;
	char *swap_module;
	int swap_seconds = 0;
	tfp_printf("Started launcher..\r\n");
	int index = kernel_get_free_index(ptr);
	app_index = index;
#if !PETIT_FF
	if (load_app_to_pm(ptr, &fapp, 1))
#else
	if (load_app_to_pm(ptr, &fs, 1))
#endif
	{
		tfp_printf("Successfully Loaded Application:");
		s_AppProperties* AppInfo = ld_get_app_info();
		tfp_printf("%08x", AppInfo->HeapStart);
		kernel_start_app((TaskFunction_t)AppInfo->EntryPoint, 5, 200/*1024*/,(const char *)AppInfo->Name, NULL);
	}
	else
	{
		tfp_printf("Application not found in SD card");
	}


	iot_app_setup();
#if 1
	for(;;)
	{
		vTaskDelay( pdMS_TO_TICKS(1000) );
		swap_seconds++;
		if (swap_seconds == 30)
		{
			//swap_seconds = 0;
			swap_module = "Demo2.app";
#if 0
			// We need to kill existing apps first
			kernel_exit_app(1);
			index = kernel_get_free_index(ptr);
			app_index = index;
#if !PETIT_FF
			if (load_app_to_pm(swap_module, &fapp, 1))
#else
			if (load_app_to_pm(swap_module, &fs, 1))
#endif
			{
				tfp_printf("Successfully Loaded Application:");
				s_AppProperties* AppInfo = ld_get_app_info();
				tfp_printf("%08x", AppInfo->HeapStart);
				kernel_start_app((TaskFunction_t)AppInfo->EntryPoint, 5, 200/*1024*/,(const char *)AppInfo->Name, NULL);
			}
			else
			{
				tfp_printf("Application not found in SD card");
			}
#else
			launcher_swap(swap_module);
#endif
		}
	}
#endif
}
#else
void launcher_task(void *pvParameters) {

	char *ptr;
	long p1, p2;
	BYTE res;
	UINT s1, s2;
	TaskHandle_t xHandle;
	TaskHandle_t suspendHandle = NULL;
	FATFS *fsp;
	uint32_t ulNotifiedValue;
	int num_tries = 0;
	int prev_app_index = 0;
	tfp_printf("Started launcher..\r\n");


	for (;;) {

		if(xTaskNotifyWait( 0x00,TASK_NOTIFICATION_LINE_RXED, &ulNotifiedValue, portMAX_DELAY ) == true){

			ptr = (char*)Line;
			s1 = Z_NUMBYTES();
			get_line(ptr, s1);

			switch (*ptr++)
			{
#if !LAUNCHER_OPTIM_LOAD_COMMANDS
			case 'd': /* d [<path>] - Directory listing */
				while (*ptr == ' ')
					ptr++;
				res = pf_opendir(&Dir, ptr);
				if (res) {
					put_rc(res);
					break;
				}
				p1 = s1 = s2 = 0;
				for (;;) {
					res = pf_readdir(&Dir, &Finfo);
					if ((res != FR_OK) || !Finfo.fname[0])
						break;
					if (Finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++;
						p1 += Finfo.fsize;
					}
					tfp_printf(("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s"),
							(Finfo.fattrib & AM_DIR) ? 'D' : '-',
							(Finfo.fattrib & AM_RDO) ? 'R' : '-',
							(Finfo.fattrib & AM_HID) ? 'H' : '-',
							(Finfo.fattrib & AM_SYS) ? 'S' : '-',
							(Finfo.fattrib & AM_ARC) ? 'A' : '-',
							(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15,
							Finfo.fdate & 31, (Finfo.ftime >> 11),
							(Finfo.ftime >> 5) & 63, Finfo.fsize,
							&(Finfo.fname[0]));
#if _USE_LFN
					for (p2 = strlen(Finfo.fname); p2 < 14; p2++)
						tfp_printf(" ");
					tfp_printf(" %s\n", Lfname);
#else
					tfp_printf("\n");
#endif
				}
				if (res == FR_OK)
				{
					tfp_printf(("%4u File(s),%10lu bytes total\n%4u Dir(s)"), s1,
							p1, s2);
#if !PETIT_FF
					if (f_getfree(ptr, (DWORD*) &p1, &fsp) == FR_OK)
						tfp_printf((", %10luK bytes free\n"),
								p1 * fsp->csize / 2);
#endif
				}
				if (res)
					put_rc(res);
				break;
#endif
			case 'l': /* l [<path>] - load module */
				while (*ptr == ' ') // escape any spaces
					ptr++;
				// We need to kill existing apps first
				xHandle = kernel_get_app_handle();
				if (xHandle != NULL)
				{
					kernel_exit_app(1);
				}
				int index = kernel_get_free_index(ptr);
				if (index == MAX_USER_APPS)
				{
					tfp_printf("Maximum apps reached, cannot load the new app %s!\r\n", ptr);
					break; //break from the 'l' case
				}
				app_index = index;
#if !PETIT_FF
				if (load_app_to_pm(ptr, &fapp, 1))
#else
				if (load_app_to_pm(ptr, &fs, 1))
#endif
				{
					tfp_printf("Successfully Loaded Application:");
					s_AppProperties* AppInfo = ld_get_app_info();
					tfp_printf("%08x", AppInfo->HeapStart);
					kernel_start_app((TaskFunction_t)AppInfo->EntryPoint, 5, 200/*1024*/,(const char *)AppInfo->Name, NULL);
				}
				else
				{
					tfp_printf("Application not found in SD card");
				}
				break;
#if !LAUNCHER_OPTIM_LOAD_COMMANDS
			case 'k': /* k - Kill current module */
				xHandle = kernel_get_app_handle();
				if (xHandle != NULL)
				{
					tfp_printf("Successfully killed current app %x!\r\n", xHandle);
					kernel_exit_app(1);
					num_tries = 0;
					while(num_tries < MAX_USER_APPS)
					{
						app_index++;
						if (app_index >= MAX_USER_APPS)
						{
							app_index = 0;
						}
						nextAppHandle = kernel_get_app_handle();
						if (nextAppHandle != NULL)
						{
							//resuming the suspended app
							if (kernel_app_suspend_status(nextAppHandle) == 1)
							{
								char *appl = kernel_get_app_name();
								if (load_app_to_pm(appl, &fapp, 0))
								{
									tfp_printf("Successfully restored app %s!\r\n", appl);
								}
							}
							//vTaskResume(xHandle);
							kernel_resume_app(nextAppHandle);
							tfp_printf("aindex: %d, resumed!\r\n",app_index);
							break;
						}
						num_tries++;
					}
				}
				break;

			case 's': /* s - Suspend current module */
				xHandle = kernel_get_app_handle();
				if (xHandle != NULL)
				{
					tfp_printf("suspend!\r\n");
					//vTaskSuspend(xHandle);
					kernel_suspend_app(xHandle);
				}
				break;

			case 'r': /* r - Resume current module */
				xHandle = kernel_get_app_handle();
				if (xHandle != NULL )
				{
					//vTaskResume(xHandle);
					kernel_resume_app(xHandle);
					tfp_printf("resume!");
				}
				break;
#endif
			case 'w': /* w - sWap current module */
				while (*ptr == ' ') // escape any spaces
					ptr++;
				launcher_swap(ptr);
				break;
			default:
				tfp_printf("unknown cmd %d\r\n", *(ptr - 1));
				break;

			}
		}
	}
}
#endif

void launcher_swap(const char * const app_name)
{
	TaskHandle_t xHandle;
	int prev_app_index = 0;

	xHandle = kernel_get_app_handle();
	if (xHandle != NULL)
	{
		//check if exceeded max number of apps
		int index = kernel_get_free_index(app_name);
		tfp_printf("index: %d",index);
		if (index == MAX_USER_APPS)
		{
			tfp_printf("Maximum apps reached, cannot load the new app %s!\r\n", app_name);
			return;
		}

		//suspend the currently running app..
		kernel_suspend_app(xHandle);
		prevAppHandle = xHandle;
		prev_app_index = app_index;
		//load the new app
		app_index = index;
		//app_index = (app_index)?0:1;
		nextAppHandle = kernel_get_app_handle();
		if (nextAppHandle == NULL)
		{
			//loading the given app to use the PM memory
#if !PETIT_FF
			if (load_app_to_pm(app_name, &fapp, 1))
#else
			if (load_app_to_pm(app_name, &fs, 1))
#endif
			{
				s_AppProperties* AppInfo = ld_get_app_info();
				tfp_printf("Successfully Loaded another app:%08x\r\n",AppInfo->HeapStart);
				tfp_printf("aindex: %d, started!\r\n",app_index);
				kernel_start_app((TaskFunction_t)AppInfo->EntryPoint, 5, 500/*1024*/,(const char *)AppInfo->Name, NULL);
			}
			else
			{
				tfp_printf("Error loading the app! \r\n");
				//if error loading the new app, restore the suspended app
				app_index = prev_app_index;
				char *appl = kernel_get_app_name();
				if (load_app_to_pm(appl, &fapp, 0))
				{
					tfp_printf("Successfully restored prev app %s!\r\n", appl);
				}
				kernel_resume_app(prevAppHandle);
			}
		}
		else
		{
			//resuming the suspended app
			if (kernel_app_suspend_status(nextAppHandle) == 1)
			{
				if (load_app_to_pm(app_name, &fapp, 0))
				{
					tfp_printf("Successfully Loaded previous app %s!\r\n", app_name);
				}
			}
			//vTaskResume(xHandle);
			kernel_resume_app(nextAppHandle);
			tfp_printf("aindex: %d, resumed!\r\n",app_index);
		}
	}
}

#if !DEMO_MODEM_APPLICATION
/** @brief ISR to receive characters over UART and push them into a circular buffer
 *
 *  FreeRTOS Task Notifications are used to notify the background task when a carriage return is detected
 *
 *  @param None
 *  @param None
 *  @returns None
 *
 */
void uart0ISR() {
	uint8_t c;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if (uart_is_interrupted(UART0, uart_interrupt_rx)) {
		/* Read a byte from the UART... */

		/* This call will block the CPU until a character is received */
		if (!Z_ISBUFFULL()) {
			uart_read(UART0, &c);
			RxBuffer[BuffHead++ & (RX_BUFF_SIZE - 1)] = c;

			if (c == '\r') // Set a notification event if we have received a CR
			{
		        xTaskNotifyFromISR( KernelTasks[AppLauncher],TASK_NOTIFICATION_LINE_RXED,eSetBits,&xHigherPriorityTaskWoken );
			}

		} else {
			tfp_printf("ERROR! Buffer Overflow");
		}

	}

    if(xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }

}

/** @brief Extract a line from the characters received over UART0
 *
 *
 *  @param buff Pointer to buffer where received characters are copied into
 *  @param len Length of characters received in the line
 *  @returns None
 *
 */
void get_line(char *buff, int len) {
	uint8_t c;
	int i = 0;

	for (; i < len;) {
		c = RxBuffer[BuffTail++ & (RX_BUFF_SIZE - 1)];

		if (c == '\r')
			break;
		if ((c == '\b') && i) {
			i--;
			uart_write(UART0, c);
			continue;
		}
		if (c >= ' ') { /* Visible chars */
			buff[i++] = c;

			uart_write(UART0, c); // Enable echo
		}
	}

	buff[i] = 0;

	uart_write(UART0, '\n');
}
#endif

/** @brief Translate FatFS return codes into human readable format
 *
 *
 *  @param rc return code of type FRESULT
 *  @returns None
 *
 */
static void put_rc(FRESULT rc) {
	static const char* str[] = { "OK", "DISK_ERR", "INT_ERR", "NOT_READY",
			"NO_FILE", "NO_PATH", "INVALID_NAME", "DENIED", "EXIST",
			"INVALID_OBJECT", "WRITE_PROTECTED", "INVALID_DRIVE", "NOT_ENABLED",
			"NO_FILE_SYSTEM", "MKFS_ABORTED", "TIMEOUT", "LOCKED",
			"NOT_ENOUGH_CORE", "TOO_MANY_OPEN_FILES" };

	tfp_printf(("rc=%u FR_"), rc);
	tfp_printf(("%s\n"), str[rc]);
}
