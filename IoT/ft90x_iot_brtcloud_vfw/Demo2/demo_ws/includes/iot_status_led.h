/*
 * iot_status_led.h
 *
 *  Created on: 10 Feb 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_IOT_STATUS_LED_H_
#define INCLUDES_IOT_STATUS_LED_H_
#include <ft900.h>
typedef enum{
    led_none,
    led_pos,
    led_waiting_for_eth,
    led_waiting_for_mqtt,
    led_pub,
}LEDStatus_t;


void iot_status_led_init();
void iot_status_led_task(uint32_t color,uint8_t intensity);
void iot_status_led_exit();

#endif /* INCLUDES_IOT_STATUS_LED_H_ */
