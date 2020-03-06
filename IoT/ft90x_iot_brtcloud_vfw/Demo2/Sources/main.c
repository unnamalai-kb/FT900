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

/* IoT Modem */
#include "iot_modem.h"
#include "json.h"


#include <string.h>
#include <stdlib.h>

#include "iot_demos.h"
//#include "iot_status_led.h"
//#include "colornames.h"


///////////////////////////////////////////////////////////////////////////////////
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTF(...) do {CRITICAL_SECTION_BEGIN;tfp_printf(__VA_ARGS__);CRITICAL_SECTION_END;} while (0)
#else
#define DEBUG_PRINTF(...)
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
#define IOT_APP_TASK_NAME                        "iot_task"
#define IOT_APP_TASK_PRIORITY                    (5)
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
static void iot_app_task(void *pvParameters);
void ISR_gpio();

static uint16_t g_pub_time  = 5000L;


#if USE_MQTT_SUBSCRIBE
static inline char* user_generate_subscribe_topic();
static void user_subscribe_receive_cb(
    iot_subscribe_rcv* mqtt_subscribe_recv );
#endif // USE_MQTT_SUBSCRIBE

TaskHandle_t g_iot_app_handle;
iot_handle g_handle = NULL;
static uint8_t g_exit = 0;
///////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////
#if (defined DEMO1)
extern uint8_t g_tprobe_task_status;
extern uint8_t  g_1wire_task_status;
extern uint8_t g_anemometer_task_status;
#elif (defined DEMO3)
extern uint8_t adc_task_status;
#endif

uint32_t g_ulDeviceStatus = DEVICE_STATUS_RUNNING;
extern EWDemoSensors_Cfg_t ew_sensor_cfg;

//
// UART
//

#if ENABLE_UART
static UART_PROPERTIES g_oUartProperties = {
    11, // points to UART_DIVIDER_19200_BAUD
    uart_parity_none,
    uart_flow_none,
    uart_stop_bits_1,
    uart_data_bits_8
};
static uint8_t g_ucUartEnabled = 1;
#endif // ENABLE_UART


//
// GPIO
//

#if ENABLE_GPIO
GPIO_PROPERTIES g_oGpioProperties[GPIO_COUNT] = {
    { pad_dir_input, GPIO_MODES_INPUT_HIGH_LEVEL, ALERT_TYPE_ONCE, 0, GPIO_POLARITY_NEGATIVE, 0, 0, 0, 0 },
    { pad_dir_input, GPIO_MODES_INPUT_HIGH_LEVEL, ALERT_TYPE_ONCE, 0, GPIO_POLARITY_NEGATIVE, 0, 0, 0, 0 },
    { pad_dir_input, GPIO_MODES_INPUT_HIGH_LEVEL, ALERT_TYPE_ONCE, 0, GPIO_POLARITY_NEGATIVE, 0, 0, 0, 0 },
    { pad_dir_input, GPIO_MODES_INPUT_HIGH_LEVEL, ALERT_TYPE_ONCE, 0, GPIO_POLARITY_NEGATIVE, 0, 0, 0, 0 }
};
uint8_t g_ucGpioEnabled[GPIO_COUNT] = { 0, 0, 0, 0 };
static uint8_t g_ucGpioStatus[GPIO_COUNT] = { 1, 1, 1, 1 }; // ["Low", "High"]
static uint8_t g_ucGpioVoltage = GPIO_VOLTAGE_3_3;
#endif // ENABLE_GPIO


//
// I2C
//

#if ENABLE_I2C
char* g_pI2CProperties = NULL;
uint8_t g_ucI2CPropertiesCount = 2;
#endif // ENABLE_I2C


//
// ADC
//

#if ENABLE_ADC
char* g_pADCProperties = NULL;
uint8_t g_ucADCPropertiesCount = 2;
static uint8_t g_ucADCVoltage = ADC_VOLTAGE_N10_P10;
#endif // ENABLE_ADC


//
// 1WIRE
//

#if ENABLE_ONEWIRE
char* g_p1WIREProperties = NULL;
uint8_t g_uc1WIREPropertiesCount = 1;
#endif // ENABLE_ADC


//
// TPROBE
//

#if ENABLE_TPROBE
char* g_pTPROBEProperties = NULL;
uint8_t g_ucTPROBEPropertiesCount = 1;
#endif // ENABLE_TPROBE


///////////////////////////////////////////////////////////////////////////////////


#if !VFW_APP
static void myputc( void* p, char c )
{
    uart_write( (ft900_uart_regs_t*) p, (uint8_t) c );
}

static inline void uart_setup()
{
    /* enable uart */
    sys_enable( sys_device_uart0 );
    gpio_function( 48, pad_func_3 );
    gpio_function( 49, pad_func_3 );

    iot_modem_uart_enable(&g_oUartProperties, 1, 0);

    /* Enable tfp_printf() functionality... */
    init_printf( UART0, myputc );
}

static inline void ethernet_setup()
{
    net_setup();
}
#endif

int main( void )
{
    iot_handle handle = NULL;

	handle = get_iot_handle();

	if (handle != NULL)
	{
		DEBUG_PRINTF( "\r\niothandle:%p\r\n\r\n", handle );
	}
    /* subscribe and publish from/to server */
#if USE_MQTT_SUBSCRIBE
    char* topic_sub = user_generate_subscribe_topic();

    if ( iot_subscribe( handle, topic_sub, user_subscribe_receive_cb, 1 ) == 0 ) {
        DEBUG_PRINTF( "\r\nSUB: %s\r\n\r\n", topic_sub );
    }
    g_handle = handle;

    /* set device status to running */
    g_ulDeviceStatus = DEVICE_STATUS_RUNNING;
#if ENABLE_UART_ATCOMMANDS
    g_iot_app_handle = kernel_get_app_handle();
    iot_modem_uart_enable_interrupt();
    /* display the UART commands */
    iot_modem_uart_command_help();
#endif // ENABLE_UART_ATCOMMANDS

    /* process publishing of notification messages */
    do  {
        uint32_t ulNotificationValue = 0;
        if (xTaskNotifyWait(0, TASK_NOTIFY_CLEAR_BITS, &ulNotificationValue, pdMS_TO_TICKS(1000)) == pdTRUE) {
        	//DEBUG_PRINTF( "xTaskNotifyWait %d\r\n", ulNotificationValue );

#if ENABLE_UART_ATCOMMANDS
        	/* process UART */
        	if (TASK_NOTIFY_FROM_UART(ulNotificationValue)) {
        		iot_modem_uart_command_process();
        	}
#endif // ENABLE_UART_ATCOMMANDS

#if ENABLE_GPIO
        	/* process GPIO */
        	for (int i=0; i<GPIO_COUNT; i++) {
				if (TASK_NOTIFY_FROM_GPIO(ulNotificationValue, i)) {
					if (TASK_NOTIFY_ACTIVATION(ulNotificationValue)) {
						iot_modem_gpio_process(i+1, 1);
					}
					else {
						iot_modem_gpio_process(i+1, 0);
					}
				}
        	}
#endif // ENABLE_GPIO

#if ENABLE_I2C
        	/* process I2C */
        	for (int i=0; i<4; i++) {
				if (TASK_NOTIFY_FROM_I2C(ulNotificationValue, i)) {
					// TODO
				}
        	}
#endif // ENABLE_I2C

        }

    } while ( net_is_ready() && iot_is_connected( handle ) == 0 && !g_exit );

    g_exit = 0;
    //iot_unsubscribe( handle, topic_sub );

#endif // USE_MQTT_SUBSCRIBE

    /* disconnect from server */
    //iot_disconnect( handle );


	for(;;)
	{
		vTaskDelay( pdMS_TO_TICKS(1000) );
	}

    DEBUG_PRINTF( "Should never reach here!\r\n" );
}
#if !VFW_APP

static inline void display_network_info()
{
    uint8_t* mac = net_get_mac();
    DEBUG_PRINTF( "MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    ip_addr_t addr = net_get_ip();
    DEBUG_PRINTF( "IP=%s\r\n", inet_ntoa(addr) );
    addr = net_get_gateway();
    DEBUG_PRINTF( "GW=%s\r\n", inet_ntoa(addr) );
    addr = net_get_netmask();
    DEBUG_PRINTF( "MA=%s\r\n", inet_ntoa(addr) );
    vTaskDelay( pdMS_TO_TICKS(1000) );
}
#endif
static void restart_task( void *param )
{
    vTaskDelay( pdMS_TO_TICKS(1000) );
    chip_reboot();
}



///////////////////////////////////////////////////////////////////////////////////
// IOT SUBSCRIBE
///////////////////////////////////////////////////////////////////////////////////

#if USE_MQTT_SUBSCRIBE

static inline char* user_generate_subscribe_topic()
{
#if (USE_MQTT_BROKER == MQTT_BROKER_AWS_IOT) || (USE_MQTT_BROKER == MQTT_BROKER_AWS_GREENGRASS)
    // Lets subscribe to the messages we published
    return "device/#";
#elif (USE_MQTT_BROKER == MQTT_BROKER_GCP_IOT)
    // Google Cloud does not seem to support MQTT subscribe for telemetry events, only for config
    static char topic[64] = {0};
    tfp_snprintf( topic, sizeof(topic),
        "/devices/%s/config", (char*)iot_utils_getdeviceid() );
    //tfp_snprintf(topic, sizeof(topic),
    //    "/devices/%s/events", (char*)iot_getdeviceid());
    return topic;
#elif (USE_MQTT_BROKER == MQTT_BROKER_MAZ_IOT)
    static char topic[64] = {0};
    tfp_snprintf( topic, sizeof(topic),
        "devices/%s/messages/devicebound/#", (char*)iot_utils_getdeviceid() );
    //tfp_snprintf(topic, sizeof(topic),
    //    "devices/%s/messages/events/#", (char*)iot_getdeviceid());
    return topic;
#else
    static char* topic = NULL;
    if ( !topic ) {
        int len = strlen((char*)iot_utils_getdeviceid()) + 1 + 1 + 1;
        topic = pvPortMalloc( len );
        if ( topic ) {
            memset( topic, 0, len );
            tfp_snprintf( topic, len, "%s/#", (char*)iot_utils_getdeviceid() );
        }
    }

    return topic;
#endif
}

#endif // USE_MQTT_SUBSCRIBE


static int publish_default( char* topic, int topic_size, char* payload, int payload_size, iot_subscribe_rcv* mqtt_subscribe_recv )
{
    int ret;
    tfp_snprintf( topic, topic_size, "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
    tfp_snprintf( payload, payload_size, PAYLOAD_EMPTY );
    ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
    DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    return ret;
}


#if !UNUSED_DEMO_FUNCTIONS/*(ENABLE_I2C) || (ENABLE_ADC) || (ENABLE_ONEWIRE) || (ENABLE_TPROBE)*/

static inline void get_props( DEVICE_PROPERTIES* pProp, char* payload, int payload_size )
{
	if ( pProp->m_ucClass == DEVICE_CLASS_TEMPERATURE ) {
		DEVICE_ATTRIBUTES_TEMPERATURE* pAttributes = pProp->m_pvClassAttributes;
		tfp_snprintf( payload, payload_size, PAYLOAD_API_GET_XXX_DEVICE_PROPERTIES_TEMPERATURE,
			DEVICE_PROPERTIES_MODE, (int)pAttributes->m_ucMode,
			DEVICE_PROPERTIES_THRESHOLD,
			DEVICE_PROPERTIES_THRESHOLD_VALUE, pAttributes->m_oThreshold.m_ulValue,
			DEVICE_PROPERTIES_THRESHOLD_MINIMUM, pAttributes->m_oThreshold.m_ulMinimum,
			DEVICE_PROPERTIES_THRESHOLD_MAXIMUM, pAttributes->m_oThreshold.m_ulMaximum,
			DEVICE_PROPERTIES_THRESHOLD_ACTIVATE, (int)pAttributes->m_oThreshold.m_ucActivate,
			DEVICE_PROPERTIES_ALERT,
			DEVICE_PROPERTIES_ALERT_TYPE, (int)pAttributes->m_oAlert.m_ucType,
			DEVICE_PROPERTIES_ALERT_PERIOD, pAttributes->m_oAlert.m_ulPeriod,
			DEVICE_PROPERTIES_HARDWARE,
			DEVICE_PROPERTIES_HARDWARE_DEVICENAME, pAttributes->m_oHardware.m_pcDeviceName ? pAttributes->m_oHardware.m_pcDeviceName : ""
		);
	}
	else if ( pProp->m_ucClass == DEVICE_CLASS_POTENTIOMETER ) {
		DEVICE_ATTRIBUTES_TEMPERATURE* pAttributes = pProp->m_pvClassAttributes;
		tfp_snprintf( payload, payload_size, PAYLOAD_API_GET_XXX_DEVICE_PROPERTIES_POTENTIOMETER,
			DEVICE_PROPERTIES_MODE, (int)pAttributes->m_ucMode,
			DEVICE_PROPERTIES_THRESHOLD,
			DEVICE_PROPERTIES_THRESHOLD_VALUE, pAttributes->m_oThreshold.m_ulValue,
			DEVICE_PROPERTIES_THRESHOLD_MINIMUM, pAttributes->m_oThreshold.m_ulMinimum,
			DEVICE_PROPERTIES_THRESHOLD_MAXIMUM, pAttributes->m_oThreshold.m_ulMaximum,
			DEVICE_PROPERTIES_THRESHOLD_ACTIVATE, (int)pAttributes->m_oThreshold.m_ucActivate,
			DEVICE_PROPERTIES_ALERT,
			DEVICE_PROPERTIES_ALERT_TYPE, (int)pAttributes->m_oAlert.m_ucType,
			DEVICE_PROPERTIES_ALERT_PERIOD, pAttributes->m_oAlert.m_ulPeriod,
			DEVICE_PROPERTIES_HARDWARE,
			DEVICE_PROPERTIES_HARDWARE_DEVICENAME, pAttributes->m_oHardware.m_pcDeviceName ? pAttributes->m_oHardware.m_pcDeviceName : ""
		);
	}
	else if ( pProp->m_ucClass == DEVICE_CLASS_ANENOMOMETER ) {
		DEVICE_ATTRIBUTES_TEMPERATURE* pAttributes = pProp->m_pvClassAttributes;
		tfp_snprintf( payload, payload_size, PAYLOAD_API_GET_XXX_DEVICE_PROPERTIES_ANEMOMETER,
			DEVICE_PROPERTIES_MODE, (int)pAttributes->m_ucMode,
			DEVICE_PROPERTIES_THRESHOLD,
			DEVICE_PROPERTIES_THRESHOLD_VALUE, pAttributes->m_oThreshold.m_ulValue,
			DEVICE_PROPERTIES_THRESHOLD_MINIMUM, pAttributes->m_oThreshold.m_ulMinimum,
			DEVICE_PROPERTIES_THRESHOLD_MAXIMUM, pAttributes->m_oThreshold.m_ulMaximum,
			DEVICE_PROPERTIES_THRESHOLD_ACTIVATE, (int)pAttributes->m_oThreshold.m_ucActivate,
			DEVICE_PROPERTIES_ALERT,
			DEVICE_PROPERTIES_ALERT_TYPE, (int)pAttributes->m_oAlert.m_ucType,
			DEVICE_PROPERTIES_ALERT_PERIOD, pAttributes->m_oAlert.m_ulPeriod,
			DEVICE_PROPERTIES_HARDWARE,
			DEVICE_PROPERTIES_HARDWARE_DEVICENAME, pAttributes->m_oHardware.m_pcDeviceName ? pAttributes->m_oHardware.m_pcDeviceName : ""
		);
	}
	else if ( pProp->m_ucClass == DEVICE_CLASS_SPEAKER ) {
		DEVICE_ATTRIBUTES_SPEAKER* pAttributes = pProp->m_pvClassAttributes;
		if (pAttributes->m_ucType == 0) {
			DEVICE_ATTRIBUTES_SPEAKER_MIDI* pMidi = (DEVICE_ATTRIBUTES_SPEAKER_MIDI*)pAttributes->m_pvValues;
			tfp_snprintf( payload, payload_size, PAYLOAD_API_GET_XXX_DEVICE_PROPERTIES_SPEAKER,
				DEVICE_PROPERTIES_ENDPOINT, (int)pAttributes->m_ucEndpoint,
				DEVICE_PROPERTIES_TYPE, (int)pAttributes->m_ucType,
				DEVICE_PROPERTIES_DURATION, (int)pMidi->m_ulDuration,
				DEVICE_PROPERTIES_PITCH, (int)pMidi->m_ucPitch,
				DEVICE_PROPERTIES_DELAY, (int)pMidi->m_ulDelay,
				DEVICE_PROPERTIES_HARDWARE,
				DEVICE_PROPERTIES_HARDWARE_DEVICENAME, pAttributes->m_oHardware.m_pcDeviceName ? pAttributes->m_oHardware.m_pcDeviceName : "",
				DEVICE_PROPERTIES_HARDWARE_PERIPHERAL, pAttributes->m_oHardware.m_pcPeripheral ? pAttributes->m_oHardware.m_pcPeripheral : "",
				DEVICE_PROPERTIES_HARDWARE_SENSORNAME, pAttributes->m_oHardware.m_pcSensorName ? pAttributes->m_oHardware.m_pcSensorName : "",
				DEVICE_PROPERTIES_HARDWARE_ATTRIBUTE,  pAttributes->m_oHardware.m_pcAttribute  ? pAttributes->m_oHardware.m_pcAttribute  : ""
			);
		}
	}
	else if ( pProp->m_ucClass == DEVICE_CLASS_DISPLAY ) {
		DEVICE_ATTRIBUTES_DISPLAY* pAttributes = pProp->m_pvClassAttributes;
		tfp_snprintf( payload, payload_size, PAYLOAD_API_GET_XXX_DEVICE_PROPERTIES_DISPLAY,
			DEVICE_PROPERTIES_ENDPOINT, (int)pAttributes->m_ucEndpoint,
			DEVICE_PROPERTIES_TEXT, pAttributes->m_pcText,
			DEVICE_PROPERTIES_HARDWARE,
			DEVICE_PROPERTIES_HARDWARE_DEVICENAME, pAttributes->m_oHardware.m_pcDeviceName ? pAttributes->m_oHardware.m_pcDeviceName : "",
			DEVICE_PROPERTIES_HARDWARE_PERIPHERAL, pAttributes->m_oHardware.m_pcPeripheral ? pAttributes->m_oHardware.m_pcPeripheral : "",
			DEVICE_PROPERTIES_HARDWARE_SENSORNAME, pAttributes->m_oHardware.m_pcSensorName ? pAttributes->m_oHardware.m_pcSensorName : "",
			DEVICE_PROPERTIES_HARDWARE_ATTRIBUTE,  pAttributes->m_oHardware.m_pcAttribute  ? pAttributes->m_oHardware.m_pcAttribute  : ""
		);
	}
	else if ( pProp->m_ucClass == DEVICE_CLASS_LIGHT ) {
		DEVICE_ATTRIBUTES_LIGHT* pAttributes = pProp->m_pvClassAttributes;
		tfp_snprintf( payload, payload_size, PAYLOAD_API_GET_XXX_DEVICE_PROPERTIES_LIGHT,
			DEVICE_PROPERTIES_ENDPOINT, (int)pAttributes->m_ucEndpoint,
			DEVICE_PROPERTIES_COLOR, pAttributes->m_ulColor,
			DEVICE_PROPERTIES_BRIGHTNESS, pAttributes->m_ulBrightness,
			DEVICE_PROPERTIES_TIMEOUT, pAttributes->m_ulTimeout,
			DEVICE_PROPERTIES_HARDWARE,
			DEVICE_PROPERTIES_HARDWARE_DEVICENAME, pAttributes->m_oHardware.m_pcDeviceName ? pAttributes->m_oHardware.m_pcDeviceName : "",
			DEVICE_PROPERTIES_HARDWARE_PERIPHERAL, pAttributes->m_oHardware.m_pcPeripheral ? pAttributes->m_oHardware.m_pcPeripheral : "",
			DEVICE_PROPERTIES_HARDWARE_SENSORNAME, pAttributes->m_oHardware.m_pcSensorName ? pAttributes->m_oHardware.m_pcSensorName : "",
			DEVICE_PROPERTIES_HARDWARE_ATTRIBUTE,  pAttributes->m_oHardware.m_pcAttribute  ? pAttributes->m_oHardware.m_pcAttribute  : ""
		);
	}
	else {
		tfp_snprintf( payload, payload_size, PAYLOAD_EMPTY );
	}
}


static inline int set_props( DEVICE_PROPERTIES* pProp, uint8_t ucNumber, uint8_t ucAddress, uint8_t ucClass, iot_subscribe_rcv* mqtt_subscribe_recv )
{
    int ret = 0;

    pProp->m_ucSlot = ucNumber;
    pProp->m_ucAddress = ucAddress;
    pProp->m_ucEnabled = 0;
    pProp->m_ucClass = ucClass;

    if (ucClass == DEVICE_CLASS_SPEAKER) {
        if ( !pProp->m_pvClassAttributes ) {
            pProp->m_pvClassAttributes = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_SPEAKER) );
            if ( !pProp->m_pvClassAttributes ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 1\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pProp->m_pvClassAttributes, 0, sizeof(DEVICE_ATTRIBUTES_SPEAKER) );
        }

        //
		// Set endpoint, type
        //
        DEVICE_ATTRIBUTES_SPEAKER* pAttributes = (DEVICE_ATTRIBUTES_SPEAKER*)pProp->m_pvClassAttributes;
        pAttributes->m_ucEndpoint = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ENDPOINT );
        pAttributes->m_ucType = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_TYPE );
        //DEBUG_PRINTF( "set_props endpoint=%d type=%d\r\n", pAttributes->m_ucEndpoint, pAttributes->m_ucType );
        if ( !pAttributes->m_pvValues ) {
            pAttributes->m_pvValues = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_SPEAKER_MIDI) );
            if ( !pAttributes->m_pvValues ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 2\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pAttributes->m_pvValues, 0, sizeof(DEVICE_ATTRIBUTES_SPEAKER_MIDI) );
        }

        //
		// Set duration, pitch, delay
        //
        if (pAttributes->m_ucType == 0) {
            DEVICE_ATTRIBUTES_SPEAKER_MIDI* pMidi = (DEVICE_ATTRIBUTES_SPEAKER_MIDI*)pAttributes->m_pvValues;
            pMidi->m_ulDuration = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_DURATION );
            pMidi->m_ulDelay = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_DELAY );
            pMidi->m_ucPitch = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_PITCH );
            //DEBUG_PRINTF( "set_props duration=%d pitch=%d delay=%d\r\n", (int)pMidi->m_ulDuration, (int)pMidi->m_ucPitch, (int)pMidi->m_ulDelay );
        }
    }
    else if (ucClass == DEVICE_CLASS_DISPLAY) {
		if ( !pProp->m_pvClassAttributes ) {
            pProp->m_pvClassAttributes = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_DISPLAY) );
            if ( !pProp->m_pvClassAttributes ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 1\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pProp->m_pvClassAttributes, 0, sizeof(DEVICE_ATTRIBUTES_DISPLAY) );
        }

        int iParamLen = 0;
        char* pcParam = json_parse_str( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_TEXT, &iParamLen );
        if ( iParamLen == 0 ) {
            //DEBUG_PRINTF( "json_parse_str failed! 2\r\n" );
            ret = -1;
            goto exit;
        }

        //
		// Set endpoint, text
        //
        DEVICE_ATTRIBUTES_DISPLAY* pAttributes = (DEVICE_ATTRIBUTES_DISPLAY*)pProp->m_pvClassAttributes;
        pAttributes->m_ucEndpoint = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ENDPOINT );
        if ( pAttributes->m_pcText ) {
        	if ( iParamLen > strlen( pAttributes->m_pcText ) ) {
				vPortFree( pAttributes->m_pcText );
				pAttributes->m_pcText = NULL;
        	}
        }
        if ( !pAttributes->m_pcText ) {
			pAttributes->m_pcText = pvPortMalloc( iParamLen+1 );
        }
		memset( pAttributes->m_pcText, 0, iParamLen+1 );
        strncpy( pAttributes->m_pcText, pcParam, iParamLen );
    }
    else if (ucClass == DEVICE_CLASS_LIGHT) {
        if ( !pProp->m_pvClassAttributes ) {
            pProp->m_pvClassAttributes = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_LIGHT) );
            if ( !pProp->m_pvClassAttributes ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 1\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pProp->m_pvClassAttributes, 0, sizeof(DEVICE_ATTRIBUTES_LIGHT) );
        }

        //
		// Set endpoint, color, brightness, timeout
        //
        DEVICE_ATTRIBUTES_LIGHT* pAttributes = (DEVICE_ATTRIBUTES_LIGHT*)pProp->m_pvClassAttributes;
        pAttributes->m_ucEndpoint   = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ENDPOINT );
        pAttributes->m_ulColor      = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_COLOR );
        pAttributes->m_ulBrightness = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_BRIGHTNESS );
        pAttributes->m_ulTimeout    = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_TIMEOUT );
    }
    else if (ucClass == DEVICE_CLASS_POTENTIOMETER) {
        if ( !pProp->m_pvClassAttributes ) {
            pProp->m_pvClassAttributes = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_POTENTIOMETER) );
            if ( !pProp->m_pvClassAttributes ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 1\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pProp->m_pvClassAttributes, 0, sizeof(DEVICE_ATTRIBUTES_POTENTIOMETER) );
        }

        //
		// Set mode, threshold (value, min, max, activate), alert (type, period)
        //
        DEVICE_ATTRIBUTES_POTENTIOMETER* pAttributes = (DEVICE_ATTRIBUTES_POTENTIOMETER*)pProp->m_pvClassAttributes;
        pAttributes->m_ucMode                  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_MODE );
        pAttributes->m_oAlert.m_ucType         = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ALERT_TYPE );
        pAttributes->m_oAlert.m_ulPeriod       = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ALERT_PERIOD );
        if ( pAttributes->m_ucMode != DEVICE_MODE_CONTINUOUS) {
            if ( pAttributes->m_ucMode == DEVICE_MODE_SINGLE_THRESHOLD) {
            	pAttributes->m_oThreshold.m_ulValue    = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_VALUE );
            }
            else if ( pAttributes->m_ucMode == DEVICE_MODE_DUAL_THRESHOLD) {
            	pAttributes->m_oThreshold.m_ulMinimum  = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_MINIMUM );
            	pAttributes->m_oThreshold.m_ulMaximum  = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_MAXIMUM );
            	pAttributes->m_oThreshold.m_ucActivate = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_ACTIVATE );
            }
        }
        else {
#if 1
            int iParamLen = 0;
            char* pcParam = NULL;

            pcParam = json_parse_str(mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &iParamLen);
            DEBUG_PRINTF( "devicename %d\r\n", iParamLen );
            if ( pcParam && iParamLen ) {
            	if (!pAttributes->m_oHardware.m_pcDeviceName) {
            		pAttributes->m_oHardware.m_pcDeviceName = pvPortMalloc( iParamLen+1 );
            	}
            	else if ( iParamLen > strlen(pAttributes->m_oHardware.m_pcDeviceName) ) {
            		vPortFree(pAttributes->m_oHardware.m_pcDeviceName);
            		pAttributes->m_oHardware.m_pcDeviceName = NULL;
            		pAttributes->m_oHardware.m_pcDeviceName = pvPortMalloc( iParamLen+1 );
            	}
            	if ( pAttributes->m_oHardware.m_pcDeviceName ) {
            		memset( pAttributes->m_oHardware.m_pcDeviceName, 0, iParamLen+1 );
            		strncpy( pAttributes->m_oHardware.m_pcDeviceName, pcParam, iParamLen );
                    DEBUG_PRINTF( "devicename %s\r\n", pAttributes->m_oHardware.m_pcDeviceName );
            	}
            }
#endif
            iot_modem_demo3_update(mqtt_subscribe_recv->payload);
        }
        DEBUG_PRINTF( "set_props mode=%d threshold=%d %d %d %d alert=%d %d hardware %p\r\n",
        	(int)pAttributes->m_ucMode,
			pAttributes->m_oThreshold.m_ulValue,
			pAttributes->m_oThreshold.m_ulMinimum,
			pAttributes->m_oThreshold.m_ulMaximum,
			pAttributes->m_oThreshold.m_ucActivate,
			pAttributes->m_oAlert.m_ucType,
			pAttributes->m_oAlert.m_ulPeriod,
			pAttributes->m_oHardware.m_pcDeviceName
			);
    }
    else if (ucClass == DEVICE_CLASS_TEMPERATURE) {
        if ( !pProp->m_pvClassAttributes ) {
            pProp->m_pvClassAttributes = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_TEMPERATURE) );
            if ( !pProp->m_pvClassAttributes ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 1\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pProp->m_pvClassAttributes, 0, sizeof(DEVICE_ATTRIBUTES_TEMPERATURE) );
        }

        //
		// Set mode, threshold (value, min, max, activate), alert (type, period)
        //
        DEVICE_ATTRIBUTES_TEMPERATURE* pAttributes = (DEVICE_ATTRIBUTES_TEMPERATURE*)pProp->m_pvClassAttributes;
        pAttributes->m_ucMode                  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_MODE );
		pAttributes->m_oAlert.m_ucType         = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ALERT_TYPE );
		pAttributes->m_oAlert.m_ulPeriod       = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ALERT_PERIOD );
        if ( pAttributes->m_ucMode != DEVICE_MODE_CONTINUOUS) {
            if ( pAttributes->m_ucMode == DEVICE_MODE_SINGLE_THRESHOLD) {
            	pAttributes->m_oThreshold.m_ulValue    = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_VALUE );
            }
            else if ( pAttributes->m_ucMode == DEVICE_MODE_DUAL_THRESHOLD) {
            	pAttributes->m_oThreshold.m_ulMinimum  = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_MINIMUM );
            	pAttributes->m_oThreshold.m_ulMaximum  = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_MAXIMUM );
            	pAttributes->m_oThreshold.m_ucActivate = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_ACTIVATE );
            }
        }
        else {
            int iParamLen = 0;
            char* pcParam = NULL;

            pcParam = json_parse_str(mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &iParamLen);
            DEBUG_PRINTF( "devicename %d\r\n", iParamLen );
            if ( pcParam && iParamLen ) {
            	if (!pAttributes->m_oHardware.m_pcDeviceName) {
            		pAttributes->m_oHardware.m_pcDeviceName = pvPortMalloc( iParamLen+1 );
            	}
            	else if ( iParamLen > strlen(pAttributes->m_oHardware.m_pcDeviceName) ) {
            		vPortFree(pAttributes->m_oHardware.m_pcDeviceName);
            		pAttributes->m_oHardware.m_pcDeviceName = NULL;
            		pAttributes->m_oHardware.m_pcDeviceName = pvPortMalloc( iParamLen+1 );
            	}
            	if ( pAttributes->m_oHardware.m_pcDeviceName ) {
            		memset( pAttributes->m_oHardware.m_pcDeviceName, 0, iParamLen+1 );
            		strncpy( pAttributes->m_oHardware.m_pcDeviceName, pcParam, iParamLen );
                    DEBUG_PRINTF( "devicename %s\r\n", pAttributes->m_oHardware.m_pcDeviceName );
            	}
            }
        }
        DEBUG_PRINTF( "set_props mode=%d threshold=%d %d %d %d alert=%d %d hardware %p\r\n",
        	(int)pAttributes->m_ucMode,
			pAttributes->m_oThreshold.m_ulValue,
			pAttributes->m_oThreshold.m_ulMinimum,
			pAttributes->m_oThreshold.m_ulMaximum,
			pAttributes->m_oThreshold.m_ucActivate,
			pAttributes->m_oAlert.m_ucType,
			pAttributes->m_oAlert.m_ulPeriod,
			pAttributes->m_oHardware.m_pcDeviceName
			);
    }
    else if (ucClass == DEVICE_CLASS_ANENOMOMETER) {
        if ( !pProp->m_pvClassAttributes ) {
            pProp->m_pvClassAttributes = pvPortMalloc( sizeof(DEVICE_ATTRIBUTES_ANENOMOMETER) );
            if ( !pProp->m_pvClassAttributes ) {
                //DEBUG_PRINTF( "pvPortMalloc failed! 1\r\n" );
                ret = -1;
                goto exit;
            }
            memset( pProp->m_pvClassAttributes, 0, sizeof(DEVICE_ATTRIBUTES_ANENOMOMETER) );
        }

        //
		// Set mode, threshold (value, min, max, activate), alert (type, period)
        //
        DEVICE_ATTRIBUTES_ANENOMOMETER* pAttributes = (DEVICE_ATTRIBUTES_ANENOMOMETER*)pProp->m_pvClassAttributes;
        pAttributes->m_ucMode                  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_MODE );
        pAttributes->m_oAlert.m_ucType         = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ALERT_TYPE );
        pAttributes->m_oAlert.m_ulPeriod       = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_ALERT_PERIOD );
        if ( pAttributes->m_ucMode != DEVICE_MODE_CONTINUOUS) {
            if ( pAttributes->m_ucMode == DEVICE_MODE_SINGLE_THRESHOLD) {
            	pAttributes->m_oThreshold.m_ulValue    = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_VALUE );
            }
            else if ( pAttributes->m_ucMode == DEVICE_MODE_DUAL_THRESHOLD) {
            	pAttributes->m_oThreshold.m_ulMinimum  = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_MINIMUM );
            	pAttributes->m_oThreshold.m_ulMaximum  = json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_MAXIMUM );
            	pAttributes->m_oThreshold.m_ucActivate = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_THRESHOLD_ACTIVATE );
            }
        }
        else {
            int iParamLen = 0;
            char* pcParam = NULL;

            pcParam = json_parse_str(mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &iParamLen);
            DEBUG_PRINTF( "devicename %d\r\n", iParamLen );
            if ( pcParam && iParamLen ) {
            	if (!pAttributes->m_oHardware.m_pcDeviceName) {
            		pAttributes->m_oHardware.m_pcDeviceName = pvPortMalloc( iParamLen+1 );
            	}
            	else if ( iParamLen > strlen(pAttributes->m_oHardware.m_pcDeviceName) ) {
            		vPortFree(pAttributes->m_oHardware.m_pcDeviceName);
            		pAttributes->m_oHardware.m_pcDeviceName = NULL;
            		pAttributes->m_oHardware.m_pcDeviceName = pvPortMalloc( iParamLen+1 );
            	}
            	if ( pAttributes->m_oHardware.m_pcDeviceName ) {
            		memset( pAttributes->m_oHardware.m_pcDeviceName, 0, iParamLen+1 );
            		strncpy( pAttributes->m_oHardware.m_pcDeviceName, pcParam, iParamLen );
                    DEBUG_PRINTF( "devicename %s\r\n", pAttributes->m_oHardware.m_pcDeviceName );

            	}
            }
        }
        DEBUG_PRINTF( "set_props mode=%d threshold=%d %d %d %d alert=%d %d hardware %p\r\n",
        	(int)pAttributes->m_ucMode,
			pAttributes->m_oThreshold.m_ulValue,
			pAttributes->m_oThreshold.m_ulMinimum,
			pAttributes->m_oThreshold.m_ulMaximum,
			pAttributes->m_oThreshold.m_ucActivate,
			pAttributes->m_oAlert.m_ucType,
			pAttributes->m_oAlert.m_ulPeriod,
			pAttributes->m_oHardware.m_pcDeviceName
			);
    }

exit:
    return ret;
}
#endif // defined(ENABLE_I2C) || defined(ENABLE_ADC) || defined(ENABLE_ONEWIRE) || defined(ENABLE_TPROBE)




///////////////////////////////////////////////////////////////////////////////////
// PROCESS MQTT SUBSCRIBED PACKETS
///////////////////////////////////////////////////////////////////////////////////

#define IS_API(api) (strncmp( ptr, api, len ) == 0)

static void user_subscribe_receive_cb( iot_subscribe_rcv* mqtt_subscribe_recv )
{
    char topic[MQTT_MAX_TOPIC_SIZE] = {0};
    char payload[MQTT_MAX_PAYLOAD_SIZE] = {0};
    int ret = 0;


    DEBUG_PRINTF( "\r\nRECV: %s [%d]\r\n", mqtt_subscribe_recv->topic, (unsigned int)mqtt_subscribe_recv->payload_len );
    DEBUG_PRINTF( "%s [%d]\r\n", mqtt_subscribe_recv->payload, strlen(mqtt_subscribe_recv->payload) );

    // get api
    char* ptr = strrchr(mqtt_subscribe_recv->topic, '/') + 1;
    int len = strlen(ptr);


    ///////////////////////////////////////////////////////////////////////////////////
    // STATUS
    ///////////////////////////////////////////////////////////////////////////////////
    if ( IS_API(API_GET_STATUS) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic);
        tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_STATUS, STATUS_STRING, g_ulDeviceStatus, VERSION_MAJOR, VERSION_MINOR);
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_SET_STATUS) ) {
        uint32_t ulDeviceStatus = json_parse_int(mqtt_subscribe_recv->payload, STATUS_STRING);
        switch (ulDeviceStatus) {
            case DEVICE_STATUS_RESTART: {
                g_ulDeviceStatus = DEVICE_STATUS_RESTARTING;
                tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
                tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_SET_STATUS, STATUS_STRING, g_ulDeviceStatus );
                ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
#if !VFW_APP
                xTaskCreate( restart_task, "restart_task", 64, NULL, 3, NULL );
#else
                kernel_create_task( restart_task, "restart_task", 64, NULL, 3, NULL );
#endif
                //DEBUG_PRINTF( "DEVICE_STATUS_RESTARTING\r\n" );
                break;
            }
            case DEVICE_STATUS_STOP: {
                if (g_ulDeviceStatus != DEVICE_STATUS_STOPPING && g_ulDeviceStatus != DEVICE_STATUS_STOPPED) {
                    g_ulDeviceStatus = DEVICE_STATUS_STOPPING;
                    tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
                    tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_SET_STATUS, STATUS_STRING, g_ulDeviceStatus );
                    ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                    DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
                    //DEBUG_PRINTF( "PUB:  %s %s\r\n", topic, payload );
                    // TODO
                    g_ulDeviceStatus = DEVICE_STATUS_STOPPED;
                    break;
                }
                // fall through to default
            }
            case DEVICE_STATUS_START: {
                if (g_ulDeviceStatus != DEVICE_STATUS_STARTING && g_ulDeviceStatus != DEVICE_STATUS_RUNNING) {
                    g_ulDeviceStatus = DEVICE_STATUS_STARTING;
                    tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
                    tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_SET_STATUS, STATUS_STRING, g_ulDeviceStatus );
                    ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                    DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
                    // TODO
                    g_ulDeviceStatus = DEVICE_STATUS_RUNNING;
                    break;
                }
                // fall through to default
            }
            default: {
                tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
                tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_SET_STATUS, STATUS_STRING, g_ulDeviceStatus );
                ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
                break;
            }
        }
    }


#if ENABLE_UART
    ///////////////////////////////////////////////////////////////////////////////////
    // UART
    ///////////////////////////////////////////////////////////////////////////////////
    else if ( IS_API(API_GET_UARTS) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_UARTS, ENABLED_STRING, g_ucUartEnabled);
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_GET_UART_PROPERTIES) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_UART_PROPERTIES,
            UART_PROPERTIES_BAUDRATE,
            g_oUartProperties.m_ucBaudrate,
            UART_PROPERTIES_PARITY,
            g_oUartProperties.m_ucParity,
            UART_PROPERTIES_FLOWCONTROL,
            // uart_flow_xon_xoff is the max value but uart_flow_dtr_dsr is not exposed
            g_oUartProperties.m_ucFlowcontrol == uart_flow_xon_xoff ? uart_flow_dtr_dsr : g_oUartProperties.m_ucFlowcontrol,
            UART_PROPERTIES_STOPBITS,
            // uart_stop_bits_2 is the max value but uart_stop_bits_1_5 is not exposed
            g_oUartProperties.m_ucStopbits == uart_stop_bits_2 ? uart_stop_bits_1_5 : g_oUartProperties.m_ucStopbits,
            UART_PROPERTIES_DATABITS,
            // only uart_data_bits_7 and uart_data_bits_8 are exposed
            g_oUartProperties.m_ucDatabits - uart_data_bits_7 // subtract offset
            );
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_SET_UART_PROPERTIES) ) {
        // get the parameter values
        uint8_t ucDatabits    = (uint8_t)json_parse_int(mqtt_subscribe_recv->payload, UART_PROPERTIES_DATABITS);
        uint8_t ucStopbits    = (uint8_t)json_parse_int(mqtt_subscribe_recv->payload, UART_PROPERTIES_STOPBITS);
        uint8_t ucFlowcontrol = (uint8_t)json_parse_int(mqtt_subscribe_recv->payload, UART_PROPERTIES_FLOWCONTROL);
        uint8_t ucParity      = (uint8_t)json_parse_int(mqtt_subscribe_recv->payload, UART_PROPERTIES_PARITY);
        uint8_t ucBaudrate    = (uint8_t)json_parse_int(mqtt_subscribe_recv->payload, UART_PROPERTIES_BAUDRATE);
        DEBUG_PRINTF( "ucBaudrate=%d ucParity=%d ucFlowcontrol=%d, ucStopbits=%d, ucDatabits=%d\r\n",
            ucBaudrate, ucParity, ucFlowcontrol, ucStopbits, ucDatabits );

        // baudrate index should be valid
        if (ucBaudrate < UART_PROPERTIES_BAUDRATE_COUNT) {
            g_oUartProperties.m_ucBaudrate = ucBaudrate;
        }
        // uart_parity_even is the max value, ergo use <=
        if (ucParity <= uart_parity_even) {
            g_oUartProperties.m_ucParity = ucParity;
        }
        // uart_flow_xon_xoff is the max value but uart_flow_dtr_dsr is not exposed, ergo use <
        if (ucFlowcontrol < uart_flow_xon_xoff) {
            if (ucFlowcontrol == uart_flow_dtr_dsr) {
                g_oUartProperties.m_ucFlowcontrol = uart_flow_xon_xoff;
            }
            else {
                g_oUartProperties.m_ucFlowcontrol = ucFlowcontrol;
            }
        }
        // uart_stop_bits_2 is the max value but uart_stop_bits_1_5 is not exposed, ergo use <
        if (ucStopbits < uart_stop_bits_2) {
            if (ucStopbits == uart_stop_bits_1_5) {
                g_oUartProperties.m_ucStopbits = uart_stop_bits_2;
            }
            else {
                g_oUartProperties.m_ucStopbits = ucStopbits;
            }
        }
        // only uart_data_bits_7 and uart_data_bits_8 are exposed
        if (ucDatabits < 2) {
            g_oUartProperties.m_ucDatabits = ucDatabits + uart_data_bits_7; // add offset
        }

        //DEBUG_PRINTF( "UPD ucBaudrate=%d ucParity=%d ucFlowcontrol=%d, ucStopbits=%d, ucDatabits=%d\r\n",
        //        g_oUartProperties.m_ucBaudrate,
        //        g_oUartProperties.m_ucParity,
        //        g_oUartProperties.m_ucFlowcontrol,
        //        g_oUartProperties.m_ucStopbits,
        //        g_oUartProperties.m_ucDatabits );
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );

        // configure UART with the new values, uart_soft_reset is needed to avoid distorted text when changing databits or parity
        iot_modem_uart_enable(&g_oUartProperties, 1, 1);
    }
    else if ( IS_API(API_ENABLE_UART) ) {
        uint8_t ucEnabled = (uint8_t)json_parse_int(mqtt_subscribe_recv->payload, ENABLE_STRING);
        //DEBUG_PRINTF( "ucEnabled=%d\r\n", ucEnabled );

        if ( g_ucUartEnabled != ucEnabled ) {
            if (ucEnabled == 0) {
                // Disable UART by closing the UART
                iot_modem_uart_enable(&g_oUartProperties, 0, 1);
            }
            else {
                // Enable UART by opening the UART
                iot_modem_uart_enable(&g_oUartProperties, 1, 0);
            }
            g_ucUartEnabled = ucEnabled;
        }
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
#endif // ENABLE_UART



#if ENABLE_I2C
    ///////////////////////////////////////////////////////////////////////////////////
    // I2C
    ///////////////////////////////////////////////////////////////////////////////////
    else if ( IS_API(API_GET_I2C_DEVICES) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        if(iot_modem_i2c_get_devices( mqtt_subscribe_recv->payload,payload, sizeof(payload)) < 0){
            tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
        }
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DPRINTF_WARN( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_ENABLE_I2C_DEVICE) ) {
        iot_modem_i2c_enable(mqtt_subscribe_recv->payload,0xff);
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
    else if ( IS_API(API_GET_I2C_DEVICE_PROPERTIES) )
    {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic);
        if(iot_modem_i2c_get_properties( mqtt_subscribe_recv->payload,payload, sizeof(payload)) < 0){
            ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
        }else{
            ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
            DPRINTF_WARN( "PUB:  %s %s\r\n\r\n", topic, payload );
        }
    }
    else if ( IS_API(API_SET_I2C_DEVICE_PROPERTIES) ) {
        iot_modem_i2c_set_properties(mqtt_subscribe_recv->payload,0xff);
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
#endif // ENABLE_I2C



#if ENABLE_ADC
    ///////////////////////////////////////////////////////////////////////////////////
    // ADC
    ///////////////////////////////////////////////////////////////////////////////////
    else if ( IS_API(API_GET_ADC_DEVICES) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        if(iot_modem_adc_get_devices( mqtt_subscribe_recv->payload,payload, sizeof(payload)) < 0){
            tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
            //ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
        }
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DPRINTF_WARN( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_ENABLE_ADC_DEVICE) ) {
        iot_modem_adc_enable(mqtt_subscribe_recv->payload,0xff);
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
    else if ( IS_API(API_GET_ADC_DEVICE_PROPERTIES) ){

        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic);
        if(iot_modem_adc_get_properties( mqtt_subscribe_recv->payload,payload, sizeof(payload)) < 0){
            ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
        }else{
            ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
            DPRINTF_WARN( "PUB:  %s %s\r\n\r\n", topic, payload );
        }
    }
    else if ( IS_API(API_SET_ADC_DEVICE_PROPERTIES) ) {
        iot_modem_adc_set_properties(mqtt_subscribe_recv->payload,0xff);
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
    else if ( IS_API(API_GET_ADC_VOLTAGE) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_ADC_VOLTAGE, VOLTAGE_STRING, g_ucADCVoltage );
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_SET_ADC_VOLTAGE) ) {
        uint8_t ucVoltage = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, VOLTAGE_STRING );
        if (ucVoltage < ADC_VOLTAGE_COUNT) {
            if ( g_ucADCVoltage != ucVoltage ) {
                iot_modem_adc_set_voltage(ucVoltage);
                g_ucADCVoltage = ucVoltage;
            }
        }
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
#endif // ENABLE_ADC

#if ENABLE_ONEWIRE
    ///////////////////////////////////////////////////////////////////////////////////
    // 1WIRE
    ///////////////////////////////////////////////////////////////////////////////////
    else if ( IS_API(API_GET_1WIRE_DEVICES) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        DEBUG_PRINTF( "1WIRE %d GETDEVS\r\n", ucNumber );

        if (ucNumber < ONEWIRE_COUNT) {
			DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)(g_p1WIREProperties+ucNumber*sizeof(DEVICE_PROPERTIES));
			if (!pProp) {
				tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
				tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
				ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
			}
			else {
				tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
				tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES,
					DEVICE_PROPERTIES_CLASS,
					pProp->m_ucClass,
					ENABLED_STRING,
					pProp->m_ucEnabled
					);
				ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );

#ifndef ENABLE_CNT_PUBLISH
                if(pProp->m_ucEnabled)
                {
                    if(!iot_modem_1wire_get_sensor_reading(NULL,topic,64,payload,128)){
                        pub_msg(payload,TOPIC_PAYLOAD);
                        int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                        DEBUG_PRINTF( ">>PUB: %s %s\r\n", topic, payload);
                    }else{
                        tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
                        int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1);
                    }
                }
#endif
			}
        }
        else {
			tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
			tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
			ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        }
    }
    else if ( IS_API(API_ENABLE_1WIRE_DEVICE) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        uint8_t ucEnabled = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, ENABLE_STRING );
        DEBUG_PRINTF( "1WIRE %d ENABLE=%d\r\n", ucNumber, ucEnabled );

        if (ucNumber < ONEWIRE_COUNT && ucEnabled < 2) {
            int index = 0xFF;
            DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_p1WIREProperties;
            if (pProp) {
				for ( int i=0; i<g_uc1WIREPropertiesCount; i++, pProp++ ) {
					if ( pProp->m_ucSlot == ucNumber) {
						DEBUG_PRINTF( "found\r\n");
						index = i;
						break;
					}
				}
				if ( index != 0xFF ) {
					if ( pProp->m_ucEnabled != ucEnabled ) {
						pProp->m_ucEnabled = ucEnabled;
						iot_modem_1wire_enable(pProp);
					}
				}
            }
        }
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
    else if ( IS_API(API_GET_1WIRE_DEVICE_PROPERTIES) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        DEBUG_PRINTF( "1WIRE %d GET\r\n", ucNumber );

        int index = 0xFF;
        DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_p1WIREProperties;
        if (pProp) {
			for ( int i=0; i<g_uc1WIREPropertiesCount; i++, pProp++ ) {
				//DEBUG_PRINTF( "class=%d enabled=%d number=%d address=%d  index=%d\r\n", (int)pProp->m_ucClass, (int)pProp->m_ucEnabled, (int)pProp->m_ucSlot, (int)pProp->m_ucAddress, i );
				if ( pProp->m_ucSlot == ucNumber ) {
					index = i;
					break;
				}
			}

			if ( index != 0xFF ) {
				tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
				get_props( pProp, payload, sizeof(payload) );
				ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
				DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
			}
			else {
				ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
			}
        }
        else {
			ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
        }
    }
    else if ( IS_API(API_SET_1WIRE_DEVICE_PROPERTIES) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        uint8_t ucClass   = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_CLASS );
        DEBUG_PRINTF( "1WIRE %d class=%d SET %s\r\n", ucNumber, ucClass, mqtt_subscribe_recv->payload );

        if ( g_p1WIREProperties == NULL ) {
            g_p1WIREProperties = pvPortMalloc( g_uc1WIREPropertiesCount * sizeof(DEVICE_PROPERTIES) );
            if ( !g_p1WIREProperties ) {
                DEBUG_PRINTF( "pvPortMalloc failed!\r\n" );
                ret = -1;
                goto exit;
            }
            memset( g_p1WIREProperties, 0, g_uc1WIREPropertiesCount * sizeof(DEVICE_PROPERTIES) );

            DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_p1WIREProperties;
            ret = set_props( pProp, ucNumber, 0xFF, ucClass, mqtt_subscribe_recv );
            if ( ret < 0 ) {
                goto exit;
            }

            iot_modem_1wire_set_properties( pProp );
        }
        else {
            // find the 1WIRE device and set the values
            int next = 0xFF;
            int index = 0xFF;
            DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_p1WIREProperties;

            for ( int i=0; i<g_uc1WIREPropertiesCount; i++,pProp++ ) {
                if ( pProp->m_ucAddress == 0 && next == 0xFF ) {
                    next = i;
                }
                if ( pProp->m_ucSlot == ucNumber && pProp->m_ucClass == ucClass ) {
                    index = i;
                    break;
                }
            }
            if (index != 0xFF) {
                ret = set_props( (DEVICE_PROPERTIES*)(g_p1WIREProperties+index*sizeof(DEVICE_PROPERTIES)),
                	ucNumber, 0xFF, ucClass, mqtt_subscribe_recv );
                if ( ret < 0 ) {
                    goto exit;
                }

                iot_modem_1wire_set_properties( (DEVICE_PROPERTIES*)(g_p1WIREProperties+index*sizeof(DEVICE_PROPERTIES)) );
            }
            else if (next != 0xFF) {
                ret = set_props( (DEVICE_PROPERTIES*)(g_p1WIREProperties+next*sizeof(DEVICE_PROPERTIES)),
                	ucNumber, 0xFF, ucClass, mqtt_subscribe_recv );
                if ( ret < 0 ) {
                    goto exit;
                }

                iot_modem_1wire_set_properties( (DEVICE_PROPERTIES*)(g_p1WIREProperties+next*sizeof(DEVICE_PROPERTIES)) );
            }
        }
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
#endif // ENABLE_ONEWIRE

#if ENABLE_TPROBE
    ///////////////////////////////////////////////////////////////////////////////////
    // TPROBE
    ///////////////////////////////////////////////////////////////////////////////////
    else if ( IS_API(API_GET_TPROBE_DEVICES) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        DEBUG_PRINTF( "TPROBE %d GETDEVS\r\n", ucNumber );

        if (ucNumber < TPROBE_COUNT) {
			DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)(g_pTPROBEProperties+ucNumber*sizeof(DEVICE_PROPERTIES));
			if (!pProp) {
				tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
				tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
				ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
			}
			else {
				tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
				tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES,
					DEVICE_PROPERTIES_CLASS,
					pProp->m_ucClass,
					ENABLED_STRING,
					pProp->m_ucEnabled
					);
				ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );

#ifndef ENABLE_CNT_PUBLISH
                if(pProp->m_ucEnabled)
                {
                    if(!iot_modem_tprobe_get_sensor_reading(NULL,topic,64,payload,128)){
                        pub_msg(payload,TOPIC_PAYLOAD);
                        int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
                        DEBUG_PRINTF( ">>PUB: %s %s\r\n", topic, payload);
                    }else{
                        tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
                        int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1);
                    }
                }
#endif

			}
        }
        else {
			tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
			tfp_snprintf( payload, sizeof(payload), PAYLOAD_API_GET_XXX_DEVICES_EMPTY );
			ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        }
    }
    else if ( IS_API(API_ENABLE_TPROBE_DEVICE) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        uint8_t ucEnabled = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, ENABLE_STRING );
        DEBUG_PRINTF( "TPROBE %d ENABLE=%d\r\n", ucNumber, ucEnabled );

        if (ucNumber < TPROBE_COUNT && ucEnabled < 2) {
            int index = 0xFF;
            DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_pTPROBEProperties;
            if (pProp) {
                for ( int i=0; i<g_ucTPROBEPropertiesCount; i++, pProp++ ) {
                    if ( pProp->m_ucSlot == ucNumber) {
                        DEBUG_PRINTF( "found\r\n");
                        index = i;
                        break;
                    }
                }
                if ( index != 0xFF ) {
                    if ( pProp->m_ucEnabled != ucEnabled ) {
                        pProp->m_ucEnabled = ucEnabled;
                        iot_modem_tprobe_enable(pProp);
                    }
                }
            }
        }
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
    else if ( IS_API(API_GET_TPROBE_DEVICE_PROPERTIES) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        DEBUG_PRINTF( "TPROBE %d GET\r\n", ucNumber );

        int index = 0xFF;
        DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_pTPROBEProperties;
        if (pProp) {
			for ( int i=0; i<g_ucTPROBEPropertiesCount; i++, pProp++ ) {
				//DEBUG_PRINTF( "class=%d enabled=%d number=%d address=%d  index=%d\r\n", (int)pProp->m_ucClass, (int)pProp->m_ucEnabled, (int)pProp->m_ucSlot, (int)pProp->m_ucAddress, i );
				if ( pProp->m_ucSlot == ucNumber ) {
					index = i;
					break;
				}
			}

			if ( index != 0xFF ) {
				tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
				get_props( pProp, payload, sizeof(payload) );
 			    ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
				DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
			}
			else {
				ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
			}
        }
		else {
			ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
		}
    }
    else if ( IS_API(API_SET_TPROBE_DEVICE_PROPERTIES) ) {
        uint8_t ucNumber  = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, NUMBER_STRING ) - 1;
        uint8_t ucClass   = (uint8_t)json_parse_int( mqtt_subscribe_recv->payload, DEVICE_PROPERTIES_CLASS );
        DEBUG_PRINTF( "TPROBE %d SET class=%d %s\r\n", ucNumber, ucClass, mqtt_subscribe_recv->payload );

        if ( g_pTPROBEProperties == NULL ) {
            g_pTPROBEProperties = pvPortMalloc( g_ucTPROBEPropertiesCount * sizeof(DEVICE_PROPERTIES) );
            if ( !g_pTPROBEProperties ) {
                DEBUG_PRINTF( "pvPortMalloc failed!\r\n" );
                ret = -1;
                goto exit;
            }
            memset( g_pTPROBEProperties, 0, g_ucTPROBEPropertiesCount * sizeof(DEVICE_PROPERTIES) );

            DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_pTPROBEProperties;
            ret = set_props( pProp, ucNumber, 0xFF, ucClass, mqtt_subscribe_recv );
            if ( ret < 0 ) {
                goto exit;
            }

            iot_modem_tprobe_set_properties( pProp );
        }
        else {
            // find the TPROBE device and set the values
            int next = 0xFF;
            int index = 0xFF;
            DEVICE_PROPERTIES* pProp = (DEVICE_PROPERTIES*)g_pTPROBEProperties;

            for ( int i=0; i<g_ucTPROBEPropertiesCount; i++,pProp++ ) {
                if ( pProp->m_ucAddress == 0 && next == 0xFF ) {
                    next = i;
                }
                if ( pProp->m_ucSlot == ucNumber && pProp->m_ucClass == ucClass ) {
                    index = i;
                    break;
                }
            }
            if (index != 0xFF) {
                ret = set_props( (DEVICE_PROPERTIES*)(g_pTPROBEProperties+index*sizeof(DEVICE_PROPERTIES)),
                	ucNumber, 0xFF, ucClass, mqtt_subscribe_recv );
                if ( ret < 0 ) {
                    goto exit;
                }

                iot_modem_tprobe_set_properties( (DEVICE_PROPERTIES*)(g_pTPROBEProperties+index*sizeof(DEVICE_PROPERTIES)) );
            }
            else if (next != 0xFF) {
                ret = set_props( (DEVICE_PROPERTIES*)(g_pTPROBEProperties+next*sizeof(DEVICE_PROPERTIES)),
                	ucNumber, 0xFF, ucClass, mqtt_subscribe_recv );
                if ( ret < 0 ) {
                    goto exit;
                }

                iot_modem_tprobe_set_properties( (DEVICE_PROPERTIES*)(g_pTPROBEProperties+next*sizeof(DEVICE_PROPERTIES)) );
            }
        }
        ret = publish_default( topic, sizeof(topic), payload, sizeof(payload), mqtt_subscribe_recv );
    }
#endif // ENABLE_TPROBE



#if ENABLE_NOTIFICATIONS
    ///////////////////////////////////////////////////////////////////////////////////
    // NOTIFICATIONS
    ///////////////////////////////////////////////////////////////////////////////////
    else if ( IS_API(API_TRIGGER_NOTIFICATION) ) {
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        tfp_snprintf( payload, sizeof(payload), "%s", mqtt_subscribe_recv->payload );
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else if ( IS_API(API_RECEIVE_NOTIFICATION) ) {
        int iParamLen = 0;
        char* pcParam = NULL;

        pcParam = json_parse_str(mqtt_subscribe_recv->payload, MENOS_MESSAGE, &iParamLen);
        char message[UART_ATCOMMAND_MAX_MESSAGE_SIZE] = {0};
        strncpy(message, pcParam, iParamLen);

        pcParam = json_parse_str(mqtt_subscribe_recv->payload, MENOS_SENDER, &iParamLen);
        char sender[16+1] = {0};
        strncpy(sender, pcParam, iParamLen);

        DEBUG_PRINTF( "From %s:\r\n", sender );
        DEBUG_PRINTF( "%s\r\n\r\n", message );
    }
    else if ( IS_API(API_STATUS_NOTIFICATION) ) {
        int iParamLen = 0;
        char* pcParam = NULL;

        pcParam = json_parse_str(mqtt_subscribe_recv->payload, STATUS_STRING, &iParamLen);
        char status[UART_ATCOMMAND_MAX_STATUS_SIZE] = {0};
        strncpy(status, pcParam, iParamLen);
        DEBUG_PRINTF( "\r\n%s\r\n\r\n", status );
    }
#endif // ENABLE_NOTIFICATIONS

#if SENSOR_DASHBOARD
    else if ( IS_API(API_GET_PERIPHERAL_DEVICES) )
    {
#if ENABLE_ADC
        tfp_snprintf( topic, sizeof(topic), "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),API_GET_PERIPHERAL_DEVICES);
        len = tfp_snprintf(payload,sizeof(payload),"{\"value\": {");
        len += iot_get_adc_devices(payload+len,sizeof(payload)-len);
        payload[len++] = '}';
        payload[len++] = '}';
        DPRINTF_INFO("\r\n%s, %s\r\n",topic,payload);
        int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
#endif
#if ENABLE_I2C
        len = 0;
        tfp_snprintf( topic, sizeof(topic), "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),API_GET_PERIPHERAL_DEVICES);
        len = iot_get_i2c_devices(payload+len,sizeof(payload)-len);
        DPRINTF_INFO("\r\n%s, %s\r\n",topic,payload);
        int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
#endif
#if 0
    	int len = 0;
    	char i = 0;
    	char *prpptr = NULL;


    	tfp_snprintf( topic, sizeof(topic), "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),API_GET_PERIPHERAL_DEVICES);
#if ENABLE_ONEWIRE
    	prpptr = g_p1WIREProperties;
    	for(i = 0;i<g_uc1WIREPropertiesCount;i++){
    		DEVICE_PROPERTIES *prop = (DEVICE_PROPERTIES*)prpptr;
    		if(prop  != NULL){
    			if(prop->m_ucEnabled){
    			 len += tfp_snprintf(payload + len ,sizeof(payload) - len,GET_IND_PAYLOAD,"1wire",i+1,DEVICE_CLASS_TEMPERATURE,1);
    			 payload[len++] = ',';
    			}
    			prpptr+= sizeof(DEVICE_PROPERTIES);
    		}
    	}

#endif
    //	DPRINTF_INFO("\r\n%s, %s\r\n",topic,payload);
#if ENABLE_ADC
    	len+=iot_get_adc_devices(g_pADCProperties,payload+len,sizeof(payload)-len, g_ucADCPropertiesCount);
#endif
    //	DPRINTF_INFO("\r\n%s, %s\r\n",topic,payload);
#if ENABLE_TPROBE
    	prpptr = g_pTPROBEProperties;
    	for(i = 0;i<g_ucTPROBEPropertiesCount;i++){
    		DEVICE_PROPERTIES *prop = (DEVICE_PROPERTIES*)prpptr;
    		if(prop  != NULL){
    			if(prop->m_ucEnabled){
    				len += tfp_snprintf(payload + len ,sizeof(payload) - len,GET_IND_PAYLOAD,"tprobe",i+1,DEVICE_CLASS_TEMPERATURE,1);
    				 payload[len++] = ',';
    			}
    			prpptr+= sizeof(DEVICE_PROPERTIES);
    		}
    	}
#endif
    	payload[len-1] = '\0';
    	DPRINTF_INFO("\r\n%s, %s\r\n",topic,payload);
    	pub_msg(payload,GET_DEV_PAYLOAD);
    	DPRINTF_INFO("\r\n%s, %s\r\n",topic,payload);
    	int ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
#ifndef ENABLE_CNT_PUBLISH
#endif
#endif
    }
#endif
    else if ( IS_API(API_RECEIVE_SENSOR_READING) )
    {
        iot_modem_demo3_update(mqtt_subscribe_recv->payload);
    }
    else if ( IS_API(API_SENSORCFG_RECEIVE) ) {
    //    iot_adc_bootup_config( mqtt_subscribe_recv->payload );
        iot_i2c_bootup_config( mqtt_subscribe_recv->payload );
    }
    else if ( IS_API(API_SETTINGS) ) {
        //    iot_adc_bootup_config( mqtt_subscribe_recv->payload );
        g_pub_time = (uint32_t)((uint8_t)json_parse_int( mqtt_subscribe_recv->payload, "sensorrate") * 1000L);
        DPRINTF_WARN("PUB TIME Updated to %lu mSecs",g_pub_time);

    }else if ( IS_API(API_GET_SETTINGS) ) {
        //    iot_adc_bootup_config( mqtt_subscribe_recv->payload );
        tfp_snprintf( topic, sizeof(topic), "%s%s", PREPEND_REPLY_TOPIC, mqtt_subscribe_recv->topic );
        tfp_snprintf( payload, sizeof(payload), "{\"value\" : {\"sensorrate\": %d}}", g_pub_time / 1000);
        ret = iot_publish( g_handle, topic, payload, strlen(payload), 1 );
        DEBUG_PRINTF( "PUB:  %s %s\r\n\r\n", topic, payload );
    }
    else {
        DEBUG_PRINTF( "UNKNOWN API. Is peripheral enabled? UART=%d GPIO=%d I2C=%d ADC=%d 1WIRE=%d TPROBE=%d\r\n",
       		ENABLE_UART,
       		ENABLE_GPIO,
       		ENABLE_I2C,
       		ENABLE_ADC,
       		ENABLE_ONEWIRE,
       		ENABLE_TPROBE
        );
    }
exit:
    if (ret < 0) {
        g_exit = 1;
    }
}


#if 0
void ISR_gpio(){
  static bool prog_debug = true;
  if (gpio_is_interrupted(9))
  {
      prog_debug ^=1;
      if(!prog_debug){
          DEBUG_PRINTF("USB->Programmer");
      }else{
          DEBUG_PRINTF("USB->Debugger");
      }
      gpio_write(8,prog_debug);
  }
}
#endif
