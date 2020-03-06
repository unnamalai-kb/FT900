/**
 @file

 @brief
 Optmized version of SD card host controller for use with petit FatFS.
 No support for writes


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

/* INCLUDES ************************************************************************/

#include <stdint.h>
#include <stdbool.h>

#include "ft900_sdhost.h"
#include "ft900_delay.h"
#include "ft900.h"


// Enable to debug. Note that Debugging relies on tinyprintf
#define SD_HOST_DEBUG 0
#if SD_HOST_DEBUG
#include "tinyprintf.h"

void AnalyzeR1Status(uint32_t response_r1);
void RegisterDump(void);
#endif

/* CONSTANTS ***********************************************************************/

/* SD Card commands */

enum sd_command {
	SDCMD_GO_IDLE_STATE = 0,
	SDCMD_ALL_SEND_CID = 2,
	SDCMD_SEND_RELATIVE_ADDR = 3,
	SDCMD_SDIO = 5,
	SDCMD_SET_BUS_WIDTH = 6,
	SDCMD_SEL_DESEL_CARD = 7,
	SDCMD_SEND_IF_COND = 8,
	SDCMD_SEND_CSD = 9,
	SDCMD_STOP_TRANSMISSION = 12,
	SDCMD_READ_STATUS_REG = 13,
	SDCMD_SEND_BLOCKLEN = 16,
	SDCMD_SEND_OP_CON = 41,
	SDCMD_APP_CMD = 55
};


#define COMM_TIMEOUT_U32		(uint32_t)(10000000) // A simple timeout
/* GLOBAL VARIABLES ****************************************************************/

/* LOCAL VARIABLES *****************************************************************/


#ifdef __USE_SDHOST_INTERRUPT_
volatile static int card_status_changed = false;
volatile static int cmd_complete = false;
volatile static int transfer_complete = false;
volatile static int write_buf_ready = false;
volatile static int read_buf_ready = false;
volatile static int errRecoverable;
volatile static uint16_t errStatus;
#endif

/* MACROS **************************************************************************/
/*
 *  FT900 32 bit registers
 *  These are used to provide the actual 32bit address of the sdhost registers
 *  These are named arbitrarily
 *
 */

#if defined(__FT900__)
/* Block Size Register & Block Count Register */
#define SDHOST_BS_BC                          ((volatile uint32_t *)0x10404)
/* Argument 1 Register */
#define SDHOST_ARG1                           ((volatile uint32_t *)0x10408)
/* Transfer Mode Register & Command Register */
#define SDHOST_TM_CMD                         ((volatile uint32_t *)0x1040c)
/* Response Register 0 */
#define SDHOST_RESPONSE0                      ((volatile uint32_t *)0x10410)
/* Response Register 1 */
#define SDHOST_RESPONSE1                      ((volatile uint32_t *)0x10414)
/* Response Register 2 */
#define SDHOST_RESPONSE2                      ((volatile uint32_t *)0x10418)
/* Response Register 3 */
#define SDHOST_RESPONSE3                      ((volatile uint32_t *)0x1041c)
/* Buffer Data Port Register */
#define SDHOST_BUFDATAPORT                    ((volatile uint32_t *)0x10420)
/* Present State Register */
#define SDHOST_PRESENTSTATE                   ((volatile uint32_t *)0x10424)
/* Host Control 1 Register & Power Control Register & Block Gap Control Register */
#define SDHOST_HOSTCTRL1_PWR_BLKGAP           ((volatile uint32_t *)0x10428)
/* Clock Control Register & Timeout Control Register & Software Reset Register */
#define SDHOST_CLK_TIMEOUT_SWRESET            ((volatile uint32_t *)0x1042c)
/* Normal Interrupt Status Register & Error Interrupt Status Register */
#define SDHOST_NORMINTSTATUS_ERRINTSTATUS     ((volatile uint32_t *)0x10430)
/* Normal Interrupt Status Enable Reg. & Error Interrupt Status Enable Reg. */
#define SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN ((volatile uint32_t *)0x10434)
/* Normal Interrupt Signal Enable Reg. & Error Interrupt Signal Enable Reg. */
#define SDHOST_NORMINTSIGEN_ERRINTSIGEN       ((volatile uint32_t *)0x10438)
/* Auto CMD12 Error Status Register & Host Control 2 Register */
#define SDHOST_AUTOCMD12_HOSTCTRL2            ((volatile uint32_t *)0x1043c)
/* Capabilities Register 0 */
#define SDHOST_CAP0                           ((volatile uint32_t *)0x10440)
/* Capabilities Register 1 */
#define SDHOST_CAP1                           ((volatile uint32_t *)0x10444)
/* Maximum Current Capabilities Register 0 */
#define SDHOST_MAXCURRENTCAP0                 ((volatile uint32_t *)0x10448)
/* Maximum Current Capabilities Register 1 */
#define SDHOST_MAXCURRENTCAP1                 ((volatile uint32_t *)0x1044c)
/* Force Event Reg. for Auto CMD12 Error Status & Force Event Reg. for Error Interrupt Status */
#define SDHOST_FORCEEVENTCMD12_FORCEEVENTERRINT ((volatile uint32_t *)0x10450)
/* ADMA Error Status Register */
#define SDHOST_ADMAERRORSTATUS                ((volatile uint32_t *)0x10454)
/* ADMA System Address Register */
#define SDHOST_ADMASYSADDR                    ((volatile uint32_t *)0x10458)
/* Preset Value Register 0 */
#define SDHOST_PRESETVALUE0                   ((volatile uint32_t *)0x10460)
/* Preset Value Register 1 */
#define SDHOST_PRESETVALUE1                   ((volatile uint32_t *)0x10464)
/* Preset Value Register 2 */
#define SDHOST_PRESETVALUE2                   ((volatile uint32_t *)0x10468)
/* Preset Value Register 3 */
#define SDHOST_PRESETVALUE3                   ((volatile uint32_t *)0x1046c)
/* Vendor-defined Register 0 */
#define SDHOST_VENDOR0                        ((volatile uint32_t *)0x10500)
/* Vendor-defined Register 1 */
#define SDHOST_VENDOR1                        ((volatile uint32_t *)0x10504)
/* Vendor-defined Register 5 */
#define SDHOST_VENDOR5                        ((volatile uint32_t *)0x10514)

#elif defined(__FT930__)
/* Block Size Register & Block Count Register */
#define SDHOST_BS_BC                          ((volatile uint32_t *)0x10604)
/* Argument 1 Register */
#define SDHOST_ARG1                           ((volatile uint32_t *)0x10608)
/* Transfer Mode Register & Command Register */
#define SDHOST_TM_CMD                         ((volatile uint32_t *)0x1060c)
/* Response Register 0 */
#define SDHOST_RESPONSE0                      ((volatile uint32_t *)0x10610)
/* Response Register 1 */
#define SDHOST_RESPONSE1                      ((volatile uint32_t *)0x10614)
/* Response Register 2 */
#define SDHOST_RESPONSE2                      ((volatile uint32_t *)0x10618)
/* Response Register 3 */
#define SDHOST_RESPONSE3                      ((volatile uint32_t *)0x1061c)
/* Buffer Data Port Register */
#define SDHOST_BUFDATAPORT                    ((volatile uint32_t *)0x10620)
/* Present State Register */
#define SDHOST_PRESENTSTATE                   ((volatile uint32_t *)0x10624)
/* Host Control 1 Register & Power Control Register & Block Gap Control Register */
#define SDHOST_HOSTCTRL1_PWR_BLKGAP           ((volatile uint32_t *)0x10628)
/* Clock Control Register & Timeout Control Register & Software Reset Register */
#define SDHOST_CLK_TIMEOUT_SWRESET            ((volatile uint32_t *)0x1062c)
/* Normal Interrupt Status Register & Error Interrupt Status Register */
#define SDHOST_NORMINTSTATUS_ERRINTSTATUS     ((volatile uint32_t *)0x10630)
/* Normal Interrupt Status Enable Reg. & Error Interrupt Status Enable Reg. */
#define SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN ((volatile uint32_t *)0x10634)
/* Normal Interrupt Signal Enable Reg. & Error Interrupt Signal Enable Reg. */
#define SDHOST_NORMINTSIGEN_ERRINTSIGEN       ((volatile uint32_t *)0x10638)
/* Auto CMD12 Error Status Register & Host Control 2 Register */
#define SDHOST_AUTOCMD12_HOSTCTRL2            ((volatile uint32_t *)0x1063c)
/* Capabilities Register 0 */
#define SDHOST_CAP0                           ((volatile uint32_t *)0x10640)
/* Capabilities Register 1 */
#define SDHOST_CAP1                           ((volatile uint32_t *)0x10644)
/* Maximum Current Capabilities Register 0 */
#define SDHOST_MAXCURRENTCAP0                 ((volatile uint32_t *)0x10648)
/* Maximum Current Capabilities Register 1 */
#define SDHOST_MAXCURRENTCAP1                 ((volatile uint32_t *)0x1064c)
/* Force Event Reg. for Auto CMD12 Error Status & Force Event Reg. for Error Interrupt Status */
#define SDHOST_FORCEEVENTCMD12_FORCEEVENTERRINT ((volatile uint32_t *)0x10650)
/* ADMA Error Status Register */
#define SDHOST_ADMAERRORSTATUS                ((volatile uint32_t *)0x10654)
/* ADMA System Address Register */
#define SDHOST_ADMASYSADDR                    ((volatile uint32_t *)0x10658)
/* Preset Value Register 0 */
#define SDHOST_PRESETVALUE0                   ((volatile uint32_t *)0x10660)
/* Preset Value Register 1 */
#define SDHOST_PRESETVALUE1                   ((volatile uint32_t *)0x10664)
/* Preset Value Register 2 */
#define SDHOST_PRESETVALUE2                   ((volatile uint32_t *)0x10668)
/* Preset Value Register 3 */
#define SDHOST_PRESETVALUE3                   ((volatile uint32_t *)0x1066c)
/* Vendor-defined Register 0 */
#define SDHOST_VENDOR0                        ((volatile uint32_t *)0x10700)
/* Vendor-defined Register 1 */
#define SDHOST_VENDOR1                        ((volatile uint32_t *)0x10704)
/* Vendor-defined Register 5 */
#define SDHOST_VENDOR5                        ((volatile uint32_t *)0x10714)

#else

#error -- processor type not defined

#endif /* __FT900__ */

/* ft900 gpio registers */

#if defined(__FT930__)
/* function 2 configuration */
#define SYS_REGPAD00             ((volatile uint8_t  *)0x1001C) /*SD CLK*/
#define SYS_REGPAD01             ((volatile uint8_t  *)0x1001D) /*SD CMD*/
#define SYS_REGPAD02             ((volatile uint8_t  *)0x1001E) /*SD CD*/
#define SYS_REGPAD03             ((volatile uint8_t  *)0x1001F) /*SD DAT0*/
#define SYS_REGPAD04             ((volatile uint8_t  *)0x10020) /*SD DAT1*/
#define SYS_REGPAD05             ((volatile uint8_t  *)0x10021) /*SD DAT2*/
#define SYS_REGPAD06             ((volatile uint8_t  *)0x10022) /*SD DAT3*/
#define SYS_REGPAD07             ((volatile uint8_t  *)0x10023) /*SD WP*/
#else
/* function 1 configuration */
#define SYS_REGPAD19             ((volatile uint8_t  *)0x1002f) /*SD CLK*/
#define SYS_REGPAD20             ((volatile uint8_t  *)0x10030) /*SD CMD*/
#define SYS_REGPAD21             ((volatile uint8_t  *)0x10031) /*SD DAT3*/
#define SYS_REGPAD22             ((volatile uint8_t  *)0x10032) /*SD DAT2*/
#define SYS_REGPAD23             ((volatile uint8_t  *)0x10033) /*SD DAT1*/
#define SYS_REGPAD24             ((volatile uint8_t  *)0x10034) /*SD DAT0*/
#define SYS_REGPAD25             ((volatile uint8_t  *)0x10035) /*SD CD*/
#define SYS_REGPAD26             ((volatile uint8_t  *)0x10036) /*SD WP*/
#endif

#define SYS_REGCLKCFG            ((volatile uint32_t *)0x10008)

#if SD_HOST_DEBUG
#define DBG_TIMEOUT()			 if(timeout == 0) {printf("Timeout ERROR @"); printf(__FILE__); printf(" %d", __LINE__); RegisterDump();}
#else
#define DBG_TIMEOUT()
#endif
/* LOCAL FUNCTIONS / INLINES *******************************************************/

static uint32_t read_sdhost_reg(uint32_t addr);
static void write_sdhost_reg(uint32_t val, uint32_t addr);
static void sdhost_wait_till_set(uint32_t nint_stat_cmd);
static void sdhost_reset_line(uint32_t line);
static void sdhost_wait_till_clear(uint32_t nint_stat_cmd);
static  void sdhost_wait_till_debounced(void);

#ifdef __USE_SDHOST_INTERRUPT_
/* SD Host ISR */
extern void attach_interrupt(int, void (*func)(void));
void sdhost_isr(void);
#endif

/* FUNCTIONS ***********************************************************************/



/** @brief Recover from SD Host errors
 *  @param none
 *  @returns none
 */

static void sdhost_error_recovery(void) {

	uint16_t errIntStatus = (uint16_t) read_sdhost_reg(SDH_ERR_INT_STATUS);

	if (errIntStatus & 0x07FF) {
		/* Disable the Error Interrupt Signal */
		write_sdhost_reg(~(errIntStatus), SDH_ERR_INT_SGNL_ENABLE);

		/*
		 *  Check bits [3:0] in the Error Interrupt Status register
		 *  If one of bits [3:0] is set, namely CMD Line Error occurs,
		 *  set soft_rst_cmd to 1 in the Software Reset register
		 */

		if (errIntStatus & 0x000F) {
			/*
			 *  Check soft_rst_cmd in the Software Reset register.
			 *  If 0, continue. If 1, set soft_rst_cmd to 1 in the Software Reset register again.
			 */
		    sdhost_reset_line(SW_RESET_CMD_LINE);

		}

		/*
		 *  Check bits [6:4] in the Error Interrupt Status register
		 *  If one of bits[6:4] is set, namely DAT Line Error occurs,
		 *  set soft_rst_dat to 1 in the Software Reset register
		 */

		if (errIntStatus & 0x0070) {
			/*
			 *  Check soft_rst_dat in the Software Reset register.
			 *  If 0, continue. If 1, set soft_rst_dat to 1 in the Software Reset register again.
			 */
		    sdhost_reset_line(SW_RESET_DATA_LINE);

		}

		/* Clear previous error status by setting them to 1 */
		write_sdhost_reg(errIntStatus, SDH_ERR_INT_STATUS);

		/* Enable the Error Interrupt Signal */
		write_sdhost_reg(
				read_sdhost_reg(SDH_ERR_INT_SGNL_ENABLE) | errIntStatus,
				SDH_ERR_INT_SGNL_ENABLE);
	}

	return;

}


/** @brief Initialise system registers to enable SD Host peripheral
 *  @param none
 *  @returns none
 */

void sdhost_sys_init(void) {
#if defined(__FT930__)
	/* Enable pad for SD Host (bit [7:6]). Must pull up (bit [3]) as stated in specs... */
	*SYS_REGPAD00 = 0x80; /* CLK, none */
	*SYS_REGPAD01 = 0x80; /* CMD, none */
	*SYS_REGPAD02 = 0x80; /* CD, none */
	*SYS_REGPAD03 = 0x80; /* DATA0, none */
	*SYS_REGPAD04 = 0x80; /* DATA1, none */
	*SYS_REGPAD05 = 0x80; /* DATA2, none */
	*SYS_REGPAD06 = 0x80; /* DATA3, none */
	*SYS_REGPAD07 = 0x80; /* WP, none */
#else
	/* Enable pad for SD Host (bit [7:6]). Must pull up (bit [3]) as stated in specs... */
	*SYS_REGPAD19 = 0x40; /* CLK */
	*SYS_REGPAD20 = 0x40; /* CMD, pulled up */
	*SYS_REGPAD21 = 0x40; /* DATA3, pulled up */
	*SYS_REGPAD22 = 0x40; /* DATA2, pulled up */
	*SYS_REGPAD23 = 0x40; /* DATA1, pulled up */
	*SYS_REGPAD24 = 0x40; /* DATA0, pulled up */
	*SYS_REGPAD25 = 0x40; /* CD, pulled up */
	*SYS_REGPAD26 = 0x40; /* WP, pulled up */
#endif

	/* Enable SD Host function (bit 12) */
    sys_enable(sys_device_sd_card);

	return;
}

/**
 *  @brief Read a register from the SD Host peripheral
 *
 *  Access to the SD host operipheral registers can only be per perfornmed on a 32 bit basis.
 *  read_sdhost_rec() takes care of any masking and shifting when accessing a register
 *  less than 32 bits.
 *
 *  @param addr register id.
 *  @returns the contents of the register requested
 */

static uint32_t read_sdhost_reg(uint32_t addr) {
	switch (addr) {
	case SDH_BLK_SIZE:
		return *SDHOST_BS_BC & 0x0000fff;

	case SDH_BLK_COUNT:
		return *SDHOST_BS_BC >> 16;

	case SDH_TNSFER_MODE:
		return *SDHOST_TM_CMD & 0x0000ffff;

	case SDH_CMD:
		return *SDHOST_TM_CMD >> 16;

	case SDH_RESPONSE0:
		return *SDHOST_RESPONSE0;

	case SDH_RESPONSE1:
		return *SDHOST_RESPONSE1;

	case SDH_RESPONSE2:
		return *SDHOST_RESPONSE2;

	case SDH_RESPONSE3:
		return *SDHOST_RESPONSE3;

	case SDH_BUF_DATA:
		return *SDHOST_BUFDATAPORT;

	case SDH_PRESENT_STATE:
		return *SDHOST_PRESENTSTATE;

	case SDH_HST_CNTL_1:
		return *SDHOST_HOSTCTRL1_PWR_BLKGAP & 0xff;

	case SDH_PWR_CNTL:
		return (*SDHOST_HOSTCTRL1_PWR_BLKGAP >> 8) & 0xff;

	case SDH_CLK_CNTL:
		return *SDHOST_CLK_TIMEOUT_SWRESET & 0xffff;

	case SDH_SW_RST:
		return (*SDHOST_CLK_TIMEOUT_SWRESET >> 24) & 0xff;

	case SDH_NRML_INT_STATUS:
		return *SDHOST_NORMINTSTATUS_ERRINTSTATUS & 0xffff;

	case SDH_ERR_INT_STATUS:
		return *SDHOST_NORMINTSTATUS_ERRINTSTATUS >> 16;

	case SDH_NRML_INT_STATUS_ENABLE:
		return *SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN & 0xffff;

	case SDH_ERR_INT_STATUS_ENABLE:
		return *SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN >> 16;

	case SDH_NRML_INT_SGNL_ENABLE:
		return *SDHOST_NORMINTSIGEN_ERRINTSIGEN & 0xffff;

	case SDH_ERR_INT_SGNL_ENABLE:
		return *SDHOST_NORMINTSIGEN_ERRINTSIGEN >> 16;

	case SDH_HST_CNTL_2:
		return *SDHOST_AUTOCMD12_HOSTCTRL2 >> 16;

	case SDH_CAP_1:
		return *SDHOST_CAP0;

	case SDH_CAP_2:
		return *SDHOST_CAP1;

	case SDH_VNDR_0:
		return *SDHOST_VENDOR0;

	case SDH_VNDR_1:
		return *SDHOST_VENDOR1;

	case SDH_VNDR_5:
		return *SDHOST_VENDOR5;

	default:
		return 0;
		/* Invalid address */
	}
}

/** @brief Write value to a SD Host peripheral register
 *
 *  This function is used for writing (any) SD Host registers.
 *  All this is absolutely necessary as we can only accees the SDhost hardware in 32 bit chunks.
 *  This function takes care of the necessary and'ing and or'ing of bits.
 *
 *  @param addr register id.
 *  @param val value to write
 *  @returns the contents of the register requested
 */

static void write_sdhost_reg(uint32_t val, uint32_t addr) {
	switch (addr) {
	case SDH_BLK_SIZE:
		*SDHOST_BS_BC = val | (*SDHOST_BS_BC & 0xffff0000);
		break;

	case SDH_BLK_COUNT:
		*SDHOST_BS_BC = (val << 16) | (*SDHOST_TM_CMD & 0xffff);
		break;

	case SDH_TNSFER_MODE:
		*SDHOST_TM_CMD = val | (*SDHOST_TM_CMD & 0xffff0000);
		break;

	case SDH_CMD:
		*SDHOST_TM_CMD = (val << 16) | (*SDHOST_TM_CMD & 0xffff);
		break;

	case SDH_BUF_DATA:
		*SDHOST_BUFDATAPORT = val;
		break;

	case SDH_HST_CNTL_1:
		*SDHOST_HOSTCTRL1_PWR_BLKGAP = val
				| (*SDHOST_HOSTCTRL1_PWR_BLKGAP & 0xffffff00);
		break;

	case SDH_PWR_CNTL:
		*SDHOST_HOSTCTRL1_PWR_BLKGAP = (val << 8)
				| (*SDHOST_HOSTCTRL1_PWR_BLKGAP & 0xffff00ff);
		break;

	case SDH_CLK_CNTL:
		*SDHOST_CLK_TIMEOUT_SWRESET = val
				| (*SDHOST_CLK_TIMEOUT_SWRESET & 0xffff0000);
		break;

	case SDH_SW_RST:
		*SDHOST_CLK_TIMEOUT_SWRESET = (val << 24)
				| (*SDHOST_CLK_TIMEOUT_SWRESET & 0xffffff);
		break;

	case SDH_NRML_INT_STATUS:
		*SDHOST_NORMINTSTATUS_ERRINTSTATUS = val
				| (*SDHOST_NORMINTSTATUS_ERRINTSTATUS & 0xffff0000);
		break;

	case SDH_ERR_INT_STATUS:
		*SDHOST_NORMINTSTATUS_ERRINTSTATUS = (val << 16)
				| (*SDHOST_NORMINTSTATUS_ERRINTSTATUS & 0xffff);
		break;

	case SDH_NRML_INT_STATUS_ENABLE:
		*SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN = val
				| (*SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN & 0xffff0000);
		break;

	case SDH_ERR_INT_STATUS_ENABLE:
		*SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN = (val << 16)
				| (*SDHOST_NORMINTSTATUSEN_ERRINTSTATUSEN & 0xffff);
		break;

	case SDH_NRML_INT_SGNL_ENABLE:
		*SDHOST_NORMINTSIGEN_ERRINTSIGEN = val
				| (*SDHOST_NORMINTSIGEN_ERRINTSIGEN & 0xffff0000);
		break;

	case SDH_ERR_INT_SGNL_ENABLE:
		*SDHOST_NORMINTSIGEN_ERRINTSIGEN = (val << 16)
				| (*SDHOST_NORMINTSIGEN_ERRINTSIGEN & 0xffff);
		break;

	case SDH_HST_CNTL_2:
		*SDHOST_AUTOCMD12_HOSTCTRL2 = (val << 16)
				| (*SDHOST_AUTOCMD12_HOSTCTRL2 & 0xffff);
		break;

	case SDH_VNDR_0:
		*SDHOST_VENDOR0 = val;
		break;

	case SDH_VNDR_1:
		*SDHOST_VENDOR1 = val;
		break;

	case SDH_VNDR_5:
		*SDHOST_VENDOR5 = val;
		break;

	default:
		break;
		/* Invalid address */
	}

	return;
}


#if SD_HOST_DEBUG
void sdhost_register_dump(void){

    for(int i=1; i <= SDH_VNDR_5; i++){
        printf("%d : %08X \n", i, read_sdhost_reg(i));
    }

}
#endif
/** @brief Initialise the SD Host peripheral
 *  @param none
 *  @returns none
 */

void sdhost_init(void) {
	uint32_t timeout;

	/* Reset all */
	sdhost_reset_line(SW_RESET_ALL);

	/*
	 * Set card detection de-bouncing time
	 * vendor vegister 5 sets the card debouncing debug time
	 * where 0 = 2^^9, 1 = 2^^10 ... 10 - 2^^24
	 */
	write_sdhost_reg(0x0B, SDH_VNDR_5);
	// Writing into the debounce register triggers a de-bounce sequence and you have to wait here till the debounce is done before you do anything else!
	sdhost_wait_till_debounced();

	/* reset all bits in host control 2 register */
	write_sdhost_reg(0, SDH_HST_CNTL_2);

	/* Set SD bus voltage after checking SDH_CAP_1 for voltage support */
	if (read_sdhost_reg(SDH_CAP_1) & 0x01000000) /* SD Host supports 3.3 V */
	{
		write_sdhost_reg(0x0E, SDH_PWR_CNTL);
	} else if (read_sdhost_reg(SDH_CAP_1) & 0x02000000) { /* SD Host supports 3.0 V */
		write_sdhost_reg(0x0C, SDH_PWR_CNTL);
	} else if (read_sdhost_reg(SDH_CAP_1) & 0x04000000) { /* SD Host supports 1.8 V */
		write_sdhost_reg(0x0A, SDH_PWR_CNTL);
	}

	/* Enable SD Bus Power */
	write_sdhost_reg(read_sdhost_reg(SDH_PWR_CNTL) | 0x01, SDH_PWR_CNTL);

	/* Enable interrupt signal and status */
	write_sdhost_reg(0x01FFF, SDH_NRML_INT_STATUS_ENABLE);
	write_sdhost_reg(0x07FF, SDH_ERR_INT_STATUS_ENABLE);

    /* Enable high speed, set bus data width to 1-bit mode */
    write_sdhost_reg(0x05, SDH_HST_CNTL_1);

	/* Configure CRC status wait cycle, pulse latch offset to 0x01 (pulse latch offset MUST be set to 0x01) */
	write_sdhost_reg(0x02000101, SDH_VNDR_0);
	write_sdhost_reg(0, SDH_VNDR_1);

	/* Stop the SD clock if it was active */
	write_sdhost_reg(read_sdhost_reg(SDH_CLK_CNTL) & 0xFFFB, SDH_CLK_CNTL);

    /* In the card identification mode, the maximum clock frequency is 400 kHz. Have to set the divider to reduce the clock speed */
    /* Enable internal clock, choose divider to be 0x40 (i.e. base SD clock divided by 128) */
	write_sdhost_reg(0x4001, SDH_CLK_CNTL);

	/* Wait for the internal clock to be stable */
	timeout = COMM_TIMEOUT_U32;
	while ((!(read_sdhost_reg(SDH_CLK_CNTL) & CLK_CTRL_CLK_STABLE))
			&& (--timeout != 0))
		;
	DBG_TIMEOUT()

	/* Finally enable SD Clock */
    write_sdhost_reg(read_sdhost_reg(SDH_CLK_CNTL) | 0x0004, SDH_CLK_CNTL);


#if SD_HOST_DEBUG
    sdhost_register_dump();
#endif

}



/** @brief This function detects whether an SD card is inserted
 *  @param none
 *  @returns SDHOST_ERROR / SDHOST_CARD_INSERTED / SDHOST_CARD_REMOVED
 */

SDHOST_STATUS sdhost_card_detect(void) {

    SDHOST_STATUS sdhost_status = SDHOST_ERROR;

    //sdhost_wait_till_debounced();

    /* Check if a card has been inserted */
    if (read_sdhost_reg(SDH_PRESENT_STATE) & PRESENT_STATE_SYS_CARD_INST)
    {
        sdhost_status = SDHOST_CARD_INSERTED;
    } else {

        sdhost_status = SDHOST_CARD_REMOVED;

#ifdef __USE_SDHOST_INTERRUPT_
        /* Wait for interrupt to occur */
        while (!card_status_changed)
        ;

        /* Interrupt detected! Check SDH_PRESENT_STATE to see if the card has been inserted or removed */

        if (read_sdhost_reg(SDH_PRESENT_STATE) & PRESENT_STATE_SYS_CARD_INST)
        {
            sdhost_status = SDHOST_CARD_INSERTED;
        } else if (!(read_sdhost_reg(SDH_PRESENT_STATE) & PRESENT_STATE_SYS_CARD_INST)) {
            sdhost_status = SDHOST_CARD_REMOVED;
        }

        /* Reset card detection flag */
        card_status_changed = false;
#else
        if ((read_sdhost_reg(SDH_NRML_INT_STATUS) & NORM_INT_STATUS_CARD_INS) == 0) {
            sdhost_status = SDHOST_CARD_REMOVED;
        } else {
            /* Clear the flag by writing 1 to the flag bit */
            write_sdhost_reg(NORM_INT_STATUS_CARD_INS, SDH_NRML_INT_STATUS);
            //sdhost_status = SDHOST_CARD_INSERTED;
        }
#endif
    }

    return sdhost_status;
}

/**
 *  @brief This function gets a response from a command issued to the SD Host peripheral
 *
 *  This is an internal function that extarts a response from the SD host peripheral in
 *  response to an issued command.
 *
 *  @param response a pointer to the array (R2) / variable (others) that will hold the response
 *  @param response_type the response type
 *  @param is_auto_command_response indicates whether the response is an auto CMD response (auto CMD12/CMD23)
 *  @returns SDHOST_STATUS
 */

static SDHOST_STATUS sdhost_get_response(uint32_t *response,
		sdhost_response_t response_type) {

	SDHOST_STATUS sdhost_status = SDHOST_OK;

#if 0
	if (is_auto_cmd_response) {
		/*
		 *    Bits [39:8] (card status) of the response for Auto CMD23 (R1) / Auto CMD12 (R1b) is stored
		 *   in SDH_RESPONSE3 register
		 */

		*response = read_sdhost_reg(SDH_RESPONSE3);

#if SD_HOST_DEBUG
		for (int i = 0; i < 1; i++) {
			printf("%08X", *((unsigned char*) response + i));
		}
		printf("\n--------------------------------\n");
#endif
	} else {
#endif
		if (response_type == SDHOST_RESPONSE_R2) {
			/* For R2, bits [127:8] of the response is stored in all 4 response registers */
			response[0] = read_sdhost_reg(SDH_RESPONSE0);
			response[1] = read_sdhost_reg(SDH_RESPONSE1);
			response[2] = read_sdhost_reg(SDH_RESPONSE2);
			response[3] = read_sdhost_reg(SDH_RESPONSE3);

#if SD_HOST_DEBUG
			for (int i = 0; i < 4*4; i++) {
				printf("0x%02X,", *((unsigned char*) response + i));
			}
			printf("\n--------------------------------\n");
#endif
		} else {

			*response = read_sdhost_reg(SDH_RESPONSE0);

#if SD_HOST_DEBUG

			for (int i = 0; i < 1; i++) {
				printf("%08X", *((unsigned char*) response + i));
			}
			printf("\n--------------------------------\n");
#endif
		}

#if 0
	}
#endif

	return sdhost_status;
}

/** @brief This function generates a command for the Command Register.
 *  @param cmd_index command to generate (0-63)
 *  @param cmd_type command type (bus / application-specific)
 *  @param sdio_special_op the SDIO special operation for CMD52, assign NOT_IN_USE
 *          if the operation is not one of the 3 special ones
 *
 *  @param resp_type_ptr pointer to a variable to save the response type for the cmd
 *  @returns Returns the value to be written to Command Register
 */

static uint16_t sdhost_generate_cmd(uint8_t cmd_index, sdhost_response_t *resp_type_ptr) {
	sdhost_response_t response_type;
	uint16_t cmd_reg_value;



	/* preset */
	response_type = SDHOST_RESPONSE_NONE;
	cmd_reg_value = 0;

	/* Compute Command Register value based on the info passed into this function */
	/* First, set the cmd_idx bits */
	cmd_reg_value = cmd_index << 8;

	/* Now, compute the other bits */
#if 0
	if (cmd_type == SDHOST_BUS_CMD) {
#endif

#if 0   // Not Used
		if (cmd_index == SDCMD_STOP_TRANSMISSION) {
			cmd_reg_value |= CMD_TYPE_ABORT;
		} else {
			cmd_reg_value |= CMD_TYPE_NORMAL;
		}
#endif

		/* Then set the data_pres_sel bits */
		/*
		 *  Refer to the SD Physical Layer Simplified Spec page 69
		 *  for the list of commands with data transfer
		 */
#if 0
		if (cmd_index == SDCMD_SET_BUS_WIDTH)
			cmd_reg_value |= CMD_DATA_PRESENT;
		else
			cmd_reg_value |= CMD_NO_DATA_PRESENT;
#endif

		/* Last, get the response type and set the bits */

		/*
		 *  Refer to the SD Physical Layer Simplified Spec page 69 for
		 *  the response type for each command
		 */
		switch (cmd_index) {
		case SDCMD_GO_IDLE_STATE:
			cmd_reg_value |= CMD_RESPONSE_NONE;
			response_type = SDHOST_RESPONSE_NONE;
			break;

		case SDCMD_ALL_SEND_CID:
		case SDCMD_SEND_CSD:
			cmd_reg_value |= CMD_RESPONSE_R2;
			response_type = SDHOST_RESPONSE_R2;
			break;

		case SDCMD_SEND_RELATIVE_ADDR:
			cmd_reg_value |= CMD_RESPONSE_R6;
			response_type = SDHOST_RESPONSE_R6;
			break;

		case SDCMD_SDIO:
			cmd_reg_value |= CMD_RESPONSE_R4;
			response_type = SDHOST_RESPONSE_R4;
			break;

		case SDCMD_SEND_IF_COND:
			cmd_reg_value |= CMD_RESPONSE_R7;
			response_type = SDHOST_RESPONSE_R7;
			break;

		case SDCMD_SEL_DESEL_CARD:
//		case SDCMD_STOP_TRANSMISSION: --> Not used
			cmd_reg_value |= CMD_RESPONSE_R1b;
			response_type = SDHOST_RESPONSE_R1b;
			break;
#if 0 // Same as default case
		case SDCMD_READ_STATUS_REG:
			cmd_reg_value |= CMD_RESPONSE_R1;
			response_type = SDHOST_RESPONSE_R1;
			break;
#endif

	        /* Refer to the SD Physical Layer Simplified Spec page 74 for the response type for each command */
	        case SDCMD_SEND_OP_CON:
	            cmd_reg_value |= CMD_RESPONSE_R3;
	            response_type = SDHOST_RESPONSE_R3;
	            break;

		default:
			cmd_reg_value |= CMD_RESPONSE_R1;
			response_type = SDHOST_RESPONSE_R1;
			break;
		}

		*resp_type_ptr = response_type;

#if 0
	} else if (cmd_type == SDHOST_APP_SPECIFIC_CMD) {

		/* Set the cmd_type bits in the Command Register */
		cmd_reg_value |= CMD_TYPE_NORMAL;

		/* Other commands are without data transfer */
		cmd_reg_value |= CMD_NO_DATA_PRESENT;

		/* Last, get the response type and set the bits */
		switch (cmd_index) {
		/* Refer to the SD Physical Layer Simplified Spec page 74 for the response type for each command */
		case SDCMD_SEND_OP_CON:
			cmd_reg_value |= CMD_RESPONSE_R3;
			response_type = SDHOST_RESPONSE_R3;
			break;

		case SDCMD_READ_STATUS_REG:
			cmd_reg_value |= CMD_RESPONSE_R2;
			response_type = SDHOST_RESPONSE_R2;
			break;

		default:
			cmd_reg_value |= CMD_RESPONSE_R1;
			response_type = SDHOST_RESPONSE_R1;
			break;
		}
		*resp_type_ptr = response_type;
	}
#endif
	return (cmd_reg_value);
}

/** @brief This function sends a command without data transfer.
 *  @param cmd_index command to generate (0-63)
 *  @param cmd_type command type (bus / application-specific)
 *  @param sdio_special_op the SDIO special operation for CMD52, assign NOT_IN_USE
 *         if the operation is not one of the 3 special ones
 *
 *  @param cmd_arg command argument (32 bits)
 *  @param  response pointer to the array (R2) / variable (others) that will hold the response
 *  @returns a SD Host status
 */

SDHOST_STATUS sdhost_send_command(uint8_t cmd_index, uint32_t cmd_arg, uint32_t *response) {
#if SD_HOST_DEBUG
	printf("--------------------------------\n");
	printf("CMD:%d\n",cmd_index );
#endif

	SDHOST_STATUS sdhost_status;
	uint16_t cmd_reg_value;
	sdhost_response_t response_type;

	/* preset */
	sdhost_status = SDHOST_OK;
	response_type = SDHOST_RESPONSE_NONE;

	/* Compute the value for Command Register */
	cmd_reg_value = sdhost_generate_cmd(cmd_index,&response_type);

	/*
	 *  Wait until Command Inhibit (CMD) bit in the present state register is 0.
	 *  When Command Inhibit (CMD) is set to 1, the SD command bus is in use and the host driver
	 *  cannot issue a SD command.
	 */
	sdhost_wait_till_clear(PRESENT_STATE_CMD_INHIBIT_C );
//	timeout = COMM_TIMEOUT_U32;
//	while (((present_state = read_sdhost_reg(SDH_PRESENT_STATE))
//			& PRESENT_STATE_CMD_INHIBIT_C) && (--timeout != 0))
//		;
//	DBG_TIMEOUT()

	/* Check if the previous SD command uses the SD data bus with a busy signal */
	if (read_sdhost_reg(SDH_PRESENT_STATE) & PRESENT_STATE_DATA_LIN_ACT) {
		/*
		 *  If the host driver does not issue an abort command, check cmd_inhibit_d in the present state register.
		 *  Repeat this step until cmd_inhibit_d becomes 0.
		 */
		if ((cmd_reg_value & CMD_TYPE_ABORT) != CMD_TYPE_ABORT) {
		    sdhost_wait_till_clear(PRESENT_STATE_CMD_INHIBIT_D );

		}
	}

	/* Set the argument to the argument register */
	*SDHOST_ARG1 = cmd_arg;

	/* Set the command register */
	write_sdhost_reg((uint32_t) cmd_reg_value, SDH_CMD);


#if 0
	/* Wait for the Command Complete bit in the Normal Status register to be set */
	timeout = COMM_TIMEOUT_U32;
	while (((read_sdhost_reg(SDH_NRML_INT_STATUS) & NORM_INT_STATUS_CMD_COMPLETE)
			== 0) && (--timeout != 0)) {

	}
	DBG_TIMEOUT()
#endif

	sdhost_wait_till_set(NORM_INT_STATUS_CMD_COMPLETE);
    /*
     *  If the Command Timeout bit is set in the Error Status register,
     *  return an error.
     */
    if (read_sdhost_reg(SDH_ERR_INT_STATUS) & ERR_INT_STATUS_CMD_TIMEOUT_ERR) {
        /* perform error recovery */
        sdhost_error_recovery();
        #if SD_HOST_DEBUG
        printf("SDHOST_CMD_TIMEOUT");
        #endif
        sdhost_status = SDHOST_CMD_TIMEOUT;
        goto __exit;

    };

	/* Clear the status flag by writing 1 to it */
	write_sdhost_reg(NORM_INT_STATUS_CMD_COMPLETE, SDH_NRML_INT_STATUS);


	/* Get response data */
	sdhost_get_response(response, response_type);

#if SD_HOST_DEBUG
	RegisterDump();
#endif
	/*
	 *  Check whether the command uses the Transfer Complete Interrupt
	 *  by checking cmd_inhibit_d bit in the Present State Register
	 */

	if (read_sdhost_reg(SDH_PRESENT_STATE) & PRESENT_STATE_CMD_INHIBIT_D) {

#if 0
		/* Wait for the Transfer Complete bit in the Normal Status register to be set */
		timeout = COMM_TIMEOUT_U32;
		while (((read_sdhost_reg(SDH_NRML_INT_STATUS)
				& NORM_INT_STATUS_TRANSFER_COMPLETE) == 0) && (--timeout != 0))
			;
		DBG_TIMEOUT()

		/* Clear the status flag by writing 1 to it */
		write_sdhost_reg(NORM_INT_STATUS_TRANSFER_COMPLETE,
		SDH_NRML_INT_STATUS);
#endif
		sdhost_wait_till_set(NORM_INT_STATUS_TRANSFER_COMPLETE);

	}

	/*
	 *  Examine the response data if the response type is R1 or R1b,
	 *  return SDHOST_ERROR if an error occurred
	 */

	if ((response_type == SDHOST_RESPONSE_R1)
			|| (response_type == SDHOST_RESPONSE_R1b)) {
#if SD_HOST_DEBUG
		AnalyzeR1Status(*response);
#endif
		if ((*response) & RESPONSE_R1_ERROR_MASK)
			sdhost_status = SDHOST_RESPONSE_ERROR;
	} else if (response_type == SDHOST_RESPONSE_R6) {
		if ((*response) & RESPONSE_R6_ERROR_MASK)
			sdhost_status = SDHOST_RESPONSE_ERROR;
	}

	/* Check if any error occurred. Note that SDHOST_CMD_TIMEOUT (if any) has been detected and returned prior to this. */


	if (read_sdhost_reg(SDH_ERR_INT_STATUS) & 0x07FF) {
		/* Recover SD Host from error */
		#if SD_HOST_DEBUG
		printf("SDH_ERR_INT_STATUS");
		#endif
		sdhost_error_recovery();
		sdhost_status = SDHOST_ERROR;
	}


	__exit: return (sdhost_status);
}


void sdhost_wait_till_set(uint32_t nint_stat_cmd){
    /* (6) Wait for the Command Complete bit in the Normal Status register to be set */
    uint32_t timeout = COMM_TIMEOUT_U32;
    while (((read_sdhost_reg(SDH_NRML_INT_STATUS)
            & nint_stat_cmd) == 0) && (--timeout != 0))
        ;
    DBG_TIMEOUT()

    /*(7) Clear the status flag by writing 1 to it */
    write_sdhost_reg(nint_stat_cmd, SDH_NRML_INT_STATUS);
}

void sdhost_reset_line(uint32_t line){
    /*
     *  Check soft_rst_cmd in the Software Reset register.
     *  If 0, continue. If 1, set soft_rst_cmd to 1 in the Software Reset register again.
     */
    uint32_t timeout = COMM_TIMEOUT_U32;
    do {
        write_sdhost_reg(line, SDH_SW_RST);
        delayus(1);
    } while ((read_sdhost_reg(SDH_SW_RST) & line) && (--timeout != 0));
    DBG_TIMEOUT()
}

 void sdhost_wait_till_clear(uint32_t nint_stat_cmd){
    /* (6) Wait for the Command Complete bit in the Normal Status register to be clear */
    uint32_t timeout = COMM_TIMEOUT_U32;
    while (((read_sdhost_reg(SDH_PRESENT_STATE) & nint_stat_cmd)) && (--timeout != 0))
        ;
    DBG_TIMEOUT()

}

 void sdhost_wait_till_debounced(void){

     uint32_t timeout = COMM_TIMEOUT_U32;
      // wait for status to be debounced
      while( ((read_sdhost_reg(SDH_PRESENT_STATE) & PRESENT_STATE_SYS_CARD_STABLE) == 0) && (--timeout != 0));
      DBG_TIMEOUT()

 }

/**
 *  @brief This function peforms data transfers to/fr0m the SD card
 *
 *  This function follows paragraph 3.7.2.1 in the SD Host Controller Simplified Specification V3.00
 *  The flow has been modified so that each transfer type has it's own path through the function, instead
 *  of having common parts of the function as implied in the specification.
 *
 *  @param direction SDHOST_READ / SDHOST_WRITE
 *  @param buf data buffer
 *  @param num_bytes number of bytes to be transferred
 *  @param addr sector address
 *  @returns a SD Host status
 */

// Govind: Optimization we know that petit FatFs will only read single blocks and no writes are allowed for SD bootloader..
SDHOST_STATUS sdhost_transfer_data_ex( void *buf, uint32_t addr, sdhost_context_t* sdhost_context) {

	SDHOST_STATUS sdhost_status;

	uint32_t response;
	uint32_t *buf32;
	uint16_t cmd;
	uint16_t transfer_mode;


	/* initialise local variables */
	sdhost_status = SDHOST_OK;

	buf32 = (uint32_t *) buf;
	transfer_mode = 0;


	/*
	 *  Note: For block CMDs (read/write/lock) SDSC Card uses byte unit address and SDHC/SDXC Cards
	 *        use block unit address (512 Bytes unit); We have to convert from sector address to the
	 *        appropriate address for the CMDs first
	 */

	if (sdhost_context->isSDSCCard == true)
		addr <<= SDHOST_BLK_SIZE_POW;

	/*
	 *  Note: The length of a multiple block transfer needs to be in block size units.
	 *        If the total data length cannot be divided evenly by the block size, there
	 *        are two ways to transfer the data depending on the function and card design:
	 *        Option 1 is for the Card Driver to split the transaction. The remainder of block size
	 *                 data is transferred by using a single block command at the end.
	 *        Option 2 is to add dummy data in the last block to fill the block size. This is efficient but the card must
	 *                 manage or remove the dummy data.
	 */

	/*
	 *  Note: Since Transfer Mode and Command Register has to be accessed at the same time and a
	 *        write to Command Register will send a command, it's better to set both registers at
	 *        the same time to prevent the current command in the Command Register to be sent again.
	 */

	/*
	 *  Note: To improve performance, response data is not be checked.
	 */

	/* Calculate how many "whole" blocks are needed. The remaining byte will be sent in one single-block transfer. */

//	num_blocks = num_bytes >> SDHOST_BLK_SIZE_POW;
//	num_bytes_left = num_bytes & ((1UL << SDHOST_BLK_SIZE_POW) - 1);

	/* Send CMD16 to set the card's block length */

	sdhost_send_command(SDCMD_SEND_BLOCKLEN, SDHOST_BLK_SIZE, &response);

	/*
	 *  If only one "whole" block is needed, perform single-block transfer for this block.
	 *  Remaining bytes (if any) will be sent in another single-block transfer later.
	 *  This also include the case where less than SDHOST_BLK_SIZE bytes are transferred.
	 */

	//if (num_blocks == 1) {
		/* (1) Set Block Size register */
		*SDHOST_BS_BC = (*SDHOST_BS_BC & 0xFFFF0000) | SDHOST_BLK_SIZE;

		/* (2) Single-block transfer, SDH_BLK_COUNT register will be ignored */

			/* (3) Set Argument 1 Register */
			*SDHOST_ARG1 = addr;

			/* (4) Set Transfer Mode Register and Command Register at the same time */
			cmd = (17 << 8) | CMD_TYPE_NORMAL | CMD_RESPONSE_R1
					| CMD_DATA_PRESENT;
			transfer_mode = SDHOST_DMA_DISABLED | SDHOST_AUTO_CMD12_DISABLED
					| SDHOST_SINGLE_BLK | SDHOST_READ;

			/* (5) Set the command register */
			*SDHOST_TM_CMD = (cmd << 16) | transfer_mode;


			sdhost_wait_till_set(NORM_INT_STATUS_CMD_COMPLETE);


			/* (8) Get response data */
			/* sdhost_get_response(&response, SDHOST_RESPONSE_R1, false); -- not checked */

			/* (14) Wait for Buffer Read Ready */
#if 0
			timeout = COMM_TIMEOUT_U32;
			while (((read_sdhost_reg(SDH_NRML_INT_STATUS)
					& NORM_INT_STATUS_BUF_R_RDY) == 0) && (--timeout != 0))
				;
			DBG_TIMEOUT()

			/* (15) Clear the flag by writing 1 to the flag bit */
			write_sdhost_reg(NORM_INT_STATUS_BUF_R_RDY, SDH_NRML_INT_STATUS);
#endif
			sdhost_wait_till_set(NORM_INT_STATUS_BUF_R_RDY);


			/*
			 * (16) Read block data from Buffer Data Port register
			 * Note: To improve performance, use inline assembly here
			 */

			__asm__ ( "streamin.l        %0, %1, %2    \n\t"
					: /* output registers */
					: "r" (buf32), "r" (&(*SDHOST_BUFDATAPORT)), "r" (SDHOST_BLK_SIZE) /* input registers */
			);

#if 0
		/* (19) Wait for the Transfer Complete bit in the Normal Status register to be set */
		timeout = COMM_TIMEOUT_U32;
		while (((read_sdhost_reg(SDH_NRML_INT_STATUS)
				& NORM_INT_STATUS_TRANSFER_COMPLETE) == 0) && (--timeout != 0))
			;
		DBG_TIMEOUT()

		/* (20)  Clear the status flag by writing 1 to it */
		write_sdhost_reg(NORM_INT_STATUS_TRANSFER_COMPLETE,
		SDH_NRML_INT_STATUS);
#endif
		sdhost_wait_till_set(NORM_INT_STATUS_TRANSFER_COMPLETE);


		/* Set the next index in the data buffer for the remaining bytes */
		//nextIdx = SDHOST_BLK_SIZE >> 2;



	return (sdhost_status);
}

#if 0
/** @brief abort current SD controller host operation
 *
 *  Function sends Abort Command following the Asynchronous Abort Sequence
 *  ref: (SD Host Simplified Spec page 101).
 *
 *  @param none
 *  @returns a SD Host Status
 */

SDHOST_STATUS sdhost_abort(void) {
	#if SD_HOST_DEBUG
	printf("sdhost_abort\n");
	#endif
	SDHOST_STATUS sdhost_status = SDHOST_OK;
	uint32_t response;
	uint32_t timeout;

	sdhost_send_command(SDCMD_STOP_TRANSMISSION,  0,&response);

	/* Set Software Reset For DAT line (soft_rst_dat = 1) and CMD line (soft_rst_cmd = 1) */
	timeout = COMM_TIMEOUT_U32;
	do {
		write_sdhost_reg(SW_RESET_CMD_LINE | SW_RESET_DATA_LINE, SDH_SW_RST);
	} while (((read_sdhost_reg(SDH_SW_RST) & SW_RESET_CMD_LINE)
			|| (read_sdhost_reg(SDH_SW_RST) & SW_RESET_DATA_LINE))
					&& (--timeout != 0));
	DBG_TIMEOUT()

	return (sdhost_status);
}
#endif

#define SUPPORT_SDIO    0
/** @brief Initialise the sdcard host device
 *
 *  Initialises and identifies the SD card that is being connected.
 *  Function sends Abort Command following the Asynchronous Abort Sequence
 *
 *  See SD Host Controller Simplified Specification V3.00 Para. 3.6
 *  The numbers in parantheses indicate the progress of the steps shown in Para 6.1.3
 *  of the simplified specification.
 *
 *  @param none
 *  @returns a SD Host Status
 *
 */

SDHOST_STATUS sdhost_card_init_ex(sdhost_context_t* sdhost_context) {

	SDHOST_STATUS sdhost_status = SDHOST_OK;
	uint8_t F8 = 0, SDIO = 0;
	uint32_t response;
	bool initFailed = false;
	uint8_t retry = 0;
	uint32_t timeout;

	sdhost_context->internalStatus = SDHOST_OK;

	do {
		/* Reset init status flag */
		initFailed = false;

		/* (1) SD Bus mode is selected by CMD0, always succeeds */
		sdhost_status = sdhost_send_command(SDCMD_GO_IDLE_STATE, 0, &response);

		/* (2) Issue CMD8 to check the high-capacity SD memory card. SD Physical Layer Simplified Spec page 59. */
		sdhost_status = sdhost_send_command(SDCMD_SEND_IF_COND, 0x000001AA, &response);

		/* (3) Check if the card is a legacy (non-SD) cards. Legacy cards do not respond to CMD8. */
		if (sdhost_status == SDHOST_CMD_TIMEOUT) {
			/* Legacy card detected, clear F8 flag - SDv1 or MMCv3 */
			F8 = 0;
			#if SD_HOST_DEBUG
			printf("SDv1 or MMCv3 detected.. \n");
			#endif
			sdhost_context->cardType = SD_V1;
		} else {
			/*
			 *  The card responded to CMD8. Check validity of the response: CRC, VHS-VCA and check pattern
			 *  CRC:
			 */
			if (sdhost_status == SDHOST_ERROR) {
				initFailed = true;
				retry++;
				continue;
			}

			/* VHS-VCA & check pattern (0xAA) comparison: */
			if ((response & 0x00000FFF) != 0x000001AA) {
				initFailed = true;
				retry++;
				continue;
			}

			/* If we reach here, response for CMD8 is OK, set F8 flag and continue - SDCv2 */
			F8 = 1;
			sdhost_context->cardType = SD_V2;
			#if SD_HOST_DEBUG
			printf("SDCv2 detected..\n");
			#endif
		}
	} while (initFailed && (retry < 2)); /* (4) */

	if (initFailed) {
		//sdhost_context->internalStatus = SDHOST_CMD8_FAILED;
		//sdhost_status = SDHOST_UNUSABLE_CARD;
		//goto __exit;
	    return (sdhost_status);
	}

#if SUPPORT_SDIO
	/* (5) Issue CMD5 to obtain SDIO OCR by setting the voltage window to 0 in the argument */
	sdhost_status = sdhost_send_command(SDCMD_SDIO, 0, &response);

	if (sdhost_status == SDHOST_CMD_TIMEOUT) {
		/* (6) This card does not support SDIO function */
		sdhost_error_recovery();
		SDIO = 0;

	}


	else if (sdhost_status == SDHOST_OK) {
		/* (7) TODO: Add support for SDIO cards here */
	} else if (sdhost_status == SDHOST_ERROR) {
		/* This card does not support SDIO function */
		SDIO = 0;

		/* (10) TODO: Check MP (memory present) flag in the response */
	}
#endif

#if 1 // Govind: Is it safe to disable this voltage window check??
	/*
	 *  OCR is available by issuing ACMD41 and setting the voltage window (Bit 23 to bit 0) in the argument to 0.
	 *   CMD55 should be issued before ACMD41.
	 */
	sdhost_status = sdhost_send_command(SDCMD_APP_CMD,0, &response);

	if (sdhost_status != SDHOST_OK) {
		if (sdhost_status != SDHOST_RESPONSE_ERROR) {
			return (sdhost_status);
		}
	}

	/*
	 *  Note: If the voltage window field is set to zero, it is called "inquiry CMD41" that does not start initialization.
	 *        It is used for getting OCR (OCR = response).
	 *        The inquiry ACMD41 shall ignore the other field (bit 31-24) in the argument. So it is safe to use '0' for the argument.
	 */

	sdhost_status = sdhost_send_command(SDCMD_SEND_OP_CON,0x00000000,
			&(sdhost_context->OCR));

#endif

#if SUPPORT_SDIO
	if (sdhost_status != SDHOST_OK) {
		if ((sdhost_status == SDHOST_CMD_TIMEOUT) && (F8 == 0)) {
			/* This card is not an SD card */
			sdhost_context->internalStatus = SDHOST_ACMD41_FAILED;
			sdhost_status = SDHOST_UNUSABLE_CARD;
		} else {
			if (SDIO) {
				/* Issue CMD3 to get RCA */
				sdhost_status = sdhost_send_command(SDCMD_SEND_RELATIVE_ADDR, 0, &response);
				if (sdhost_status == SDHOST_OK) {
					/* RCA is stored in the upper 16 bits of CMD3 response */
					sdhost_context->RCA = response >> 16;
				}
			} else {
				sdhost_context->internalStatus = SDHOST_CMD3_FAILED;
				sdhost_status = SDHOST_UNUSABLE_CARD;
			}
		}
	} else {
#endif

		/*
		 *  Issue ACMD41 again to set the voltage window. Check the busy status in the response.
		 *  While the busy is indicated (OCR bit[31] = 0), repeat to issue ACMD41.
		 *
		 *  Note: If the voltage window field (bit 23-0) in the argument is set to non-zero at the first time,
		 *        it is called "first ACMD41" that starts initialization.
		 *
		 *  The other field (bit 31-24) in the argument is effective. Since this host supports high capacity, HCS is set to 1.
		 *  The argument of following ACMD41 shall be the same as that of the first ACMD41.
		 */
		timeout = COMM_TIMEOUT_U32;
		do {

			sdhost_status = sdhost_send_command(SDCMD_APP_CMD,0, &response); /* (23) */
			if (sdhost_status != SDHOST_OK) {return (sdhost_status);}

			/* Note: SDSC Card ignores HCS. If HCS is set to 0, SDHC and SDXC Cards never
			 *        return ready status (SD Physical Layer Simplified Spec)
			 */

			/* select 2.7-3.3 voltage window */
			sdhost_status = sdhost_send_command(SDCMD_SEND_OP_CON,
					sdhost_context->OCR | 0x50000000, &(sdhost_context->OCR));

			if (sdhost_status != SDHOST_OK) { /* (24) */
#if SUPPORT_SDIO
				if (SDIO) {
					/* Issue CMD3 to get RCA */
					sdhost_status = sdhost_send_command(
							SDCMD_SEND_RELATIVE_ADDR,
							 0, &response);
					if (sdhost_status == SDHOST_OK) {
						/* RCA is stored in the upper 16 bits of CMD3 response */
						sdhost_context->RCA = response >> 16;
					}
				} else {
#endif
				//	sdhost_context->internalStatus = SDHOST_ACMD41_FAILED;
				//	sdhost_status = SDHOST_UNUSABLE_CARD;
#if SUPPORT_SDIO
				}
#endif
				//goto __exit;
				return (sdhost_status);
			}
			delayus(10);
		} while ((!(sdhost_context->OCR & 0x80000000)) && (--timeout != 0));
		DBG_TIMEOUT()
#if SD_HOST_DEBUG
		printf("exit ACMD loop:%08X\r", sdhost_context->OCR);
#endif
		/*
		 *  (25) If F8 != 0, check CCS bit in the response to determine if the card is a Standard Capacity SD Memory Card
		 *   or High Capacity SD Memory Card
		 */

		if (F8 == 1) {
			if (sdhost_context->OCR & 0x40000000) /* (26) */
			{
				sdhost_context->isSDSCCard = false;
				#if SD_HOST_DEBUG
				printf("Detected SDHC\n");
				#endif
			} else {
				sdhost_context->isSDSCCard = true; /* (27) */
				#if SD_HOST_DEBUG
				printf("Detected SDSC\n");
				#endif
			}
		} else {
			sdhost_context->isSDSCCard = true;
		}

		/* (32) Issue CMD2 to get CID */
		sdhost_status = sdhost_send_command(SDCMD_ALL_SEND_CID,0, sdhost_context->CID); /* CMD2 responses with R2, which is the 127-bit CID */
		if (sdhost_status != SDHOST_OK) {
			//sdhost_context->internalStatus = SDHOST_CMD2_FAILED;
			//sdhost_status = SDHOST_ERROR;
			//goto __exit;
		    return (sdhost_status);
		}

		/* (33) Issue CMD3 to get RCA */
		sdhost_status = sdhost_send_command(SDCMD_SEND_RELATIVE_ADDR, 0, &response);
		if (sdhost_status != SDHOST_OK) {
			//sdhost_context->internalStatus = SDHOST_CMD3_FAILED;
			//sdhost_status = SDHOST_ERROR;
			//goto __exit;
		    return (sdhost_status);
		}

		/* RCA is stored in the upper 16 bits of CMD3 response */
		sdhost_context->RCA = response >> 16;
#if SD_HOST_DEBUG
		printf("RCA = %08X",sdhost_context->RCA);
#endif
		/* Issue CMD9 to get CSD */
		sdhost_status = sdhost_send_command(SDCMD_SEND_CSD,sdhost_context->RCA << 16, sdhost_context->CSD);
		if (sdhost_status != SDHOST_OK) {
			//sdhost_context->internalStatus = SDHOST_CMD9_FAILED;
			//sdhost_status = SDHOST_ERROR;
			//goto __exit;
		    return (sdhost_status);
		}
#if SD_HOST_DEBUG
		printf("CSD = ");
		for (int i = 0; i < 16; i++){
			printf("0x%02X,", *(((uint8_t*) &sdhost_context->CSD[0]) + i));
		}
#endif


		/* Send CMD7 to set the card in Data Transfer mode */
		sdhost_status = sdhost_send_command(SDCMD_SEL_DESEL_CARD,  sdhost_context->RCA << 16,
				&response);

		if ((sdhost_status != SDHOST_OK) || (response != 0x700)) {
			//sdhost_context->internalStatus = SDHOST_CANNOT_ENTER_TRANSFER_STATE;
			//sdhost_status = SDHOST_ERROR;
			//goto __exit;
		    return (sdhost_status);
		}


		/* Clear transfer complete interrupt flag due to CMD7 */

		write_sdhost_reg(NORM_INT_STATUS_TRANSFER_COMPLETE,SDH_NRML_INT_STATUS);


		//sdhost_status = sdhost_send_command(SDCMD_READ_STATUS_REG, SDHOST_BUS_CMD, NOT_IN_USE, sdhost_context->RCA << 16,  &response);

		/* Send ACMD6 to set the card to 4-bit bus mode */
		sdhost_status = sdhost_send_command(SDCMD_APP_CMD,sdhost_context->RCA << 16, &response);
		sdhost_status = sdhost_send_command(SDCMD_SET_BUS_WIDTH, 0x2, &response);

		if (sdhost_status != SDHOST_OK) {
			//sdhost_context->internalStatus = SDHOST_CANNOT_SET_CARD_BUS_WIDTH;
			//sdhost_status = SDHOST_ERROR;
			//goto __exit;
		    return (sdhost_status);
		} else {
			/* Set SD host bus width to 4-bit mode */
			write_sdhost_reg(0x02, SDH_HST_CNTL_1);
		}

#if SUPPORT_SDIO
	}
#endif



	/*
	 *  Now set the SD clock to 25 MHz for data transfer
	 *  NOTE: The SDHC "base clock" is 50MHz and NOT 100MHz, that's why we use a divide by 2 factor here.
	 *  See redmine bug 402 - http://glared1.ftdi.local/issues/402
	 */
	 /*  Disable SD clock */
	write_sdhost_reg(read_sdhost_reg(SDH_CLK_CNTL) & 0xFFFB, SDH_CLK_CNTL);

	/* Clear the frequency divider and load with divide by 2 factor (=1) */
	write_sdhost_reg(read_sdhost_reg(SDH_CLK_CNTL) & 0x003F, SDH_CLK_CNTL);
	write_sdhost_reg(read_sdhost_reg(SDH_CLK_CNTL) | 0x0100, SDH_CLK_CNTL);

    /* Wait for the internal clock to be stable */
	// See redmine bug 402 - http://glared1.ftdi.local/issues/402
    timeout = COMM_TIMEOUT_U32;
    while ((!(read_sdhost_reg(SDH_CLK_CNTL) & CLK_CTRL_CLK_STABLE))
            && (--timeout != 0))
        ;
    DBG_TIMEOUT()

    /* Finally enable SD Clock Output */
    write_sdhost_reg(read_sdhost_reg(SDH_CLK_CNTL) | 0x0004, SDH_CLK_CNTL);

	__exit:

	if (sdhost_status != SDHOST_OK) {
		#if SD_HOST_DEBUG
		printf("---------------- SD HOST CARD INIT ERROR -----------");
		#endif
	} else {
#if SD_HOST_DEBUG
		printf("************* SD HOST CARD INIT OK **********\n");
		sdhost_register_dump();
		unsigned long __attribute__ ((aligned (4))) data[64 / 4];
		sdhost_get_card_status_reg(data);
#endif
	}
	return (sdhost_status);
}

/** @brief This function returns the contents of SD Status register by sending the ACMD13 command.
 *  @param  response pointer to store the contents of SD Status register (64 bytes). Note that it must be 32 byte aligned!
 *  @returns a SD Host status
 */
void sdhost_get_card_status_reg(uint32_t* pBuff)
{

}



#if SD_HOST_DEBUG

void RegisterDump(void) {

	uint8_t response[4];
	printf("PRESENT STATE: %08X\n", read_sdhost_reg(SDH_PRESENT_STATE));
	printf("HOST CNTL 1: %08X\n", read_sdhost_reg(SDH_HST_CNTL_1));
	printf("NRM INT STATUS: %08X\n", read_sdhost_reg(SDH_NRML_INT_STATUS));
	printf("ERROR INT STATUS: %08X\n", read_sdhost_reg(SDH_ERR_INT_STATUS));


}

void AnalyzeR1Status(uint32_t response_r1) {

	if (response_r1 & SD_OCR_ADDR_OUT_OF_RANGE) {
		printf("SD_ADDR_OUT_OF_RANGE");
	}

	if (response_r1 & SD_OCR_ADDR_MISALIGNED) {
		printf("SD_ADDR_MISALIGNED");
	}

	if (response_r1 & SD_OCR_BLOCK_LEN_ERR) {
		printf("SD_BLOCK_LEN_ERR");
	}

	if (response_r1 & SD_OCR_ERASE_SEQ_ERR) {
		printf("SD_ERASE_SEQ_ERR");
	}

	if (response_r1 & SD_OCR_BAD_ERASE_PARAM) {
		printf("SD_BAD_ERASE_PARAM");
	}

	if (response_r1 & SD_OCR_WRITE_PROT_VIOLATION) {
		printf("SD_WRITE_PROT_VIOLATION");
	}

	if (response_r1 & SD_OCR_LOCK_UNLOCK_FAILED) {
		printf("SD_LOCK_UNLOCK_FAILED");
	}

	if (response_r1 & SD_OCR_COM_CRC_FAILED) {
		printf("SD_COM_CRC_FAILED");
	}

	if (response_r1 & SD_OCR_ILLEGAL_CMD) {
		printf("SD_ILLEGAL_CMD");
	}

	if (response_r1 & SD_OCR_CARD_ECC_FAILED) {
		printf("SD_CARD_ECC_FAILED");
	}

	if (response_r1 & SD_OCR_CC_ERROR) {
		printf("SD_CC_ERROR");
	}

	if (response_r1 & SD_OCR_GENERAL_UNKNOWN_ERROR) {
		printf("SD_GENERAL_UNKNOWN_ERROR");
	}

	if (response_r1 & SD_OCR_STREAM_READ_UNDERRUN) {
		printf("SD_STREAM_READ_UNDERRUN");
	}

	if (response_r1 & SD_OCR_STREAM_WRITE_OVERRUN) {
		printf("SD_STREAM_WRITE_OVERRUN");
	}

	if (response_r1 & SD_OCR_CID_CSD_OVERWRIETE) {
		printf("SD_CID_CSD_OVERWRITE");
	}

	if (response_r1 & SD_OCR_WP_ERASE_SKIP) {
		printf("SD_WP_ERASE_SKIP");
	}

	if (response_r1 & SD_OCR_CARD_ECC_DISABLED) {
		printf("SD_CARD_ECC_DISABLED");
	}

	if (response_r1 & SD_OCR_ERASE_RESET) {
		printf("SD_ERASE_RESET");
	}

	if (response_r1 & SD_OCR_AKE_SEQ_ERROR) {
		printf("SD_AKE_SEQ_ERROR");
	}

}

#endif
