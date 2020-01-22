#include <ft900.h>
#include "tinyprintf.h"

/* FreeRTOS Headers. */
#include "FreeRTOS.h"
#include "task.h"

/* netif Abstraction Header. */
#include "net.h"

/* IOT Headers. */
#include <iot_config.h>
#include "iot/iot.h"
#include "iot/iot_utils.h"

/* IoT Modem */
#include "iot_modem.h"
#include "iot_modem__debug.h"
#include "json.h"


#include <string.h>
#include <stdlib.h>





#if ENABLE_TPROBE

void iot_modem_tprobe_init()
{
    DEBUG_PRINTF( "TPROBE INIT\r\n" );

    // MODIFY ME
}

int iot_modem_tprobe_enable(DEVICE_PROPERTIES* properties)
{
    DEBUG_PRINTF( "TPROBE ENABLE\r\n" );

    // MODIFY ME

    if (properties->m_ucEnabled) {

    }
    else {

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

uint32_t iot_modem_tprobe_get_sensor_reading(DEVICE_PROPERTIES* properties)
{
    // MODIFY ME

    return 1;
}

#endif // ENABLE_TPROBE
