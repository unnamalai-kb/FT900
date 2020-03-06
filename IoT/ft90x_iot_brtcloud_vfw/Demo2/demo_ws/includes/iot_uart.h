/*
 * iot_uart.h
 *
 *  Created on: 13 Jan 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_IOT_UART_H_
#define INCLUDES_IOT_UART_H_

#define GPIO_UART0_TX   48
#define GPIO_UART0_RX   49
#define GPIO_UART1_TX   52
#define GPIO_UART1_RX   53

#include <stdint.h>

typedef enum IoTUART{
    uart_default = 0,
    uart_uart0,
    uart_uart1,
}IoTUART_e;


typedef enum IoTUART_Rx{
    uart_rx_default = 0,     // usb
    uart_rx_con,
}IoTUART_Rx_e;

typedef enum IoTUART_BaudRate{
    baud_9600 = 0,
    baud_19200,
    baud_921600,
}IoTUART_Baudrate_e;

typedef void (*uart_callback_t)(uint8_t *, uint8_t len);
void iot_uart_init(IoTUART_e uart,IoTUART_Baudrate_e br,uart_callback_t cb);
void iot_uart_select_rx(IoTUART_e uart,IoTUART_Rx_e rx);
void iot_uart_exit(IoTUART_e uart);



#endif /* INCLUDES_IOT_UART_H_ */
