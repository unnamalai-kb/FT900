#ifndef LAUNCHER_H_
#define LAUNCHER_H_

#include <ft900.h>
#include "tinyprintf.h"
#include "pff.h"

#define DEMO_MODEM_APPLICATION		1 /* 0 - Generic Launcher, 1 - DEMO application */
#define MAX_APP_NAME_SIZE_B						40u

#define TASK_NOTIFICATION_LINE_RXED            0x01

typedef struct{
uint32_t FileFormat;
uint32_t TargetKernelApi;
uint8_t Name[MAX_APP_NAME_SIZE_B];
uint32_t Version;
uint32_t Hash;
uint32_t EntryPoint;
uint32_t HeapStart;
}s_AppProperties;

#define PETIT_FF	1
#define LOADER_MIN_SIZE 1  /*1 - to disable entry and error string and timestamping */

void launcher_init(void);
#if VFW_2019
bool load_app_to_pm(const char* objPath, FIL* fapp);
s_AppProperties* ld_get_app_info(void);
#else
#if PETIT_FF
typedef FATFS FIL;
#endif
bool ldr_load_app_to_pm(const char* objPath, FIL* fapp, int load_data);
s_AppProperties* ldr_get_app_info(void);
#define load_app_to_pm ldr_load_app_to_pm
#define ld_get_app_info ldr_get_app_info
#endif
//void mem_set (void* dst, int val, int cnt);
void launcher_hw_init(void);

#endif
