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
#include <debug.h>


#include <string.h>
#include <stdlib.h>

#if (defined DEMO2)

#define MAX_I2C_DEV_PER_SLOT            2
#define MAX_I2C_SLOTS                   1
#define MAX_I2C_DEV                     (1)
#define MAX_LEDS                        1




typedef struct Demo2PotConfig{
    // hardware
    uint8_t slot;
    uint8_t address;
    uint8_t class;
    uint8_t range;
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
}Demo2PotConfig_t;


typedef struct LEDhardware{
    uint8_t endpoint;
    char src_device_name[32];
    char sensorname[16];
    char peripheral[8];
    char attribute[8];
    uint32_t value;
    uint8_t manual;
}LEDhardware_t;

typedef struct Demo2LEDConfig{
    // hardware
    uint8_t slot;
    uint8_t address;
    uint8_t class;
    uint16_t fadeouttime;
    uint8_t usage;
    uint8_t enable;
    LEDhardware_t colors[3];
}Demo2LEDConfig_t;

static Demo2PotConfig_t demo2potconfig[MAX_I2C_DEV];
static Demo2LEDConfig_t demo2LEDconfig[MAX_LEDS];
static uint8_t demo2potconfig_cts = 0;
#endif

extern TaskHandle_t g_iot_app_handle; // used by iot_modem_gpio_process()

#if ENABLE_I2C

uint8_t get_device_index(uint8_t ucNumber,uint8_t address){
    uint8_t index = 0xff;
    for(int i = 0;i<MAX_I2C_DEV;i++)
    {
        if(demo2potconfig[i].slot == ucNumber)
        {
            if(demo2potconfig[i].address == address){
                index = i;
                break;
            }
        }
    }
    return index;
}


static uint8_t get_slot_devices(uint8_t ucNumber){
    uint8_t index = 0xff;
    for(int i = 0;i<MAX_I2C_DEV;i++)
    {
        if(demo2potconfig[i].slot == ucNumber)
        {
            index = i;
            break;
        }
    }
    return index;
}

void led_off()
{
    uint8_t val[4] = {0,0,0};
    iot_led_write(demo2LEDconfig[0].slot,demo2LEDconfig[0].address,1,val);
}
#if !UNUSED_DEMO_FUNCTIONS
void iot_modem_i2c_init()
{
    // MODIFY ME
    memset(demo2potconfig,0,sizeof(demo2potconfig));
    iot_i2c_init();
}
#endif

int iot_modem_i2c_enable(const char *json,uint8_t ucNumber)
{
    if(ucNumber == 0xff)
        ucNumber  = (uint8_t)json_parse_int( json, NUMBER_STRING ) - 1;
    uint8_t ucAddress = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_ADDRESS );
    int8_t ucEnabled = (uint8_t)json_parse_int( json, ENABLE_STRING );
    if(ucEnabled < 0) return 0;

    if(demo2LEDconfig[0].slot == ucNumber){
        if(demo2LEDconfig[0].address == ucAddress)
        {
            demo2LEDconfig[0].enable = ucEnabled;
            if(!ucEnabled) led_off();
            DPRINTF_INFO("LF");
            return 0;
        }
    }

    uint8_t pot = 0;

    for(int i = 0;i<MAX_I2C_DEV;i++){
        if(demo2potconfig[i].slot == ucNumber){
            if(demo2potconfig[i].address == ucAddress)
            {
                demo2potconfig[i].enable = ucEnabled;
                DPRINTF_INFO("PF");
                pot = 1;
                break;
            }
        }
    }
    if(pot){
        uint8_t enable = 0;
        for(int i = 0;i<MAX_I2C_DEV;i++){
            if(demo2potconfig[i].enable == 1)
            {
                enable = 1;
                break;
            }
        }
        if(enable == 0){
            xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_I2C0), eNoAction);
        }else
        {
            xTaskNotify( g_iot_app_handle, TASK_NOTIFY_BIT(TASK_NOTIFY_BIT_I2C0), eSetBits);
        }
    }



    return 1;
}

void iot_modem_i2c_set_properties(const char *json,uint8_t ucNumber)
{
    DPRINTF_ERROR("%s",json);
    uint8_t ucEP  = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_ENDPOINT );
    if(ucNumber == 0xff)
     ucNumber  = (uint8_t)json_parse_int( json, NUMBER_STRING ) - 1;

    uint8_t ucAddress = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_ADDRESS );
    uint8_t ucClass   = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_CLASS );
    uint8_t ucFormat   = (uint8_t)json_parse_int( json, DEVICE_DIGI2_FORMAT );
    uint8_t ucBrightness = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_BRIGHTNESS );
    DPRINTF_INFO( "S %d A=%d C=%d S %s\r\n", ucNumber, ucAddress, ucClass,json);
    DPRINTF_INFO( "ucEP %d ucFormat=%d b=%d\r\n", ucEP, ucFormat,ucBrightness);

    switch (ucClass)
    {
		case DEVICE_CLASS_POTENTIOMETER:
		{
			/*
			 * {"endpoint": 1, "hardware": {"devicename": "Demo4 Test", "peripheral": "ADC", "sensorname": "ADC Potentiometer 1",
			 *  "attribute": "Range"},
			 * "brightness": 255, "format": 0, "text": "23", "address": 20, "class": 1, "number": 1}
			 */

			uint8_t index = 0xff;
			for(int i = 0;i<demo2potconfig_cts;i++){
				if(demo2potconfig[i].address == ucAddress){
					index = i;
					break;
				}
			}
			if(index == 0xff){
				if(demo2potconfig_cts  < MAX_I2C_DEV)
					index = demo2potconfig_cts++;
			}
			int len = 0;
			if(index == 0xff){
				DPRINTF_ERROR("Conf. Alrdy");
				return;
			}

			char* str = json_parse_str(json, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &len);
			strncpy(demo2potconfig[index].src_device_name,str,len);
			demo2potconfig[index].src_device_name[len] = 0;




			demo2potconfig[index].slot = ucNumber;
			demo2potconfig[index].address = ucAddress;
			demo2potconfig[index].class = ucClass;
			demo2potconfig[index].range = (uint8_t)json_parse_int( json, "range");
			demo2potconfig[index].mode = (uint8_t)json_parse_int( json, "mode");

			demo2potconfig[index].t_val = (uint16_t)json_parse_int( json, "value");
			demo2potconfig[index].t_min = (uint8_t)json_parse_int( json, "min");
			demo2potconfig[index].t_max = (uint8_t)json_parse_int( json, "max");
			demo2potconfig[index].t_activate = (uint8_t)json_parse_int( json, "activate");
			demo2potconfig[index].alert_type = (uint8_t)json_parse_int( json, "type");
			demo2potconfig[index].alert_period = (uint16_t)json_parse_int( json, "period");
			//demo2potconfig[index].enable = (uint8_t)json_parse_int( json, "enabled");

			iot_modem_i2c_enable(json,ucNumber);
			demo2potconfig[index].configured = 1;
			DPRINTF_WARN("Conf %d",ucNumber);
			DPRINTF_INFO("@ %d",index);
		}
		break;
		case DEVICE_CLASS_LIGHT:
		{
			int len = 0;
			iot_modem_i2c_enable(json,ucNumber);
			demo2LEDconfig[0].usage = (uint8_t)json_parse_int( json, "usage");
			demo2LEDconfig[0].address = (uint8_t)json_parse_int( json, "address");
			demo2LEDconfig[0].slot = ucNumber;
			demo2LEDconfig[0].fadeouttime = (uint8_t)json_parse_int( json, "fadeouttime");
			iot_modem_i2c_enable(json,ucNumber);
			if(demo2LEDconfig[0].usage == 1)
			{
				DPRINTF_WARN("Ind");
				uint8_t val[3];
				bool new_val =false;
				char *comp[] = {"red","green","blue"};
				char *json_str_colour[3];
				json_str_colour[0] = json_parse_str(json, "red", &len);
				json_str_colour[1] = json_parse_str(json, "green", &len);
				json_str_colour[2] = json_parse_str(json, "blue", &len);
				char *ind = json_parse_str(json, "individual", &len);
				for(int i = 0;i<3;i++)
				{
					DEBUG_PRINTF("%d",i);
					char *cmp = json_parse_str(json, comp[i], &len);
					demo2LEDconfig[0].colors[i].endpoint = (uint8_t)json_parse_int( cmp, "endpoint");
					char *hardware = json_parse_str(cmp, "hardware", &len);




					/*
					if(i==0){
						if(hardware > green){
							DPRINTF_ERROR(">> %d hardw %lu %lu %lu",i,hardware,red,green,blue);
							hardware = NULL;
						}
					}else if(i == 1){
						if(hardware > blue ){
							DPRINTF_ERROR(">> %d hardw %lu %lu %lu",i,hardware,red,green,blue);
							hardware = NULL;
						}
					}*/
					if(hardware == NULL)
					{
						//Manual control
						int32_t manual = (uint32_t)json_parse_int( json_str_colour[i], "manual");
						if(manual >= 0)
						{
							DPRINTF_INFO("Manual %lu",manual);
							val[i] = manual;
							demo2LEDconfig[0].colors[i].manual = 1;
							demo2LEDconfig[0].colors[i].value = manual;
							demo2LEDconfig[0].colors[i].src_device_name[0] = 0;
							demo2LEDconfig[0].colors[i].sensorname[0] = 0;
							demo2LEDconfig[0].colors[i].peripheral[0] = 0;
							demo2LEDconfig[0].colors[i].attribute[0] = 0;
							new_val = true;
						}
						else
						{
							return;
						}
					}
					else
					{
						// Hardware control
						ind = hardware;
						if(ind == NULL) return;
						demo2LEDconfig[0].colors[i].manual = 0;
						char *str = json_parse_str(ind, "devicename", &len);
						strncpy(&demo2LEDconfig[0].colors[i].src_device_name[0],str,len);
						demo2LEDconfig[0].colors[i].src_device_name[len] = 0;

						str = json_parse_str(ind, "sensorname", &len);
						strncpy(&demo2LEDconfig[0].colors[i].sensorname[0],str,len);
						demo2LEDconfig[0].colors[i].sensorname[len] = 0;

						str = json_parse_str(ind, "peripheral", &len);
						strncpy(demo2LEDconfig[0].colors[i].peripheral,str,len);
						demo2LEDconfig[0].colors[i].peripheral[len] = 0;

						str = json_parse_str(ind, "\"attribute\"", &len)-1;

						int l = 0;
						while(*str != '"'){
							demo2LEDconfig[0].colors[i].attribute[l] = *str++;
							l++;
						}
						demo2LEDconfig[0].colors[i].attribute[l] = 0;
						////val[i] = manual;
						////demo2LEDconfig[0].colors[i].value = manual;
						DPRINTF_INFO("%d %s",i,demo2LEDconfig[0].colors[i].sensorname);
					}
					DPRINTF_INFO("New val %d",new_val);
				}
				if(new_val)
				{
					if(demo2LEDconfig[0].enable){
						iot_led_write(demo2LEDconfig[0].slot, demo2LEDconfig[0].address, demo2LEDconfig[0].fadeouttime, val);
					}
				}
			}
			else if(demo2LEDconfig[0].usage == 0)
			{
				char *ind = json_parse_str(json, "single", &len);
				int32_t manual = (uint32_t)json_parse_int( json, "manual");
				if(manual >= 0)
				{
					DPRINTF_INFO("Manual %lu",manual);
					if(demo2LEDconfig[0].enable)
					{
						uint8_t val[3];
						val[0] = (manual >> 16 ) & 0xff;
						val[1] = (manual >> 8 ) & 0xff;
						val[2] = (manual >> 0 ) & 0xff;
						iot_led_write(demo2LEDconfig[0].slot,demo2LEDconfig[0].address,1,val);
					}
				}
				else
				{

					ind = json_parse_str(json, "hardware", &len);
					if(ind == NULL) return;

					demo2LEDconfig[0].colors[0].endpoint = (uint8_t)json_parse_int( json, "endpoint");
					char *str = json_parse_str(ind, "devicename", &len);
					strncpy(demo2LEDconfig[0].colors[0].src_device_name,str,len);
					demo2LEDconfig[0].colors[0].src_device_name[len] = 0;

					str = json_parse_str(ind, "sensorname", &len);
					strncpy(demo2LEDconfig[0].colors[0].sensorname,str,len);
					demo2LEDconfig[0].colors[0].sensorname[len] = 0;

					str = json_parse_str(ind, "peripheral", &len);
					strncpy(demo2LEDconfig[0].colors[0].peripheral,str,len);
					demo2LEDconfig[0].colors[0].peripheral[len] = 0;

					str = json_parse_str(ind, "\"attribute\"", &len)-1;
					int l = 0;
					while(*str != '"')
					{
						demo2LEDconfig[0].colors[0].attribute[l] = *str++;
						l++;
					}
					demo2LEDconfig[0].colors[l].attribute[l] = 0;
				}
			}
			DPRINTF_WARN("LED %d ",ucNumber);
		}
		break;
		case DEVICE_CLASS_TEMPERATURE:
		{

		}
		break;

    }
}



void iot_modem_demo3_update(const char* json){


#if 0
    int idevLen = 0;
    int isensorLen = 0;
    int i = 0;
    if(demo2potconfig_cts <= 0) return;
    char *device_name = json_parse_str(json, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &idevLen);
    char *sensor_name = json_parse_str(json, DEVICE_PROPERTIES_HARDWARE_SENSORNAME, &isensorLen);

    for(i = 0;i<demo2potconfig_cts;i++){
        DPRINTF_WARN("<<<CONFIG_INFO %d>>>",i);
        DPRINTF_INFO("DN: %s",demo2potconfig[i].src_device_name);
        DPRINTF_INFO("SN: %s",demo2potconfig[i].sensorname);
        DPRINTF_INFO("C: %d",demo2potconfig[i].class);
        DPRINTF_INFO("A: %d",demo2potconfig[i].address);
        DPRINTF_INFO("F: %d",demo2potconfig[i].format);
        DPRINTF_INFO("V: %d",demo2potconfig[i].val);
        if(!strncmp(device_name,demo2potconfig[i].src_device_name,idevLen)){
            if(!strncmp(sensor_name,demo2potconfig[i].sensorname,isensorLen)){
                demo2potconfig[i].val = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_THRESHOLD_VALUE);
                if(demo2potconfig[i].enable){
                    iot_digi2_write(demo2potconfig[i].slot,
                                    demo2potconfig[i].address,
                                    demo2potconfig[i].format,
                                    demo2potconfig[i].brightness,
                                    demo2potconfig[i].val);
                }else{
                    DPRINTF_ERROR("Dev Disabled");
                }
            }
        }
    }
#else
    int idevLen = 0;
    int isensorLen = 0;
    uint8_t val[3];
    if(demo2LEDconfig[0].enable==0){
        DPRINTF_ERROR("LED Dis.")
        return;
    }
    char *device_name = json_parse_str(json, DEVICE_PROPERTIES_HARDWARE_DEVICENAME, &idevLen);
    char *sensor_name = json_parse_str(json, DEVICE_PROPERTIES_HARDWARE_SENSORNAME, &isensorLen);


    val[0] = demo2LEDconfig[0].colors[0].value;
    val[1] = demo2LEDconfig[0].colors[1].value;
    val[2] = demo2LEDconfig[0].colors[2].value;


    if(demo2LEDconfig[0].usage==1){
        for(int i = 0;i<3;i++){
            if(!strncmp(device_name,demo2LEDconfig[0].colors[i].src_device_name,idevLen)){
                if(!strncmp(sensor_name,demo2LEDconfig[0].colors[i].sensorname,isensorLen)){
                    val[i] = demo2LEDconfig[0].colors[i].value = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_THRESHOLD_VALUE);
                    DPRINTF_INFO("%d",demo2LEDconfig[0].colors[i].value);
                }
            }
        }

        /*val[0] = demo2LEDconfig[0].colors[0].value;
        val[1] = demo2LEDconfig[0].colors[1].value;
        val[2] = demo2LEDconfig[0].colors[2].value;*/
        DPRINTF_INFO("IC");
    }else{
        if(!strncmp(device_name,demo2LEDconfig[0].colors[0].src_device_name,idevLen)){
            if(!strncmp(sensor_name,demo2LEDconfig[0].colors[0].sensorname,isensorLen)){
                demo2LEDconfig[0].colors[0].value = (uint8_t)json_parse_int( json, DEVICE_PROPERTIES_THRESHOLD_VALUE);
            }
        }
        DPRINTF_INFO("SIN");
        val[1]=val[2]= val[0] = demo2LEDconfig[0].colors[0].value;
    }
    iot_led_write(demo2LEDconfig[0].slot,demo2LEDconfig[0].address,demo2LEDconfig[0].fadeouttime,val);
#endif
}


void iot_i2c_bootup_config( const char* payload )
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
    int obj_idx = 1;
    int max_arr_idx = 4;
    for (int arr_idx = 1; arr_idx<=max_arr_idx; arr_idx++)
    {
        ptr = (char*)payload;
        obj_idx = 1;
        while (ptr != NULL)
        {
            ptr = json_parse_object_multiarray(payload, "i2c", &len, arr_idx, obj_idx);
            if (ptr)
            {
               iot_modem_i2c_set_properties(ptr,arr_idx-1);
            }
            obj_idx++;
        }
    }
}


int iot_modem_i2c_get_properties(const char *rpayload,char *payload, int plen)
{
    uint8_t ucNumber  = (uint8_t)json_parse_int( rpayload, NUMBER_STRING ) -1;
    uint8_t ucAddress  = (uint8_t)json_parse_int( rpayload, DEVICE_PROPERTIES_ADDRESS );
    uint8_t index = get_device_index(ucNumber,ucAddress);
    uint8_t led = 0;
    int len = 0;
    if(index == 0xff){
        DPRINTF_ERROR("No %d",ucNumber);
        if(demo2LEDconfig[0].slot == ucNumber && demo2LEDconfig[0].address == ucAddress){
            DPRINTF_WARN("No %d",ucNumber);
            led = 1;
        }else{
            return -1;
        }
    }
    if(!led){
        tfp_snprintf(payload+ len,plen,
        "{\"value\":{\"range\": %d, \"mode\": %d,\
        \"threshold\": { \"value\": %d,\"min\": %d,\"max\": %d,\"activate\": %d},\
        \"alert\": { \"type\": %d,\"period\": %d},\
        \"hardware\": {\"devicename\": \"%s\"}}}",
        demo2potconfig[index].range,
        demo2potconfig[index].mode,
        demo2potconfig[index].t_val,
        demo2potconfig[index].t_min,
        demo2potconfig[index].t_max,
        demo2potconfig[index].t_activate,
        demo2potconfig[index].alert_type,
        demo2potconfig[index].alert_period,
        demo2potconfig[index].src_device_name
        );
    }else{
        int len = tfp_snprintf(payload+ len,plen,
               "{\"value\":{\"fadeouttime\": %d,\"color\":{\"usage\":%d,",demo2LEDconfig[0].fadeouttime,demo2LEDconfig[0].usage);
        if(demo2LEDconfig[0].usage == 1){
            char *comp[] = {"red","green","blue"};
            len += tfp_snprintf(payload+ len,plen-len,"\"individual\":{");
            for(int i = 0;i<3;i++){
                if(demo2LEDconfig[0].colors[i].manual){
                    len += tfp_snprintf(payload + len,plen-len,"\"%s\":{\"endpoint\":%d,\"manual\":%d},",
                                             comp[i],
                                             demo2LEDconfig[0].colors[i].endpoint,
                                             demo2LEDconfig[0].colors[i].value);
                }else{
                    len += tfp_snprintf(payload + len,plen-len,"\"%s\":{\"endpoint\":%d,\
                            \"hardware\":{\"devicename\":\"%s\",\"sensorname\":\"%s\",\"peripheral\":\"%s\",\"attribute\":\"%s\"}},",
                            comp[i],
                            demo2LEDconfig[0].colors[i].endpoint,
                            demo2LEDconfig[0].colors[i].src_device_name,
                            demo2LEDconfig[0].colors[i].sensorname,
                            demo2LEDconfig[0].colors[i].peripheral,
                            demo2LEDconfig[0].colors[i].attribute);
                }
            }
            len--;
            len += tfp_snprintf(payload + len,plen-len,"}}}}");
        }else{
            len += tfp_snprintf(payload + len,plen-len,"\"single\":{");
            if(demo2LEDconfig[0].colors[0].manual)
            {

            }
            else{
                len += tfp_snprintf(payload + len,plen-len,"\"endpoint\":%d,\
                        \"hardware\":{\"devicename\":\"%s\",\"sensorname\":\"%s\",\"peripheral\":\"%s\",\"attribute\":\"%s\"}}}}}",
                        demo2LEDconfig[0].colors[0].endpoint,
                        demo2LEDconfig[0].colors[0].src_device_name,
                        demo2LEDconfig[0].colors[0].sensorname,
                        demo2LEDconfig[0].colors[0].peripheral,
                        demo2LEDconfig[0].colors[0].attribute);
            }
        }

    }
    return 0;
}
uint16_t iot_get_all_i2c_devices(char *payload,uint16_t plen){
    uint16_t len = 0;
    uint8_t index = 0xff;
    for(uint8_t i = 0;i<4;i++){
        index = get_slot_devices(i);
        if(index != 0xff){
            break;
        }
    }

    if(index == 0xff) return 0;

    len += tfp_snprintf(payload + len, plen-len,"\"i2c\":[");
    for(uint8_t i = 0;i<MAX_I2C_DEV;i++)
    {
        if(demo2potconfig[i].enable)
        {
            len += tfp_snprintf(payload + len,plen-len,
                         "{\"enabled\":%d,\"address\":%d,\"class\": %d},",demo2potconfig[i].enable,demo2potconfig[i].address,demo2potconfig[i].class);
        }
    }
    payload[len-1] = ']';
    return len;
}


int iot_modem_i2c_get_devices(const char *rpayload,char *payload, int plen){
    uint8_t ucNumber  = (uint8_t)json_parse_int( rpayload, NUMBER_STRING ) - 1;
    uint8_t index = get_slot_devices(ucNumber);
    uint8_t led = 0;
    //"{\"value\":[{\"%s\":%d,\"%s\":%d,\"%s\":%d}]}"
    if(index == 0xff){
        DPRINTF_ERROR("No %d Slots",ucNumber);
        if(demo2LEDconfig[0].slot == ucNumber){
            led = 1;
        }else{
            return -1;
        }
    }
    int pplen = tfp_snprintf(payload,plen,"{\"value\":[");
    uint8_t e = 0;
    if(!led){
        for(int i = 0; i<demo2potconfig_cts;i++){
            if(demo2potconfig[i].slot == ucNumber)
            {
                pplen+= tfp_snprintf(payload + pplen,plen-pplen,
                        "{\"enabled\":%d,\"address\":%d,\"class\": %d},",demo2potconfig[i].enable,demo2potconfig[i].address,demo2potconfig[i].class);
                e = 1;
            }
        }
    }else{
        pplen+= tfp_snprintf(payload + pplen,plen-pplen,
                        "{\"enabled\":%d,\"address\":%d,\"class\": %d}",demo2LEDconfig[0].enable,demo2LEDconfig[0].address,demo2LEDconfig[0].class);
    }
/*
    for(int i = 0; i<1;i++)
    {
        if(demo2LEDconfig[i].slot == ucNumber)
        {
            pplen+= tfp_snprintf(payload + pplen,plen-pplen,
                    "{\"enabled\":%d,\"address\":%d,\"class\": %d},",demo2LEDconfig[i].enable,demo2LEDconfig[i].address,demo2LEDconfig[i].class);
            e = 1;
        }
    }
*/
    if(e){
        pplen--;
    }
    tfp_snprintf(payload + pplen,plen-pplen,"]}");
    return 0;
}


uint32_t iot_modem_i2c_get_sensor_reading(DEVICE_PROPERTIES* properties,char *topic,int len ,char *payload,int plen)
{
    // MODIFY ME
    uint16_t rlen = 0;
    tfp_snprintf( topic,len, "%s%s/%s", PREPEND_REPLY_TOPIC,(char*)iot_utils_getdeviceid(),"sensor_reading");
    rlen += tfp_snprintf(payload,plen,"{\"sensors\":{");
    for(int i = 0;i<4;i++)
    {
        if(demo2potconfig[i].slot == i){
            rlen += tfp_snprintf(payload+rlen,plen - rlen,"\"i2c%d\":[",i+1);
            break;
        }
    }
    uint8_t e = 0;
    for(int i = 0;i<MAX_I2C_DEV;i++)
    {
        if(demo2potconfig[i].enable == 1)
        {
            uint8_t range[] = {255,99,15,9};
            uint8_t val = 0;
            iot_pot_read(demo2potconfig[i].slot,
                    demo2potconfig[i].address,
                    range[demo2potconfig[i].range],
                    &val);
            rlen += tfp_snprintf( payload+rlen, plen - rlen, "{\"class\": %d,\"value\":%d,\"address\":%d},", demo2potconfig[i].class,val,demo2potconfig[i].address);
            e = 1;
        }
    }
    if(e){
        payload[rlen-1] = ']';
    }
    rlen += tfp_snprintf( payload+rlen, plen - rlen,"}}");
    payload[rlen++] = '\0';
    return 0;
}



uint16_t iot_get_i2c_devices(char *payload,uint16_t plen){
    uint16_t len = 0;
    len += tfp_snprintf(payload,plen,"{\"value\": {");
    uint8_t e = 0;
    for(int i = 0;i<4;i++)
        {
            if(demo2potconfig[i].slot == i){
                len += tfp_snprintf(payload+len,plen - len,"\"i2c%d\":[",i+1);
                e = 1;
                break;
            }
        }

    for(uint8_t i = 0;i<MAX_I2C_DEV;i++)
    {
        if(demo2potconfig[i].enable)
        {
            len += tfp_snprintf(payload + len ,plen - len,"{\"class\": %d,\"enabled\":%d,\"address\":%d},",demo2potconfig[i].class,demo2potconfig[i].enable,
                    demo2potconfig[i].address);
            e = 3;
        }
    }
    if(e == 3)
        len--;
    if(e)
        len += tfp_snprintf(payload + len ,plen - len,"]}}");
    else
        len += tfp_snprintf(payload + len ,plen - len,"}}");

    return len;
}

#endif // ENABLE_I2C
