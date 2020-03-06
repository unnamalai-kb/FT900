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


#include <string.h>
#include <stdlib.h>

#include <iot_ow.h>



#if ENABLE_ONEWIRE
uint8_t g_1wire_task_status=0;
extern TaskHandle_t g_iot_app_handle; // used by iot_modem_gpio_process()
void iot_modem_1wire_init()
{
    DEBUG_PRINTF( "1WIRE INIT\r\n" );

    // MODIFY ME
	iot_ow_init();
}

int iot_modem_1wire_enable(DEVICE_PROPERTIES* properties)
{
    DEBUG_PRINTF( "1WIRE ENABLE\r\n" );

    // MODIFY ME

    if (properties->m_ucEnabled) {
    	xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_1WIRE), eSetBits);
    }
    else {
    	xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_1WIRE), eNoAction);
    	g_1wire_task_status = 0;
    }

    return 1;
}

void iot_modem_1wire_set_properties(DEVICE_PROPERTIES* properties)
{
    DEBUG_PRINTF( "1WIRE SETPROP\r\n" );

    // MODIFY ME

    if (properties->m_ucClass == DEVICE_CLASS_TEMPERATURE) {

    }

}

uint16_t iot_modem_1wire_get_sensor_reading(DEVICE_PROPERTIES* properties,char *topic,int len ,char *payload,int plen)
{
	uint16_t temp=0;
    // MODIFY ME

    if(iot_ow_read_temperature(&temp) == -1) return 1;
    	tfp_snprintf( topic,len, "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),"sensor_reading");
    	tfp_snprintf( payload, plen, TOPIC_IND_SENSOR, "1wire1", (temp >> 4) & 0x7f,(temp & 0x0f), DEVICE_CLASS_TEMPERATURE);
    return 0;
}

#endif // ENABLE_ONEWIRE
