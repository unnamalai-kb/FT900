/**
    @file

    @brief
    SD Host

    
**/
/*
 * ============================================================================
 * History
 * =======
 *
 * (C) Copyright 2014-2015, Future Technology Devices International Ltd.
 * ============================================================================
 *
 * This source code ("the Software") is provided by Future Technology Devices
 * International Limited ("FTDI") subject to the licence terms set out
 * http://www.ftdichip.com/FTSourceCodeLicenceTerms.htm ("the Licence Terms").
 * You must read the Licence Terms before downloading or using the Software.
 * By installing or using the Software you agree to the Licence Terms. If you
 * do not agree to the Licence Terms then do not download or use the Software.
 *
 * Without prejudice to the Licence Terms, here is a summary of some of the key
 * terms of the Licence Terms (and in the event of any conflict between this
 * summary and the Licence Terms then the text of the Licence Terms will
 * prevail).
 *
 * The Software is provided "as is".
 * There are no warranties (or similar) in relation to the quality of the
 * Software. You use it at your own risk.
 * The Software should not be used in, or for, any medical device, system or
 * appliance. There are exclusions of FTDI liability for certain types of loss
 * such as: special loss or damage; incidental loss or damage; indirect or
 * consequential loss or damage; loss of income; loss of business; loss of
 * profits; loss of revenue; loss of contracts; business interruption; loss of
 * the use of money or anticipated savings; loss of information; loss of
 * opportunity; loss of goodwill or reputation; and/or loss of, damage to or
 * corruption of data.
 * There is a monetary cap on FTDI's liability.
 * The Software may have subsequently been amended by another user and then
 * distributed by that other user ("Adapted Software").  If so that user may
 * have additional licence terms that apply to those amendments. However, FTDI
 * has no liability in relation to those amendments.
 * ============================================================================
 */

#ifndef FT900_SDHOST_H
#define FT900_SDHOST_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* INCLUDES ************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/* CONSTANTS ***********************************************************************/

/*
 *  Data block (sector) size
 */

#define SDHOST_BLK_SIZE                512
#define SDHOST_BLK_SIZE_POW				9
#define SDCARD_SDSTATUS_BLK_SIZE	   64  // The status register is 512 BITS in size

/*
 *  SD host registers.
 * 
 *  These constants are used to identify the individual sdhost registers, whatever their size.
 *  These are used to symbolically address registers via the functions read_sdhost_reg()
 *  and write_sdhost_reg().
 * 
 *  This is all necessary due to the fact we can only access the reigsters with 32bit values.
 */

/** @brief Auto CMD23 Argument 2 Register */
#define SDH_AUTO_CMD23_ARG2                  1    
/** @brief Block Size Register */
#define SDH_BLK_SIZE                         2    
/** @brief Block Count Register */
#define SDH_BLK_COUNT                        3    
/** @brief Argument 1 Register */
#define SDH_ARG_1                            4    
/** @brief Transfer Mode Register */
#define SDH_TNSFER_MODE                      5    
/** @brief Command Register */
#define SDH_CMD                              6    
/** @brief Response Registers 0 */
#define SDH_RESPONSE0                        7    
/** @brief Response Registers 1 */
#define SDH_RESPONSE1                        8    
/** @brief Response Registers 2 */
#define SDH_RESPONSE2                        9    
/** @brief Response Registers 3 */
#define SDH_RESPONSE3                       10    
/** @brief Buffer Data Port Register */
#define SDH_BUF_DATA                        11    
/** @brief Present State Register */
#define SDH_PRESENT_STATE                   12    
/** @brief Host Control 1 Register */
#define SDH_HST_CNTL_1                      13    
/** @brief Power Control Register */
#define SDH_PWR_CNTL                        14    
/** @brief Block Gap Control Register */
#define SDH_BLK_GAP_CNTL                    15    
/** @brief Clock Control Register */
#define SDH_CLK_CNTL                        16    
/** @brief Timeout Control Register */
#define SDH_TIMEOUT_CNTL                    17    
/** @brief Software Reset Register */
#define SDH_SW_RST                          18    
/** @brief Normal Interrupt Status Register */
#define SDH_NRML_INT_STATUS                 19    
/** @brief Error Interrupt Status Register */
#define SDH_ERR_INT_STATUS                  20    
/** @brief Normal Interrupt Status Enable Register */
#define SDH_NRML_INT_STATUS_ENABLE          21    
/** @brief Error Interrupt Status Enable Register */
#define SDH_ERR_INT_STATUS_ENABLE           22    
/** @brief Normal Interrupt Signal Enable Register */
#define SDH_NRML_INT_SGNL_ENABLE            23    
/** @brief Error Interrupt Signal Enable Register */
#define SDH_ERR_INT_SGNL_ENABLE             24    
/** @brief Auto CMD12 Error Status Register */
#define SDH_AUTO_CMD12_ERR_STATUS           25    
/** @brief Host Control 2 Register */
#define SDH_HST_CNTL_2                      26    
/** @brief Capabilities Register 1 */
#define SDH_CAP_1                           27    
/** @brief Capabilities Register 2 */
#define SDH_CAP_2                           28    
/** @brief Reserved Register 1 */
#define SDH_RSRV_1                          29    
/** @brief Reserved Register 2 */
#define SDH_RSRV_2                          30    
/** @brief Force Event Register for Auto CMD12 Error Status */
#define SDH_FORCE_EVENT_CMD_ERR_STATUS      31    
/** @brief Force Event Register for Error Interrupt Status */
#define SDH_FORCE_EVENT_ERR_INT_STATUS      32    
/** @brief Reserved Register 3 */
#define SDH_RSRV_3                          33    
/** @brief Reserved register 4 */
#define SDH_RSRV_4                          34    
/** @brief Preset Value Register 3 */
#define SDH_PRST_INIT                       35    
/** @brief Preset Value Register 3 */
#define SDH_PRST_DFLT_SPD                   36    
/** @brief Preset Value Register 3 */
#define SDH_PRST_HIGH_SPD                   37    
/** @brief Preset Value Register 3 */
#define SDH_PRST_SDR12                      38    
/** @brief Vendor-defined Register 0 */
#define SDH_VNDR_0                          39    
/** @brief Vendor-defined Register 1 */
#define SDH_VNDR_1                          40    
/** @brief Vendor-defined Register 5 */
#define SDH_VNDR_5                          41    


/*
 *  Data transfer direction definitions, these should not be modified
 */
#define SDHOST_WRITE                        0x0000
#define SDHOST_READ                         0x0010

/*
 *  SDIO special operations, to be used only when CMD52 is sent
 */
#define NOT_IN_USE                          0
#define SDIO_WRITE_CCCR_BUS_SUSPEND         1
#define SDIO_WRITE_CCCR_FUNCTION_SELECT     2
#define SDIO_WRITE_CCCR_IO_ABORT            3

/*
 *  bits defined in individual registers
 */

/* CLK_CTRL bits */
#define CLK_CTRL_CLK_STABLE                 0x0002

/* SW_RESET bits */
#define SW_RESET_DATA_LINE                  0x04
#define SW_RESET_CMD_LINE                   0x02
#define SW_RESET_ALL                        0x01

/* NORM_INT_STATUS bits */
#define NORM_INT_STATUS_CARD_RMV            0x00000080
#define NORM_INT_STATUS_CARD_INS            0x00000040
#define NORM_INT_STATUS_BUF_R_RDY           0x00000020
#define NORM_INT_STATUS_BUF_W_RDY           0x00000010
#define NORM_INT_STATUS_TRANSFER_COMPLETE   0x00000002
#define NORM_INT_STATUS_CMD_COMPLETE        0x00000001

/* ERR_INT_STATUS bits */
#define ERR_INT_STATUS_TUNING_ERR           0x00000400
#define ERR_INT_STATUS_ADMA_ERR             0x00000200
#define ERR_INT_STATUS_AUTO_CMD12_ERR       0x00000100
#define ERR_INT_STATUS_CUR_LIM_ERR          0x00000080
#define ERR_INT_STATUS_DATA_END_BIT_ERR     0x00000040
#define ERR_INT_STATUS_DATA_CRC_ERR         0x00000020
#define ERR_INT_STATUS_DATA_TIMEOUT_ERR     0x00000010
#define ERR_INT_STATUS_CMD_IDX_ERR          0x00000008
#define ERR_INT_STATUS_CMD_END_BIT_ERR      0x00000004
#define ERR_INT_STATUS_CMD_CRC_ERR          0x00000002
#define ERR_INT_STATUS_CMD_TIMEOUT_ERR      0x00000001

/* SDHOST_PRESENTSTATE bits */
#define PRESENT_STATE_SYS_CARD_STABLE       0x00020000
#define PRESENT_STATE_SYS_CARD_INST         0x00010000
#define PRESENT_STATE_BUF_RD_EN             0x00000800
#define PRESENT_STATE_BUF_WR_EN             0x00000400
#define PRESENT_STATE_RD_TRAN_ACT           0x00000200
#define PRESENT_STATE_WR_TRAN_ACT           0x00000100
#define PRESENT_STATE_DATA_LIN_ACT          0x00000004
#define PRESENT_STATE_CMD_INHIBIT_D         0x00000002
#define PRESENT_STATE_CMD_INHIBIT_C         0x00000001

/* The combination of response R1 error bits, used for examining error during CMD transfer */
#define RESPONSE_R1_ERROR_MASK              0xFDF90008
#define RESPONSE_R6_ERROR_MASK              0X0000E008

/* Transfer type (single / multiple-block) */
#define SDHOST_SINGLE_BLK                   0x0000
#define SDHOST_MULTI_BLK                    0x0020

/* DMA enable / disable */
#define SDHOST_DMA_DISABLED                 0x0000
#define SDHOST_DMA_ENABLED                  0x0001

/* BLK_COUNT enable / disable */
#define SDHOST_BLK_COUNT_DISABLED           0x0000
#define SDHOST_BLK_COUNT_ENABLED            0x0002

/* Auto CMD12 enable / disable */
#define SDHOST_AUTO_CMD12_DISABLED          0x0000
#define SDHOST_AUTO_CMD12_ENABLED           0x0004 /* Multiple block transfers for memory only */

/* Command types, specific to the Command Register */
#define CMD_TYPE_NORMAL                     0x00    /* All other commands */
#define CMD_TYPE_SUSPEND                    0x40    /* CMD52 for writing Bus Suspend in CCCR */
#define CMD_TYPE_RESUME                     0x80    /* CMD52 for writing Function Select in CCCR */
#define CMD_TYPE_ABORT                      0xC0    /* CMD12, CMD52 for writing I/O Abort in CCCR */

/* Data present select - bit 5 of the Command Register */
#define CMD_NO_DATA_PRESENT                 0x00
#define CMD_DATA_PRESENT                    0x20

/*
 *  Response types - a combination of Command Index Check Enable bit, Command CRC Check Enable bit
 *  and Response Type Select bit in the Command Register
 * 
 *  Refer to SD Host Controller Simplified Spec document para 2.2.7 response register response */

#define CMD_RESPONSE_NONE                               0x00
#define CMD_RESPONSE_R1                                 0x1A
#define CMD_RESPONSE_R1b                                0x1B
#define CMD_RESPONSE_R2                                 0x09
#define CMD_RESPONSE_R3                                 0x02
#define CMD_RESPONSE_R4                                 0x02    /* SDIO only */
#define CMD_RESPONSE_R5                                 0x1A    /* SDIO only */
#define CMD_RESPONSE_R5b                                0x1B    /* SDIO only */
#define CMD_RESPONSE_R6                                 0x1A
#define CMD_RESPONSE_R7                                 0x1A


/**
 * @brief  Mask for errors Card Status R1 (OCR Register)
 */
#define SD_OCR_ADDR_OUT_OF_RANGE        ((uint32_t)0x80000000)
#define SD_OCR_ADDR_MISALIGNED          ((uint32_t)0x40000000)
#define SD_OCR_BLOCK_LEN_ERR            ((uint32_t)0x20000000)
#define SD_OCR_ERASE_SEQ_ERR            ((uint32_t)0x10000000)
#define SD_OCR_BAD_ERASE_PARAM          ((uint32_t)0x08000000)
#define SD_OCR_WRITE_PROT_VIOLATION     ((uint32_t)0x04000000)
#define SD_OCR_LOCK_UNLOCK_FAILED       ((uint32_t)0x01000000)
#define SD_OCR_COM_CRC_FAILED           ((uint32_t)0x00800000)
#define SD_OCR_ILLEGAL_CMD              ((uint32_t)0x00400000)
#define SD_OCR_CARD_ECC_FAILED          ((uint32_t)0x00200000)
#define SD_OCR_CC_ERROR                 ((uint32_t)0x00100000)
#define SD_OCR_GENERAL_UNKNOWN_ERROR    ((uint32_t)0x00080000)
#define SD_OCR_STREAM_READ_UNDERRUN     ((uint32_t)0x00040000)
#define SD_OCR_STREAM_WRITE_OVERRUN     ((uint32_t)0x00020000)
#define SD_OCR_CID_CSD_OVERWRIETE       ((uint32_t)0x00010000)
#define SD_OCR_WP_ERASE_SKIP            ((uint32_t)0x00008000)
#define SD_OCR_CARD_ECC_DISABLED        ((uint32_t)0x00004000)
#define SD_OCR_ERASE_RESET              ((uint32_t)0x00002000)
#define SD_OCR_AKE_SEQ_ERROR            ((uint32_t)0x00000008)
#define SD_OCR_ERRORBITS                ((uint32_t)0xFDFFE008)

#define SD_V1	0x01
#define SD_V2	0x02
#define MMC_V3	0x03

typedef enum
{
	/**
	 * @brief  SDIO specific error defines
	 */
	SD_CMD_CRC_FAIL = (1), /*!< Command response received (but CRC check failed) */
	SD_DATA_CRC_FAIL = (2), /*!< Data bock sent/received (CRC check Failed) */
	SD_CMD_RSP_TIMEOUT = (3), /*!< Command response timeout */
	SD_DATA_TIMEOUT = (4), /*!< Data time out */
	SD_TX_UNDERRUN = (5), /*!< Transmit FIFO under-run */
	SD_RX_OVERRUN = (6), /*!< Receive FIFO over-run */
	SD_START_BIT_ERR = (7), /*!< Start bit not detected on all data signals in widE bus mode */
	SD_CMD_OUT_OF_RANGE = (8), /*!< CMD's argument was out of range.*/
	SD_ADDR_MISALIGNED = (9), /*!< Misaligned address */
	SD_BLOCK_LEN_ERR = (10), /*!< Transferred block length is not allowed for the card or the number of transferred bytes does not match the block length */
	SD_ERASE_SEQ_ERR = (11), /*!< An error in the sequence of erase command occurs.*/
	SD_BAD_ERASE_PARAM = (12), /*!< An Invalid selection for erase groups */
	SD_WRITE_PROT_VIOLATION = (13), /*!< Attempt to program a write protect block */
	SD_LOCK_UNLOCK_FAILED = (14), /*!< Sequence or password error has been detected in unlock command or if there was an attempt to access a locked card */
	SD_COM_CRC_FAILED = (15), /*!< CRC check of the previous command failed */
	SD_ILLEGAL_CMD = (16), /*!< Command is not legal for the card state */
	SD_CARD_ECC_FAILED = (17), /*!< Card internal ECC was applied but failed to correct the data */
	SD_CC_ERROR = (18), /*!< Internal card controller error */
	SD_GENERAL_UNKNOWN_ERROR = (19), /*!< General or Unknown error */
	SD_STREAM_READ_UNDERRUN = (20), /*!< The card could not sustain data transfer in stream read operation. */
	SD_STREAM_WRITE_OVERRUN = (21), /*!< The card could not sustain data programming in stream mode */
	SD_CID_CSD_OVERWRITE = (22), /*!< CID/CSD overwrite error */
	SD_WP_ERASE_SKIP = (23), /*!< only partial address space was erased */
	SD_CARD_ECC_DISABLED = (24), /*!< Command has been executed without using internal ECC */
	SD_ERASE_RESET = (25), /*!< Erase sequence was cleared before executing because an out of erase sequence command was received */
	SD_AKE_SEQ_ERROR = (26), /*!< Error in sequence of authentication. */
	SD_INVALID_VOLTRANGE = (27),
	SD_ADDR_OUT_OF_RANGE = (28),
	SD_SWITCH_ERROR = (29),
	SD_SDIO_DISABLED = (30),
	SD_SDIO_FUNCTION_BUSY = (31),
	SD_SDIO_FUNCTION_FAILED = (32),
	SD_SDIO_UNKNOWN_FUNCTION = (33),

	/**
	 * @brief  Standard error defines
	 */
	SD_INTERNAL_ERROR,
	SD_NOT_CONFIGURED,
	SD_REQUEST_PENDING,
	SD_REQUEST_NOT_APPLICABLE,
	SD_INVALID_PARAMETER,
	SD_UNSUPPORTED_FEATURE,
	SD_UNSUPPORTED_HW,
	SD_ERROR,
	SD_OK = 0
} SD_Error;

/* TYPES ***************************************************************************/

/*
 *  The type SDHOST_STATUS is returned from the provided function calls
 */

typedef enum {
    SDHOST_OK = 0,				/* OK */
    SDHOST_ERROR,				/* general error */
    SDHOST_CARD_INSERTED,			/* card inserted */
    SDHOST_CARD_REMOVED,			/* card removed */
    SDHOST_INVALID_RESPONSE_TYPE,		/* invalid response */
    SDHOST_CMD_TIMEOUT,				/* command timeout */
    SDHOST_UNUSABLE_CARD,			/* card is unusable */
    SDHOST_CMD2_FAILED,				/* command 2 (get CID) failed */
    SDHOST_CMD3_FAILED,				/* command 3 (get RCA) failed */
    SDHOST_CMD8_FAILED,				/* command 8 (voltage check) failed */
    SDHOST_CMD9_FAILED,				/* command 9 (send CSD) failed */
    SDHOST_CMD55_FAILED,			/* command 55 (app cmd) failed */
    SDHOST_ACMD41_FAILED,			/* command 41 failed */
    SDHOST_CANNOT_ENTER_TRANSFER_STATE,		/* cannot enter transfer state */
    SDHOST_CANNOT_SET_CARD_BUS_WIDTH,		/* cannot set bus width */
    SDHOST_RESPONSE_ERROR,			/* response error */
    SDHOST_WRITE_ERROR,				/* read error */
    SDHOST_READ_ERROR				/* write error */
} SDHOST_STATUS;

/* 
 *  sdhost_cmd_t specifies the command type to send to the sdhost hardware
 * 
 */

typedef enum {
    SDHOST_BUS_CMD,
    SDHOST_APP_SPECIFIC_CMD
} sdhost_cmd_t;

/*
 *  sdhost_response_t specifies the response from the sdhost hardware
 */

typedef enum {
    SDHOST_RESPONSE_NONE,
    SDHOST_RESPONSE_R1,			/* normal respose */
    SDHOST_RESPONSE_R1b,		/* normal respose */
    SDHOST_RESPONSE_R2,                 /* CID, CSD register */
    SDHOST_RESPONSE_R3,                 /* OCR register (memory) */
    SDHOST_RESPONSE_R4,                 /* OCR register (i/o etc. ) */
    SDHOST_RESPONSE_R5,                 /* SDIO response */
    SDHOST_RESPONSE_R5b,                /* SDIO response */
    SDHOST_RESPONSE_R6,                 /* RCA response */
    SDHOST_RESPONSE_R7			/* SEND_IF_COND response */
} sdhost_response_t;

/*
 *  sdhost context
 */

typedef struct
{
        SDHOST_STATUS  internalStatus;  /* internal status, mainly for debugging */
        bool            isSDSCCard;     /* is Secure digital standard capacity */
        uint8_t 		cardType;		/* SDv2/SDv1/MMCv3 card */
        uint32_t        CID[4];         /* card ID */
        uint32_t        CSD[4];         /* card specific data */
        uint32_t        OCR;            /* operations condition register*/
        uint16_t        RCA;            /* relative card address */
} sdhost_context_t;

/* FUNCTIONS ***********************************************************************/

/** @brief Function used for initializing system registers.
*/
void sdhost_sys_init(void);

/** @brief Function initializes SD Host device.
*/
void sdhost_init(void);

/** @brief Check to see if a card is inserted
 *  @returns either SDHOST_CARD_INSERTED or SDHOST_CARD_REMOBVED
 */
SDHOST_STATUS sdhost_card_detect(void);

/** @brief Identifies and initializes the inserted card
 *  @returns either SDHOST_ERROR or SDHOST_OK
 */
SDHOST_STATUS sdhost_card_init_ex(sdhost_context_t* sdhost_context);

/** @brief Transfer data to/from SD card
 *  @param direction SDHOST_READread or SDHOST_WRITE
 *  @param buf address of memory data to be read or written
 *  @param numBytes size of data to be written
 *  @param addr address of SD card to written to or read from
 *  @returns SDHOST_STATUS enum indicationg on outcome of operation
 */
SDHOST_STATUS sdhost_transfer_data_ex( void *buf, uint32_t addr, sdhost_context_t* sdhost_context);
//SDHOST_STATUS sdhost_transfer_data(uint8_t direction, void *buf, uint32_t num_bytes, uint32_t addr);

/** @brief Abort current sdhost operation
 *  @returns
 */
SDHOST_STATUS sdhost_abort(void);

/** @brief Get the internal properties of the attached card (context)
 *  @returns A pointer to SD HOST context
 */
sdhost_context_t* sdhost_get_context(void);

/** @brief Read the SD STATUS register of the card
 *  @param pointer to buffer to store status register dump
 */
void sdhost_get_card_status_reg(uint32_t* pBuff);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* FT900_SDHOST_H */
