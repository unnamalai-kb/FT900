/*
 * ============================================================================
 * History
 * =======
 * 18 Jun 2019 : Created
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
#include "tinyprintf.h"

/* FreeRTOS Headers. */
#include "FreeRTOS.h"
#include "task.h"

/* netif Abstraction Header. */
#include "net.h"

/* IOT Headers. */
#include <iot_config.h>
#include "iot.h"
#include "iot_utils.h"


#include <string.h>
#include <stdlib.h>

#define VFW_APP 1



///////////////////////////////////////////////////////////////////////////////////
//#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTF(...) do {CRITICAL_SECTION_BEGIN;tfp_printf(__VA_ARGS__);CRITICAL_SECTION_END;} while (0)
#else
#define DEBUG_PRINTF(...) tfp_printf(__VA_ARGS__);
#endif
///////////////////////////////////////////////////////////////////////////////////


#if !VFW_APP
///////////////////////////////////////////////////////////////////////////////////
/* Default network configuration. */
#define USE_DHCP 1       // 1: Dynamic IP, 0: Static IP
static ip_addr_t ip      = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
static ip_addr_t gateway = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
static ip_addr_t mask    = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
static ip_addr_t dns     = IPADDR4_INIT_BYTES( 0, 0, 0, 0 );
///////////////////////////////////////////////////////////////////////////////////
#endif


///////////////////////////////////////////////////////////////////////////////////
/* Task configurations. */
#define IOT_APP_TASK_NAME                        "iot_core"
#define IOT_APP_TASK_PRIORITY                    (3)
#if (USE_MQTT_BROKER == MQTT_BROKER_AWS_IOT) || (USE_MQTT_BROKER == MQTT_BROKER_AWS_GREENGRASS)
    #define IOT_APP_TASK_STACK_SIZE              (512)
#elif (USE_MQTT_BROKER == MQTT_BROKER_GCP_IOT)
    #define IOT_APP_TASK_STACK_SIZE              (1536 + 32)
#elif (USE_MQTT_BROKER == MQTT_BROKER_MAZ_IOT)
    #if (MAZ_AUTH_TYPE == AUTH_TYPE_SASTOKEN)
        #define IOT_APP_TASK_STACK_SIZE          (1536 + 16)
    #elif (MAZ_AUTH_TYPE == AUTH_TYPE_X509CERT)
        #define IOT_APP_TASK_STACK_SIZE          (1024 + 64)
    #endif
#else
#define IOT_APP_TASK_STACK_SIZE                  (768)
#endif
///////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////
/* IoT application function */
static void iot_core_task(void *pvParameters);
TaskHandle_t g_iot_app_handle;
iot_handle g_handle = NULL;

iot_certificates* g_tls_certificates;
iot_credentials* g_mqtt_credentials;

int kernel_iot_credentials(iot_certificates* tls_certificates, iot_credentials* mqtt_credentials)
{
	g_tls_certificates = tls_certificates;
	g_mqtt_credentials = mqtt_credentials;
}

int iot_app_setup( void )
{
	vTaskDelay( pdMS_TO_TICKS(5000) );

    if (xTaskCreate( iot_core_task,
            IOT_APP_TASK_NAME,
            200/*IOT_APP_TASK_STACK_SIZE*/,
            NULL,
            IOT_APP_TASK_PRIORITY,
            &g_iot_app_handle ) != pdTRUE ) {
        DEBUG_PRINTF( "xTaskCreate failed\r\n" );
    }
}

iot_handle get_iot_handle(void)
{
	return g_handle;
}

static void iot_core_task( void *pvParameters )
{
    (void) pvParameters;

    /* Initialize IoT library */
    iot_init();
    iot_utils_init();

    /* Initialize rtc */
    // MM900Ev1b (RevC) has an internal RTC
    // IoTBoard does not have internal RTC
    // When using IoTBoard, this must be disabled to prevent crash
    // TODO: support external RTC to work on IoTBoard
#if 0 // TEMPORARILY DISABLED FOR THE NEW FT900 IOT BOARD
    init_rtc();
#endif

    vTaskDelay( pdMS_TO_TICKS(10000) );

    while (1)
    {
        /*
         * Wait until network is ready then display network info
         *
         * */
        DEBUG_PRINTF( "Waiting for network ready..." );
        int i = 0;
        while ( !net_is_ready() ) {
            vTaskDelay( pdMS_TO_TICKS(1000) );
            DEBUG_PRINTF( "." );
            if (i++ > 30) {
                DEBUG_PRINTF( "Could not recover. Do reboot.\r\n" );
                chip_reboot();
            }
        }
        vTaskDelay( pdMS_TO_TICKS(1000) );
        DEBUG_PRINTF( "\r\n" );


        /*
         * IoT process
         *
         * */
        DEBUG_PRINTF( "Starting...\r\n\r\n" );

        /* connect to server using TLS certificates and MQTT credentials
         * sample call back functions, iot_utils_getcertificates & iot_utils_getcredentials,
         *     are provided in iot_utils.c to retrieve information from iot_config.h.
         *     These have been tested to work with Amazon AWS, Google Cloud and Microsoft Azure.
         */
        g_handle = iot_connect_ex();
        if ( !g_handle ) {
            /* make sure to replace the dummy certificates in the Certificates folder */
            DEBUG_PRINTF( "Error! Please check your certificates and credentials.\r\n\r\n" );
            vTaskDelay( pdMS_TO_TICKS(1000) );
            continue;
        }
        else
        {
        	DEBUG_PRINTF( "Device is now ready! Control this device from IoT Portal https://%s\r\n\r\n", MQTT_BROKER );
        }

        do{
            vTaskDelay( pdMS_TO_TICKS(1000) );
    	} while ( net_is_ready() && iot_is_connected( g_handle ) == 0);

    }
}
