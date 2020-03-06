/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2014      */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "ft900_sdhost.h"
#include <string.h>




/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (FATFS *fs)
{
	DSTATUS stat = 0;

	if (SDHOST_OK != sdhost_card_init_ex(&fs->sdhost_context)) {

		stat = STA_NOINIT;
	}

	return stat;
}



/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	BYTE* buff,		/* Pointer to the destination object */
	DWORD sector,	/* Sector number (LBA) */
	UINT offset,	/* Offset in the sector */
	UINT count,		/* Byte count (bit15:destination) */
	FATFS *fs
)
{
	DRESULT res = RES_OK;
    uint8_t __attribute__ ((aligned (4))) temp[512];
	SDHOST_STATUS sdHostStatus = SDHOST_OK;

	// Always only read one sector
	sdHostStatus = sdhost_transfer_data_ex((void*) temp, sector, &fs->sdhost_context);
    //sdHostStatus = sdhost_transfer_data(SDHOST_READ, (void*) temp, SDHOST_BLK_SIZE, sector);
	// Adjust offset
	memmove(buff, temp + offset, count);

#if 0
	for(int i=0; i < 512; i++){
	    extern void tfp_printf(char *fmt, ...);
	    tfp_printf("%2X,", temp[i]);
	}
#endif

	if (sdHostStatus != SDHOST_OK) {
		res = RES_ERROR;
	}

	return res;
}



/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/

DRESULT disk_writep (const BYTE* buff, DWORD sc)
{
return RES_OK; // No implementation for SDBL
}

