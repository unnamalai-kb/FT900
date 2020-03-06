/*
 * debug.h
 *
 *  Created on: 13 Jan 2020
 *      Author: prabhakaran.d
 */

#ifndef INCLUDES_DEBUG_H_
#define INCLUDES_DEBUG_H_



#if IOT_DEBUG_ENABLE == 1
#include <iot_uart.h>
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define CLRSCR "\033[2J"
#define TS_FLOAT()              0/*iot_get_millis()*/
#define __MODULE__              "IOT"
#define dbg_printf(fmt,...)             do { tfp_printf("%s[%lu] %s:%d %s ", KBLU, TS_FLOAT(), __MODULE__, __LINE__, __FUNCTION__); tfp_printf(fmt, ##__VA_ARGS__); tfp_printf("%s\n", KNRM); } while(0);
#define DPRINTF_ERROR(fmt, ...)         do { tfp_printf("%s[%lu] %s:%d %s ", KRED, TS_FLOAT(), __MODULE__, __LINE__, __FUNCTION__); tfp_printf(fmt, ##__VA_ARGS__); tfp_printf("%s\n", KNRM); } while(0);
#define DPRINTF_WARN(fmt, ...)          do { tfp_printf("%s[%lu] %s:%d %s ", KYEL, TS_FLOAT(), __MODULE__, __LINE__, __FUNCTION__); tfp_printf(fmt, ##__VA_ARGS__); tfp_printf("%s\n", KNRM); } while(0);
#define DPRINTF_INFO(fmt, ...)          do { tfp_printf("%s[%lu] %s:%d %s ", KGRN, TS_FLOAT(), __MODULE__, __LINE__, __FUNCTION__); tfp_printf(fmt, ##__VA_ARGS__); tfp_printf("%s\n", KNRM); } while(0);
#else
#define DPRINTF_ERROR(fmt, ...)
#define DPRINTF_WARN(fmt, ...)
#define DPRINTF_INFO(fmt, ...)
#define dbg_printf(s,...)
#endif
#endif /* INCLUDES_DEBUG_H_ */
