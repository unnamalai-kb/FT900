/*
 * mem_access.h
 *
 *  Created on: Aug 14, 2015
 *      Author: govind.mukundan
 */

#ifndef MEM_ACCESS_H_
#define MEM_ACCESS_H_


#define PMCONTROL_UNLOCK ((volatile uint32_t *)0x1fc80)
#define PMCONTROL_ADDR	((volatile uint32_t *)0x1fc84)
#define PMCONTROL_DATA	((volatile uint32_t *)0x1fc88)


void memcpydat2pm(uint32_t dst, const void *src, size_t s);
void memcpypm2dat(uint32_t* dst, const void* src, size_t s);
void mem_unlock_pm(void);
void mem_lock_pm(void);

#endif /* MEM_ACCESS_H_ */
