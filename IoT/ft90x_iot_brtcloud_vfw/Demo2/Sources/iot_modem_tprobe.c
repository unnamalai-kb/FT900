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
#include "iot_modem__debug.h"
#include "json.h"

#include <iot_tprobe.h>

#include <string.h>
#include <stdlib.h>





#if ENABLE_TPROBE
uint8_t g_tprobe_task_status=0;
extern TaskHandle_t g_iot_app_handle; // used by iot_modem_gpio_process()

void iot_modem_tprobe_init()
{
    DEBUG_PRINTF( "TPROBE INIT\r\n" );

    // MODIFY ME
	iot_tprobe_init();
}

int iot_modem_tprobe_enable(DEVICE_PROPERTIES* properties)
{
    DEBUG_PRINTF( "TPROBE ENABLE \r\n" );

    if(properties == NULL ) return -1;
    // MODIFY ME
    if (properties->m_ucEnabled) {
    	xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_TPROBE), eSetBits);
    }
    else {
    	xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_TPROBE), eNoAction);
    	g_tprobe_task_status = 0;
    }

    return 1;
}

void iot_modem_tprobe_set_properties(DEVICE_PROPERTIES* properties)
{
    DEBUG_PRINTF( "TPROBE SETPROP\r\n" );

    // MODIFY ME

    if (properties->m_ucClass == DEVICE_CLASS_TEMPERATURE) {

    }
}

uint16_t iot_modem_tprobe_get_sensor_reading(DEVICE_PROPERTIES* properties,char *topic,int len, char *payload,int plen)
{
    // MODIFY ME
	TProbe_t tp;
	int ret = 0;
	iot_tprobe_run(&tp);
	if(tp.humidity == 0xffff) return 1;
	tfp_snprintf( topic, len, "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),"sensor_reading");
	tfp_snprintf( payload, plen, TOPIC_MULTI_SENSOR, "tprobe1",tp.temperature / 10,tp.temperature%10,DEVICE_CLASS_TEMPERATURE,tp.humidity/10,tp.humidity%10,DEVICE_CLASS_HUMIDITY);
//	DPRINTF_INFO("%s",topic);
//	DPRINTF_INFO("%s",payload);
	return 0;
   // return 1;
}

#endif // ENABLE_TPROBE
