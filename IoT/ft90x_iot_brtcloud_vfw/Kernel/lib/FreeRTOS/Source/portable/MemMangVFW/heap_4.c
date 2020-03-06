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

#if (defined(VFW_PORT) && defined(FT32_PORT_HEAP) && (FT32_PORT_HEAP == 4))
#warning HEAP METHOD 4 used

#define VFW_HEAP_DEBUG	0
/*
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of http://www.FreeRTOS.org for more information.
 */
#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include <ft900.h>
#include "FreeRTOS.h"
#include "task.h"
#include "kernel.h"
#include "tinyprintf.h"


/* FreeRTOS default is USE_PROTECTION_SCHEME == 1, which does not allow malloc() and free() to be supported in ISR. */
/* For iHome, we should use either USE_PROTECTION_SCHEME 2 or 3. */
/* 0: None, 1: vTaskSuspendAll(), 2: taskENTER_CRITICAL(), 3: portIRQ_SET_MASK_ALL(). */
#define USE_PROTECTION_SCHEME           1

#if USE_PROTECTION_SCHEME == 1
    #define CRITICAL_SECTION_START      vTaskSuspendAll
    #define CRITICAL_SECTION_FINISH        xTaskResumeAll
#elif USE_PROTECTION_SCHEME == 2
    #define CRITICAL_SECTION_START      /*vPortEnterCriticalCheckISR*/taskENTER_CRITICAL
    #define CRITICAL_SECTION_FINISH        /*vPortExitCriticalCheckISR*/taskEXIT_CRITICAL
#elif USE_PROTECTION_SCHEME == 3
    #define CRITICAL_SECTION_START      portIRQ_SET_MASK_ALL
    #define CRITICAL_SECTION_FINISH        portIRQ_RESTORE_MASK_All
#else
    #define CRITICAL_SECTION_START()
    #define CRITICAL_SECTION_FINISH()
#endif

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE	( ( size_t ) ( xHeapStructSize * 2 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE		( ( size_t ) 8 )


/* Allocate the memory for the heap. */
static uint8_t KernelHeap[ configTOTAL_HEAP_SIZE ];
static volatile uint8_t* ucHeap = KernelHeap;
static BaseType_t xHeapHasBeenInitialised[MAX_HEAP_TYPES];
static uint8_t CurHeapIdx;
static uint32_t MaxHeapSize[MAX_HEAP_TYPES]; // max heap size for application and kernel
static uint32_t HeapEnd[MAX_HEAP_TYPES];
static uint32_t UserHeapStart = FT32_SRAM_END_ADDRESS; // Keeps track of user heap start address.
// Useful when you are freeing memory - you can know if you should free from user or application heap

/* Allocate the memory for the heap. */
//#if( configAPPLICATION_ALLOCATED_HEAP == 1 )
//	/* The application writer has already defined the array used for the RTOS
//	heap - probably so it can be placed in a special segment or address. */
//	extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
//#else
//	static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
//#endif /* configAPPLICATION_ALLOCATED_HEAP */

/* Define the linked list structure.  This is used to link free blocks in order
of their memory address. */
typedef struct A_BLOCK_LINK
{
	struct A_BLOCK_LINK *pxNextFreeBlock;	/*<< The next free block in the list. */
	size_t xBlockSize;						/*<< The size of the free block. */
} BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert );

/*
 * Called automatically to setup the required heap structures the first time
 * pvPortMalloc() is called.
 */
static void prvHeapInit( void );

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const size_t xHeapStructSize	= ( ( sizeof( BlockLink_t ) + ( ( ( size_t ) portBYTE_ALIGNMENT_MASK ) - ( size_t ) 1 ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK ) );

/* Create a couple of list links to mark the start and end of the list. */
static BlockLink_t xStart[MAX_HEAP_TYPES], *pxEnd[MAX_HEAP_TYPES];

//static BlockLink_t xStart, *pxEnd = NULL;

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t xFreeBytesRemaining[MAX_HEAP_TYPES];
static size_t xMinimumEverFreeBytesRemaining[MAX_HEAP_TYPES];

/* Gets set to the top bit of an size_t type.  When this bit in the xBlockSize
member of an BlockLink_t structure is set then the block belongs to the
application.  When the bit is free the block is still part of the free heap
space. */
static size_t xBlockAllocatedBit = 0;

/*-----------------------------------------------------------*/

void *pvPortMalloc( size_t xWantedSize )
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
void *pvReturn = NULL;

	//vTaskSuspendAll();
	CRITICAL_SECTION_START();
	kernel_do_on_malloc_entry();
	{
		/* If this is the first call to malloc then the heap will require
		initialisation to setup the list of free blocks. */
		if( pxEnd[CurHeapIdx] == NULL )
		{
			prvHeapInit();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* Check the requested block size is not so large that the top bit is
		set.  The top bit of the block size member of the BlockLink_t structure
		is used to determine who owns the block - the application or the
		kernel, so it must be free. */
		if( ( xWantedSize & xBlockAllocatedBit ) == 0 )
		{
			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			if( xWantedSize > 0 )
			{
				xWantedSize += xHeapStructSize;

				/* Ensure that blocks are always aligned to the required number
				of bytes. */
				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
				{
					/* Byte alignment required. */
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
					configASSERT( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) == 0 );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
#if (VFW_HEAP_DEBUG == 1)
		    tfp_printf("----------------\n");
		    tfp_printf("xWantedSize: %u\n", xWantedSize);
		    tfp_printf("ucHeap =%08lX\n", ((uint32_t)&ucHeap[ portBYTE_ALIGNMENT ] ));
#endif

			if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining[CurHeapIdx] ) )
			{
				/* Traverse the list from the start	(lowest address) block until
				one	of adequate size is found. */
				pxPreviousBlock = &xStart[CurHeapIdx];
				pxBlock = xStart[CurHeapIdx].pxNextFreeBlock;
				while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

				/* If the end marker was reached then a block of adequate size
				was	not found. */
				if( pxBlock != pxEnd[CurHeapIdx] )
				{
					/* Return the memory space pointed to - jumping over the
					BlockLink_t structure at its start. */
					pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

					/* This block is being returned for use so must be taken out
					of the list of free blocks. */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* If the block is larger than required it can be split into
					two. */
					if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
					{
						/* This block is to be split into two.  Create a new
						block following the number of bytes requested. The void
						cast is used to prevent byte alignment warnings from the
						compiler. */
						pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
						configASSERT( ( ( ( uint32_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );

						/* Calculate the sizes of two blocks split from the
						single block. */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

						/* Insert the new block into the list of free blocks. */
						prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					xFreeBytesRemaining[CurHeapIdx] -= pxBlock->xBlockSize;

					if( xFreeBytesRemaining[CurHeapIdx] < xMinimumEverFreeBytesRemaining[CurHeapIdx] )
					{
						xMinimumEverFreeBytesRemaining[CurHeapIdx] = xFreeBytesRemaining[CurHeapIdx];
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The block is being returned - it is allocated and owned
					by the application and has no "next" block. */
					pxBlock->xBlockSize |= xBlockAllocatedBit;
					pxBlock->pxNextFreeBlock = NULL;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC( pvReturn, xWantedSize );
	}
	//( void ) xTaskResumeAll();
	CRITICAL_SECTION_FINISH();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	configASSERT( ( ( ( uint32_t ) pvReturn ) & portBYTE_ALIGNMENT_MASK ) == 0 );
#if (VFW_HEAP_DEBUG == 1)
	 tfp_printf("MALLOC: xWantedSize= %u, pvReturn =%08lX, %08X\n", xWantedSize, (uint32_t)pvReturn, pxBlock->xBlockSize);
#endif

	return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( void *pv )
{
uint8_t *puc = ( uint8_t * ) pv;
BlockLink_t *pxLink;

#if (VFW_HEAP_DEBUG == 1)
	tfp_printf("FREE: %08lX\n", (uint32_t)pv);
#endif
	if( pv != NULL )
	{
		//vTaskSuspendAll();
		CRITICAL_SECTION_START();
		kernel_do_on_free_entry((uint32_t)pv);
		/* The memory being freed will have an BlockLink_t structure immediately
		before it. */
		puc -= xHeapStructSize;

		/* This casting is to keep the compiler from issuing warnings. */
		pxLink = ( void * ) puc;
#if (VFW_HEAP_DEBUG == 1)
		tfp_printf("FREE: %08lX, Block Size: %08X\n", (uint32_t)pv, pxLink->xBlockSize);
#endif
		/* Check the block is actually allocated. */
		configASSERT( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 );
		configASSERT( pxLink->pxNextFreeBlock == NULL );

		if( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 )
		{
			if( pxLink->pxNextFreeBlock == NULL )
			{
				/* The block is being returned to the heap - it is no longer
				allocated. */
				pxLink->xBlockSize &= ~xBlockAllocatedBit;

				{
					/* Add this block to the list of free blocks. */
					xFreeBytesRemaining[CurHeapIdx] += pxLink->xBlockSize;
					traceFREE( pv, pxLink->xBlockSize );
					prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
		//( void ) xTaskResumeAll();
		CRITICAL_SECTION_FINISH();
	}
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
	return xFreeBytesRemaining[CurHeapIdx];
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( void )
{
	return xMinimumEverFreeBytesRemaining[CurHeapIdx];
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
	/* This just exists to keep the linker quiet. */
}
/*-----------------------------------------------------------*/

static void prvHeapInit( void )
{
BlockLink_t *pxFirstFreeBlock;
uint8_t *pucAlignedHeap;
uint32_t ulAddress;

MaxHeapSize[Kernel_Heap] = configTOTAL_HEAP_SIZE;
size_t xTotalHeapSize = MaxHeapSize[CurHeapIdx];

	/* Ensure the heap starts on a correctly aligned boundary. */
	ulAddress = ( uint32_t ) ucHeap;

	if( ( ulAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
	{
		ulAddress += ( portBYTE_ALIGNMENT - 1 );
		ulAddress &= ~( ( uint32_t ) portBYTE_ALIGNMENT_MASK );
		xTotalHeapSize -= ulAddress - ( uint32_t ) ucHeap;
	}

	pucAlignedHeap = ( uint8_t * ) ulAddress;

	/* xStart is used to hold a pointer to the first item in the list of free
	blocks.  The void cast is used to prevent compiler warnings. */
	xStart[CurHeapIdx].pxNextFreeBlock = ( void * ) pucAlignedHeap;
	xStart[CurHeapIdx].xBlockSize = ( size_t ) 0;

	/* pxEnd is used to mark the end of the list of free blocks and is inserted
	at the end of the heap space. */
	ulAddress = ( ( uint32_t ) pucAlignedHeap ) + xTotalHeapSize;
	HeapEnd[CurHeapIdx] = ulAddress;
	ulAddress -= xHeapStructSize;
	ulAddress &= ~( ( uint32_t ) portBYTE_ALIGNMENT_MASK );
	pxEnd[CurHeapIdx] = ( void * ) ulAddress;
	pxEnd[CurHeapIdx]->xBlockSize = 0;
	pxEnd[CurHeapIdx]->pxNextFreeBlock = NULL;

	/* To start with there is a single free block that is sized to take up the
	entire heap space, minus the space taken by pxEnd. */
	pxFirstFreeBlock = ( void * ) pucAlignedHeap;
	pxFirstFreeBlock->xBlockSize = ulAddress - ( uint32_t ) pxFirstFreeBlock;
	pxFirstFreeBlock->pxNextFreeBlock = pxEnd[CurHeapIdx];

	/* Only one block exists - and it covers the entire usable heap space. */
	xMinimumEverFreeBytesRemaining[CurHeapIdx] = pxFirstFreeBlock->xBlockSize;
	xFreeBytesRemaining[CurHeapIdx] = pxFirstFreeBlock->xBlockSize;

	/* Work out the position of the top bit in a size_t variable. */
	xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
BlockLink_t *pxIterator;
uint8_t *puc;

	/* Iterate through the list until a block is found that has a higher address
	than the block being inserted. */
	for( pxIterator = &xStart[CurHeapIdx]; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
	{
		/* Nothing to do here, just iterate to the right position. */
	}

	/* Do the block being inserted, and the block it is being inserted after
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxIterator;
	if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
	{
		pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
		pxBlockToInsert = pxIterator;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	/* Do the block being inserted, and the block it is being inserted before
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxBlockToInsert;
	if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
	{
		if( pxIterator->pxNextFreeBlock != pxEnd[CurHeapIdx] )
		{
			/* Form one big block from the two blocks. */
			pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
			pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
		}
		else
		{
			pxBlockToInsert->pxNextFreeBlock = pxEnd[CurHeapIdx];
		}
	}
	else
	{
		pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
	}

	/* If the block being inserted plugged a gab, so was merged with the block
	before and the block after, then it's pxNextFreeBlock pointer will have
	already been set, and should not be set here as that would make it point
	to itself. */
	if( pxIterator != pxBlockToInsert )
	{
		pxIterator->pxNextFreeBlock = pxBlockToInsert;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}

bool xPortSwitchHeap(e_HeapTypes heap, uint32_t heapStartAdr)
{
	if(heap == User_Heap){
#if (VFW_HEAP_DEBUG == 1)
		tfp_printf("switched to user heap!\n");
#endif
		heapStartAdr += 1024; // A buffer of 1KB
		CurHeapIdx = (uint8_t)User_Heap;
		ucHeap = (uint8_t*)heapStartAdr; // switch the heap
		if(! xHeapHasBeenInitialised[User_Heap] ){
		// Initialise the user heap
			UserHeapStart = heapStartAdr;
			MaxHeapSize[User_Heap] = (FT32_SRAM_END_ADDRESS - heapStartAdr) + 1; // align to word
			prvHeapInit();
			xHeapHasBeenInitialised[User_Heap]  = true;
		}
		return true;
	}
	else if( heap == Kernel_Heap){
		CurHeapIdx = (uint8_t)Kernel_Heap;
		ucHeap = KernelHeap;
#if (VFW_HEAP_DEBUG == 1)
		tfp_printf("switched to kernel heap!\n");
#endif
		return true;
	}

	return false;
}

void xPortResetUserHeap(void)
{
	xHeapHasBeenInitialised[User_Heap]  = false;
	UserHeapStart = FT32_SRAM_END_ADDRESS;
}

void *xPortKernelHeapMaxAddr(void)
{
	uint32_t ulAddress = HeapEnd[Kernel_Heap];
	return ulAddress;
}

#endif
