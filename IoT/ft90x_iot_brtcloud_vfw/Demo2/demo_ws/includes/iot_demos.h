/*
 * iot_demo_ws.h
 *
 *  Created on: 14 Jan 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_IOT_DEMOS_H_
#define INCLUDES_IOT_DEMOS_H_

#include <iot_tprobe.h>
#include <iot_ow.h>
#include <iot_uart.h>
#include <iot_timer.h>
#include <iot_adc.h>

#define IOT_PROBE_ERROR     -1
#define IOT_OW_ERROR        -2
#define IOT_UNKNOWN_ERROR   -3

typedef struct Anemometer{
    int32_t raw;
    uint32_t speed;
}Anemometer_t;

typedef struct BatteryLevel{
    int32_t raw;
    uint32_t level;
}BatteryLevel_t;

typedef struct LiquidLevel{
    uint16_t raw;
    uint16_t level;
}LiquidLevel_t;

typedef struct EWDemoSensors{
    TProbe_t probe;
    uint16_t temperature;
    Anemometer_t wind;
    LiquidLevel_t liquid_level;
    LiquidLevel_t battery_level;
}EWDemoSensors_t;


typedef struct WeatherStation_Cfg{
    ADCIn adc_in_selection;
}EWDemoSensors_Cfg_t;


void iot_ws_init(EWDemoSensors_Cfg_t *ws);
int iot_ws_run(EWDemoSensors_t *);
void iot_ws_exit();
void iot_demo4_init(EWDemoSensors_Cfg_t *ws);
int iot_demo4_run(void);
uint16_t *iot_get_demo4_sensor_readings(void);
void iot_demo4_exit();
#endif /* INCLUDES_IOT_DEMOS_H_ */
