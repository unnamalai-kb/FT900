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

#include "iot_demos.h"
#include "qs_fs_wind_sensor.h"






#if ENABLE_ADC

#define MAX_ADC_CHANNELS    2
typedef struct{
    uint8_t range;
    uint8_t class;
    uint8_t mode;
    uint16_t t_val;
    uint8_t t_min;
    uint8_t t_max;
    uint8_t t_activate;
    uint8_t alert_type;
    uint16_t alert_period;
    char src_device_name[32];
    uint8_t enable;
    uint8_t configured;
}ADCConfig_t;

static ADCConfig_t adc_config[MAX_ADC_CHANNELS];
static uint8_t adc_configured_channels = 0;

EWDemoSensors_Cfg_t ew_sensor_cfg;



extern TaskHandle_t g_iot_app_handle; // used by iot_modem_gpio_process()
void iot_modem_adc_init(int voltage)
{
    DEBUG_PRINTF( "ADC INIT\r\n" );

    // MODIFY ME
	ew_sensor_cfg.adc_in_selection = voltage;
	iot_demo4_init(&ew_sensor_cfg);
    iot_modem_adc_set_voltage(voltage);
    memset(adc_config,0,sizeof(adc_config));
}

int iot_modem_adc_enable(const char *json,uint8_t ucNumber)
{
    DEBUG_PRINTF( "ADC ENABLE\r\n" );
    if(ucNumber == 0xff)
            ucNumber  = (uint8_t)json_parse_int( json, NUMBER_STRING ) - 1;
    int8_t ucEnabled = (uint8_t)json_parse_int( json, ENABLE_STRING );
        if(ucEnabled < 0) return 0;
#if 0
    if (ucEnabled) {
        adc_config[ucNumber].enable = 1;
        if(ucNumber == 0){
            xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC0), eSetBits);
        }
        else if(ucNumber == 1){
            xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC1), eSetBits);
        }
    }
    else{
        adc_config[ucNumber].enable = 0;
        if(ucNumber == 0){
            xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC0), eNoAction);
        }
        else if(ucNumber == 1){
            xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC1), eNoAction);
        }
    }
#else
    adc_config[ucNumber].enable = ucEnabled;

    if(adc_config[0].enable == 1 || adc_config[1].enable == 1)
        xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC0), eSetBits);
    else
        xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC0), eNoAction);
#endif
    return 1;
}



#if (defined DEMO1)
uint32_t iot_modem_adc_get_sensor_reading(DEVICE_PROPERTIES* properties,char *topic,int len ,char *payload,int plen)

{
    // MODIFY ME
	EWDemoSensors_t ws;
	iot_ws_run(&ws);	//DEBUG_PRINTF( "publish anemometer payload\r\n" );
	tfp_snprintf( topic,len, "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),"sensor_reading");
	tfp_snprintf( payload, plen, TOPIC_IND_SENSOR, "adc1",ws.wind.speed/1000,ws.wind.speed%1000, DEVICE_CLASS_ANENOMOMETER);
    return ws.wind.speed;
}
#elif (defined DEMO3)
uint32_t iot_modem_adc_get_sensor_reading(DEVICE_PROPERTIES* properties,char *topic,int len ,char *payload,int plen)

{
    // MODIFY ME
    uint16_t adc_cal[] = {1680,1640};
    uint16_t rlen = 0;
    uint8_t mask = 1;
    uint16_t *adc_raw;
    iot_demo4_run();
    adc_raw = iot_get_demo4_sensor_readings();
    tfp_snprintf( topic,len, "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),"sensor_reading");
    rlen += tfp_snprintf(payload,plen,"{\"sensors\":{");
    for(int i = 0;i<MAX_ADC_CHANNELS;i++){
        if(adc_config[i].enable){
            int16_t raw = (adc_cal[i] - adc_raw[i]);
            int16_t d = 0;
            int16_t f = 0;
            raw = (raw < 0) ? 0 : raw;
#if (defined DEMO3)
#if (defined BATTERY_STATUS)
            if(adc_config[i].range == 1){
                raw = (raw * 990) / 1032;
                raw = (raw >= 990) ? 990 : raw;
              //  d = raw / 10;
             //   f = raw % 10;
            }
            else if(adc_config[i].range == 2){
                raw = (raw * 150) / 1032;
                raw = (raw >= 150) ? 150 : raw;
              //  d = raw / 10;
               // f = raw % 10;
            }
            else if(adc_config[i].range == 3){          //9
                raw = (raw * 90) / 1032;
                raw = (raw >= 90) ? 90 : raw;
                d = raw / 10;
                f = raw % 10;
            }else {
                raw = (raw * 255) / 1032;
                raw = (raw >= 255) ? 255 : raw;
               // d = raw / 10;
               // f = raw % 10;
            }
#endif
#endif

            rlen += tfp_snprintf( payload+rlen, plen - rlen, TOPIC_IND_SENSOR, "adc",i+1,raw, adc_config[i].class);
            payload[rlen++] = ',';
        }
        mask <<= 1;
    }
    rlen-=1;
    rlen += tfp_snprintf( payload+rlen, plen - rlen,"}}");
    payload[rlen] = '\0';
    return 0;
}
#endif


uint16_t iot_get_adc_devices(char *payload,uint16_t plen){
    uint16_t len = 0;
    for(uint8_t i = 0;i<MAX_ADC_CHANNELS;i++)
    {
        if(adc_config[i].enable)
        {
#if (defined DEMO1)
            len += tfp_snprintf(payload + len ,plen - len,GET_IND_PAYLOAD,"adc",i+1,DEVICE_CLASS_ANENOMOMETER,1);
#else
            len += tfp_snprintf(payload + len ,plen - len,GET_IND_PAYLOAD,"adc",i+1,adc_config[i].class,1);
#endif
            payload[len++] = ',';
        }
    }len--;

    return len;
}

void iot_modem_adc_set_voltage(int voltage)
{
    // MODIFY ME - REFER TO ADC_VOLTAGE

	if (voltage == ADC_VOLTAGE_N5_P5) {

	}
	else if (voltage == ADC_VOLTAGE_N10_P10) {

	}
	else if (voltage == ADC_VOLTAGE_0_P10) {

	}
}

void iot_modem_adc_set_properties(const char *json,uint8_t ucNumber){
    if(ucNumber == 0xff)
         ucNumber  = (uint8_t)json_parse_int( json, NUMBER_STRING ) - 1;

    if(ucNumber >= MAX_ADC_CHANNELS) return;

    uint8_t tmp   = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_CLASS );
    if(tmp == DEVICE_CLASS_POTENTIOMETER)// || tmp == DEVICE_CLASS_BATTERY || tmp == DEVICE_CLASS_ETAPE){
        adc_config[ucNumber].class = tmp;
        int len = 0;
        char* str = json_parse_str(json, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &len);
        strncpy(adc_config[ucNumber].src_device_name,str,len);
        adc_config[ucNumber].src_device_name[len] = 0;


        adc_config[ucNumber].range = (uint8_t)json_parse_int( json, "range");
        adc_config[ucNumber].alert_type = (uint8_t)json_parse_int( json, "type");
        adc_config[ucNumber].alert_period = (uint16_t)json_parse_int( json, "period");
        adc_config[ucNumber].t_val = (uint16_t)json_parse_int( json, "value");
        adc_config[ucNumber].t_min = (uint8_t)json_parse_int( json, "min");
        adc_config[ucNumber].t_max = (uint8_t)json_parse_int( json, "max");
        adc_config[ucNumber].t_activate = (uint8_t)json_parse_int( json, "activate");
        adc_config[ucNumber].mode = (uint8_t)json_parse_int( json, "mode");
        adc_config[ucNumber].enable = (uint8_t)json_parse_int( json, "enabled");
        adc_config[ucNumber].configured = 1;
        DPRINTF_WARN("Configured %d",ucNumber);
        if (adc_config[ucNumber].enable==1)
        {
            if(ucNumber == 0)
            {
                xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC0), eSetBits);
            }
            else if(ucNumber == 1){
                xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC1), eSetBits);
            }
        }
        else if (adc_config[ucNumber].enable==0){
            if(ucNumber == 0){
                xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC0), eNoAction);
            }
            else if(ucNumber == 1){
                xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_ADC1), eNoAction);
            }
        }

    }
}

void iot_adc_bootup_config( const char* payload )
{
    //DEBUG_PRINTF("Please parse the sensor configuration \n%s\n", payload);
    /*
     * {"i2c": [[{"address": 41, "class": 3, "attributes": {"range": 0, "mode": 2, "threshold":
     * {"value": 0, "min": 0, "max": 100, "activate": 0}, "alert": {"type": 1, "period": 3000},
     * "hardware": {"devicename": "MyModemDevKit1"}}}, {"address": 42, "class": 3, "attributes":
     * {"range": 0, "mode": 2, "threshold": {"value": 0, "min": 0, "max": 100, "activate": 0},
     * "alert": {"type": 1, "period": 3000}, "hardware": {"devicename": "MyModemDevKit1"}}},
     * {"address": 40, "class": 3, "attributes": {"range": 0, "mode": 2, "threshold": {"value": 0,
     * "min": 0, "max": 100, "activate": 0}, "alert": {"type": 1, "period": 3000}, "hardware":
     * {"devicename": "MyModemDevKit1"}}}], [{"address": 8, "class": 2, "attributes": {"color":
     * {"usage": 1, "single": {"endpoint": 0, "manual": 0, "hardware": {"devicename": "", "peripheral": "",
     * "sensorname": "", "attribute": ""}}, "individual": {"red": {"endpoint": 1, "manual": 0, "hardware":
     * {"devicename": "MyModemDevKit1", "peripheral": "I2C", "sensorname": "POT 11", "attribute": "Range"}},
     * "green": {"endpoint": 1, "manual": 0, "hardware": {"devicename": "MyModemDevKit2", "peripheral": "I2C",
     * "sensorname": "POT 11", "attribute": "Range"}}, "blue": {"endpoint": 1, "manual": 0, "hardware":
     * {"devicename": "MyModemDevKit1", "peripheral": "I2C", "sensorname": "POT 13", "attribute": "Range"}}}},
     * "fadeouttime": 1}}], [], []]}
     */
    char* ptr = (char*)payload;
    int len = 0;
    int array_idx = 1;
    while (ptr != NULL)
    {
        ptr = json_parse_object_array(payload, "adc", &len, array_idx);
        if (ptr != NULL)
        {
            iot_modem_adc_set_properties(ptr, array_idx-1);
        }
        array_idx++;
    }
}


int iot_modem_adc_get_devices(const char *rpayload,char *payload, int plen){
    int result = -1;
    uint8_t ucNumber  = (uint8_t)json_parse_int( rpayload, NUMBER_STRING ) - 1;
    int pplen = tfp_snprintf(payload,plen,"{\"value\":[");
    for(int i = 0; i<MAX_ADC_CHANNELS;i++)
    {
        if(adc_config[i].configured)
        {
            pplen+= tfp_snprintf(payload + pplen,plen-pplen,
                    "{\"enabled\":%d,\"class\": %d},",adc_config[i].enable,adc_config[i].class);
            result = 0;
        }
    }
    tfp_snprintf(payload + pplen - 1,plen-pplen-1,"]}");
    return result;
}





int iot_modem_adc_get_properties(const char *rpayload,char *payload, int plen)
{
    uint8_t ucNumber  = (uint8_t)json_parse_int( rpayload, NUMBER_STRING ) -1;
    if(ucNumber >= MAX_ADC_CHANNELS) return -1;
    if(!adc_config[ucNumber].configured) return -1;
    tfp_snprintf(payload,plen,
    "{\"value\":{\"mode\": %d,\"range\": %d,\"class\": %d,\"number\": %d,\
    \"hardware\":{\"devicename\": \"%s\"},\
    \"alert\":{\"type\":%d,\"period\":%d},\
    \"threshold\":{\"value\":%d,\"min\":%d,\"max\":%d,\"activate\":%d}}}",
    adc_config[ucNumber].mode,
    adc_config[ucNumber].range,
    adc_config[ucNumber].class,
    ucNumber,
    adc_config[ucNumber].src_device_name,
    adc_config[ucNumber].alert_type,
    adc_config[ucNumber].alert_period,
    adc_config[ucNumber].t_val,
    adc_config[ucNumber].t_min,
    adc_config[ucNumber].t_max,
    adc_config[ucNumber].t_activate
    );
    return 0;
}

#endif // ENABLE_ADC
