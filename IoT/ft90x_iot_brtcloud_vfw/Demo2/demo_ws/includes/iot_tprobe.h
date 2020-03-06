/*
 * tprobe.h
 *
 *  Created on: 13 Jan 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_IOT_TPROBE_H_
#define INCLUDES_IOT_TPROBE_H_

#include <stdint.h>
#include <ft900.h>
#include <debug.h>
#include <iot_timer.h>
#include <iot_uart.h>

#define TX_RX_ENABLE_PIN            61
#define TX_RX_PIN                   60
#define TX_ENABLE                   1
#define RX_ENABLE                   0

#define TPROBE_BIT                  28
#define TPROBE_TX_RX_MASK           (1 << 28)
#define T

typedef struct TProbe{
    uint16_t humidity;
    uint16_t temperature;
}TProbe_t;

void iot_tprobe_init();
void iot_tprobe_run(TProbe_t *probe);
void iot_tprobe_exit();

#endif /* INCLUDES_IOT_TPROBE_H_ */
