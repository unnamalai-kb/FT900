/**
  @file
  @brief
  Main driver task for VFW Kernel


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

#include <stdint.h>
#include <ft900.h>
#include <launcher.h>
#include "pff.h"
#include "diskio.h"
#include "tinyprintf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "timers.h"
#include "kernel.h"
/* netif Abstraction Header. */
#include "net.h"
/* lwip Headers */
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/sntp.h"
#include "iot.h"
#include "iot_utils.h"

///////////////////////////////////////////////////////////////////////////////////
/* Default network configuration. */
#define USE_DHCP 1       // 1: Dynamic IP, 0: Static IP
static ip_addr_t ip      = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
static ip_addr_t gateway = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
static ip_addr_t mask    = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
static ip_addr_t dns     = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
///////////////////////////////////////////////////////////////////////////////////

void ISR_usbd(void);
void dummy_calls(void);

extern TaskHandle_t KernelTasks[MaxKernelTasks]; //TODO: Need to be used in a cleaner way

void kernel_network(void *pvParameters)
{
    /*
     * Wait until network is ready then display network info
     *
     * */
	tfp_printf( "Waiting for configuration..." );
    int i = 0;
    while ( !net_is_ready() )
    {
    	vTaskDelay( pdMS_TO_TICKS(1000) );
	    tfp_printf( "." );
        if (i++ > 30)
        {
        	tfp_printf( "Could not recover. Do reboot.\r\n" );
        }
    }
    vTaskDelay( pdMS_TO_TICKS(1000) );
    tfp_printf( "\r\n" );
    uint8_t* mac = net_get_mac();
    tfp_printf( "MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    ip_addr_t addr = net_get_ip();
    tfp_printf( "IP=%s\r\n", inet_ntoa(addr) );
    addr = net_get_gateway();
    tfp_printf( "GW=%s\r\n", inet_ntoa(addr) );
    addr = net_get_netmask();
    tfp_printf( "MA=%s\r\n", inet_ntoa(addr) );
    vTaskDelay( pdMS_TO_TICKS(1000) );

    for (;;)
    {

    	vTaskDelay( pdMS_TO_TICKS(1000) );
    }
}

void start_networking(void)
{
	net_setup();

	/* Initialize network */
	net_init( ip, gateway, mask, USE_DHCP, dns, NULL, NULL );

	xTaskCreate(kernel_network, /* Pointer to the function that implements the task. */
	"Network", /* Text name for the task */
	200, /* Stack depth  */
	NULL, /* We are not using the task parameter. */
	configMAX_PRIORITIES - 2, /* Priority */
	&KernelTasks[Network]); /* We are not going to use the task handle. */

}

/** @brief Main driver for the VFW Kernel
 *
 *  Initializes the SDIO interface, UART (if logging enabled) and mounts the file system.
 *  Starts the Kernel Watchdog and App Launcher task and finally starts the scheduler
 *
 *  @param None.
 *  @returns None.
 *
 */
int main(void) {

	sys_reset_all();
	kernel_hw_init();
	kernel_sw_init();
	kernel_start_heartbeat();
	start_networking();
	launcher_init(); // App launcher task, uses UART to launch Apps

	/* Start the scheduler so the created tasks start executing. */
	vTaskStartScheduler();

	tfp_printf("\ERROR!");

	// dummy calls to prevent optimization (removal) of unused symbols
	dummy_calls();

	for(;;);

	return 0;
}

/** @brief A handler for watchdog interrupt
 *
 *
 *  @param None.
 *  @returns None.
 *
 */
void watchdog_handler(void) {
	tfp_printf("*** Watch Dog Expired\n");

	while (1)
		;
}


// Disable the "incompatible-pointer-types" warning for the dummy calls below
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

/** @brief A dummy function that "calls" a function. Used to prevent optimization of (selected) unused functions in the kernel
 *
 *
 *  @param A function pointer
 *  @returns None.
 *
 */
void dummy_call(void(*fp)(void))
{
	fp();
}

int kernel_iot_credentials(iot_certificates* tls_certificates, iot_credentials* mqtt_credentials);
iot_handle get_iot_handle(void);
// Some dummy calls to ensure that the API functions are linked in always
void dummy_calls(void)
{
	//dummy_call(uxTaskGetStackHighWaterMark);
	dummy_call(kernel_suspend_app);
	dummy_call(kernel_resume_app);
	dummy_call(kernel_register_suspend_resume_hooks);
	//dummy_call(vTaskSetTaskNumber);
	dummy_call(kernel_sw_init);
	dummy_call(kernel_exit_app);
	dummy_call(kernel_start_app);
	dummy_call(kernel_interrupt_attach);
	dummy_call(kernel_interrupt_detach);
	dummy_call(kernel_get_app_handle);
	//dummy_call(kernel_delay);
	dummy_call(kernel_create_task);
//	dummy_do(vTaskPrioritySet);
	dummy_call(xTaskGenericNotifyFromISR);
//	dummy_do(vTaskList);
	dummy_call(xTaskGenericNotify);
//	dummy_do(vTaskSwitchContext);
//	dummy_do(uxTaskGetSystemState);
//	dummy_do(vTaskGetRunTimeStats);
//	dummy_do(xTaskResumeAll);
	dummy_call(xTaskNotifyWait);
	dummy_call(vTaskNotifyGiveFromISR);
	//dummy_call(vTaskDelayUntil);
	dummy_call(ulTaskNotifyTake);
	//dummy_call(xTaskResumeFromISR);
	//dummy_call(vTaskSuspend);
//	dummy_do(eTaskGetState);
	//dummy_call(vTaskResume);
	dummy_call(vTaskDelete);
//	dummy_do(vTaskStartScheduler);
	dummy_call(vTaskDelay);
	/////dummy_call(vTaskExitCritical);
	/////dummy_call(vTaskEnterCritical);
	//dummy_call(uxTaskPriorityGetFromISR);
	//dummy_call(uxTaskPriorityGet);
	/////dummy_call(uxTaskGetTaskNumber);
	/////dummy_call(pcTaskGetTaskName);
	dummy_call(xTaskGetTickCountFromISR);
	dummy_call(xTaskGetTickCount);
//	dummy_do(vTaskEndScheduler);
//	dummy_do(vTaskSuspendAll);
//	dummy_do(xTaskGetIdleTaskHandle);
	dummy_call(uxTaskGetNumberOfTasks);
	/////dummy_call(xQueueGenericReceive);
	dummy_call(xQueueGenericSend);
	dummy_call(xQueueGenericCreate);
	dummy_call(xQueueGenericSendFromISR);
	dummy_call(xQueueCreateMutex);
	dummy_call(xQueueReceiveFromISR);
	dummy_call(xQueueGiveFromISR);
	dummy_call(xQueuePeekFromISR);
	dummy_call(xQueueIsQueueFullFromISR);
	dummy_call(uxQueueSpacesAvailable);
	dummy_call(xQueueIsQueueEmptyFromISR);
	dummy_call(uxQueueMessagesWaiting);
	dummy_call(vQueueDelete);
	dummy_call(uxQueueMessagesWaitingFromISR);
	//dummy_call(ucQueueGetQueueType);
	//dummy_call(uxQueueGetQueueNumber);
	dummy_call(xEventGroupWaitBits);
	dummy_call(xEventGroupSetBits);
	dummy_call(xEventGroupSync);
	dummy_call(vEventGroupDelete);
	dummy_call(xEventGroupClearBits);
	dummy_call(xEventGroupCreate);
	//dummy_call(uxEventGroupGetNumber);
	//dummy_call(xEventGroupSetBitsFromISR);
	//dummy_call(xEventGroupGetBitsFromISR);
	//dummy_call(xEventGroupClearBitsFromISR);
	dummy_call(vEventGroupSetBitsCallback);
	dummy_call(vEventGroupClearBitsCallback);
	dummy_call(xTimerGenericCommand);
	dummy_call(xTimerCreate);
	//dummy_call(xTimerPendFunctionCallFromISR);
	//dummy_call(xTimerPendFunctionCall);
	dummy_call(xTimerCreateTimerTask);
	dummy_call(xTimerIsTimerActive);
	dummy_call(vTimerSetTimerID);
	dummy_call(pvTimerGetTimerID);
	/////dummy_call(pcTimerGetTimerName);
	dummy_call(xTimerGetTimerDaemonTaskHandle);
	dummy_call(vListInsert);
	dummy_call(uxListRemove);
	dummy_call(vListInsertEnd);
	dummy_call(vListInitialise);
	dummy_call(vListInitialiseItem);
	//dummy_call(pvTaskGetThreadLocalStoragePointer);
	//dummy_call(vTaskSetThreadLocalStoragePointer);
	dummy_call(pvPortMalloc);
	////dummy_call(kernel_USBD_attach);
	dummy_call(uPortIsInISR);
	dummy_call(xQueueSemaphoreTake);
	dummy_call(tfp_snprintf);
	/*********************************/
	/***** LWIP and MQTT related *****/
	dummy_call(lwip_gethostbyname);
	dummy_call(net_get_ip);
	dummy_call(ip4addr_ntoa);
	dummy_call(net_get_netmask);
	dummy_call(net_get_gateway);
	dummy_call(net_get_mac);
	dummy_call(net_is_ready);
	dummy_call(altcp_tls_free_config);
	dummy_call(mqtt_client_is_connected);
	dummy_call(mqtt_client_connect);
	dummy_call(mqtt_disconnect);
	dummy_call(mqtt_sub_unsub);
	dummy_call(mqtt_set_inpub_callback);
	dummy_call(mqtt_publish);
	/*********************************/
	dummy_call(altcp_tls_create_config_client_2wayauth);
	dummy_call(altcp_tls_create_config_client);
	/*********************************/
	dummy_call(kernel_iot_credentials);
	dummy_call(get_iot_handle);
	dummy_call(iot_subscribe);
	dummy_call(iot_publish);
	dummy_call(iot_is_connected);
	dummy_call(iot_utils_getdeviceid);
}

#pragma GCC diagnostic pop
