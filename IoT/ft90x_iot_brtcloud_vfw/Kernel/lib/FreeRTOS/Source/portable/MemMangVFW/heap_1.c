/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#if (!defined(VFW_PORT) && defined(FT32_PORT_HEAP) && (FT32_PORT_HEAP == 1))
#warning HEAP METHOD 1 used
/*
 * The simplest possible implementation of pvPortMalloc().  Note that this
 * implementation does NOT allow allocated memory to be freed again.
 *
 * See heap_2.c, heap_3.c and heap_4.c for alternative implementations, and the
 * memory management pages of http://www.FreeRTOS.org for more information.
 */
#include <ft900.h>
#include "kernel.h"
#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/* A few bytes might be lost to byte aligning the heap start address. */
#define configADJUSTED_HEAP_SIZE	( configTOTAL_HEAP_SIZE - portBYTE_ALIGNMENT )

/* Allocate the memory for the heap. */
static uint8_t KernelHeap[ configTOTAL_HEAP_SIZE ];
static volatile uint8_t* ucHeap = KernelHeap;
static size_t xNextFreeByte[MAX_HEAP_TYPES];
static uint8_t *pucAlignedHeap[MAX_HEAP_TYPES];

#define MAX_USER_HEAP_SIZE		(20 * 1024UL)
static bool UserHeapInitialized;
static uint8_t CurHeapIdx;


/*-----------------------------------------------------------*/

void *pvPortMalloc( size_t xWantedSize )
{
void *pvReturn = NULL;


	/* Ensure that blocks are always aligned to the required number of bytes. */
	#if portBYTE_ALIGNMENT != 1
		if( xWantedSize & portBYTE_ALIGNMENT_MASK )
		{
			/* Byte alignment required. */
			xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
		}
	#endif


	vTaskSuspendAll();
	kernel_do_on_malloc_entry();

    printf("----------------\n");
    printf("xWantedSize: %u\n", xWantedSize);
    printf("xNextFreeByte: %04X\n", xNextFreeByte);
    printf("pucAlignedHeap =%08X\n", ((uint32_t)pucAlignedHeap[CurHeapIdx]));
    printf("ucHeap =%08X\n", ((uint32_t)&ucHeap[ portBYTE_ALIGNMENT ] ));

	{
		if( pucAlignedHeap[CurHeapIdx] == NULL )
		{
			/* Ensure the heap starts on a correctly aligned boundary. */
			pucAlignedHeap[CurHeapIdx] = ( uint8_t * ) ( ( ( portPOINTER_SIZE_TYPE ) &ucHeap[ portBYTE_ALIGNMENT ] ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );
		}

		/* Check there is enough room left for the allocation. */
		if( ( ( xNextFreeByte[CurHeapIdx] + xWantedSize ) < configADJUSTED_HEAP_SIZE ) &&
			( ( xNextFreeByte[CurHeapIdx] + xWantedSize ) > xNextFreeByte[CurHeapIdx] )	)/* Check for overflow. */
		{
			/* Return the next free byte then increment the index past this
			block. */
			pvReturn = pucAlignedHeap[CurHeapIdx] + xNextFreeByte[CurHeapIdx];
			xNextFreeByte[CurHeapIdx] += xWantedSize;
            
            
            printf("xNextFreeByte:  %04X\n", xNextFreeByte[CurHeapIdx]);
            printf("pvReturn =%08X\n", ((uint32_t)pvReturn));

		}

		traceMALLOC( pvReturn, xWantedSize );
	}
            printf("++++++++++++++\n");

	( void ) xTaskResumeAll();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
	}
	#endif

	return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( void *pv )
{
	/* Memory cannot be freed using this scheme.  See heap_2.c, heap_3.c and
	heap_4.c for alternative implementations, and the memory management pages of
	http://www.FreeRTOS.org for more information. */
	( void ) pv;

	/* Force an assert as it is invalid to call this function. */
	configASSERT( pv == NULL );
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
	/* Only required when static memory is not cleared. */
	xNextFreeByte[Kernel_Heap] = ( size_t ) 0;
	xPortResetUserHeap();
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
	return ( configADJUSTED_HEAP_SIZE - xNextFreeByte[Kernel_Heap]  );
}

bool xPortSwitchHeap(e_HeapTypes heap, uint32_t heapStartAdr){
	if(heap == User_Heap){
		if(!UserHeapInitialized){
			// Initialise the user heap
			xNextFreeByte[User_Heap] = 0;
			pucAlignedHeap[User_Heap] = NULL;
		}
		UserHeapInitialized = true;
		CurHeapIdx = (uint8_t)User_Heap;
		ucHeap = (uint8_t*)heapStartAdr;
		printf("switched to user heap!\n");
	}
	else if( heap == Kernel_Heap){
		CurHeapIdx = (uint8_t)Kernel_Heap;
		ucHeap = KernelHeap;
		printf("switched to kernel heap!\n");
	}
}

void xPortResetUserHeap(void)
{
	UserHeapInitialized = false;
}

#endif
