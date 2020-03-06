/*
 * iot_timer.h
 *
 *  Created on: 13 Jan 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_IOT_TIMER_H_
#define INCLUDES_IOT_TIMER_H_

#include <ft900.h>
#include <stdint.h>

#define iot_millis_init() iot_timer_init()

void iot_timer_init();

uint32_t iot_get_millis();

#endif /* INCLUDES_IOT_TIMER_H_ */
