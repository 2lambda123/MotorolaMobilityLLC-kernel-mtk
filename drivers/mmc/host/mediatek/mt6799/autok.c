/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/time.h>
#include <linux/delay.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>

#include <linux/completion.h>
#include <linux/scatterlist.h>

#include "autok.h"
#include "mtk_sd.h"

#define AUTOK_VERSION                   (0x16081714)
#define AUTOK_CMD_TIMEOUT               (HZ / 10) /* 100ms */
#define AUTOK_DAT_TIMEOUT               (HZ * 3) /* 1s x 3 */
#define MSDC_FIFO_THD_1K                (1024)
#define TUNE_TX_CNT                     (100)
#define CHECK_QSR                       (0x800D)
#define TUNE_DATA_TX_ADDR               (0x358000)
/* Use negative value to represent address from end of device,
 * 33 blks used by SGPT & 32768 blks used by flashinfo immediate before SGPT
 */
#define TUNE_DATA_TX_ADDR_AT_DEV_END    (-33-32768)
#define CMDQ
#define AUTOK_LATCH_CK_EMMC_TUNE_TIMES  (10) /* 5.0IP eMMC 1KB fifo ZIZE */
#define AUTOK_LATCH_CK_SDIO_TUNE_TIMES  (20) /* 4.5IP 1KB fifo CMD19 need send 20 times  */
#define AUTOK_LATCH_CK_SD_TUNE_TIMES    (20) /* 4.5IP 1KB fifo CMD19 need send 20 times  */
#define AUTOK_CMD_TIMES                 (20)
#define AUTOK_TUNING_INACCURACY         (3) /* scan result may find xxxxooxxx */
#define AUTOK_MARGIN_THOLD              (5)
#define AUTOK_BD_WIDTH_REF              (3)

#define AUTOK_READ                      0
#define AUTOK_WRITE                     1

#define AUTOK_FINAL_CKGEN_SEL           (0)
#define SCALE_TA_CNTR                   (8)
#define SCALE_CMD_RSP_TA_CNTR           (8)
#define SCALE_WDAT_CRC_TA_CNTR          (8)
#define SCALE_INT_DAT_LATCH_CK_SEL      (8)
#define SCALE_INTERNAL_DLY_CNTR         (32)
#define SCALE_PAD_DAT_DLY_CNTR          (32)

#define TUNING_INACCURACY (2)

/* autok platform specific setting */
#define AUTOK_CKGEN_VALUE                       (0)
#define AUTOK_CMD_LATCH_EN_HS400_PORT0_VALUE    (3)
#define AUTOK_CRC_LATCH_EN_HS400_PORT0_VALUE    (3)
#define AUTOK_CMD_LATCH_EN_DDR208_PORT3_VALUE   (3)
#define AUTOK_CRC_LATCH_EN_DDR208_PORT3_VALUE   (3)
#define AUTOK_CMD_LATCH_EN_HS200_PORT0_VALUE    (2)
#define AUTOK_CRC_LATCH_EN_HS200_PORT0_VALUE    (2)
#define AUTOK_CMD_LATCH_EN_SDR104_PORT1_VALUE   (2)
#define AUTOK_CRC_LATCH_EN_SDR104_PORT1_VALUE   (2)
#define AUTOK_CMD_LATCH_EN_HS_VALUE             (1)
#define AUTOK_CRC_LATCH_EN_HS_VALUE             (1)
#define AUTOK_CMD_TA_VALUE                      (0)
#define AUTOK_CRC_TA_VALUE                      (0)
#define AUTOK_BUSY_MA_VALUE                     (1)

/* autok msdc TX init setting */
#define AUTOK_MSDC0_HS400_CLKTXDLY            0
#define AUTOK_MSDC0_HS400_CMDTXDLY            0
#define AUTOK_MSDC0_HS400_DAT0TXDLY           0
#define AUTOK_MSDC0_HS400_DAT1TXDLY           0
#define AUTOK_MSDC0_HS400_DAT2TXDLY           0
#define AUTOK_MSDC0_HS400_DAT3TXDLY           0
#define AUTOK_MSDC0_HS400_DAT4TXDLY           0
#define AUTOK_MSDC0_HS400_DAT5TXDLY           0
#define AUTOK_MSDC0_HS400_DAT6TXDLY           0
#define AUTOK_MSDC0_HS400_DAT7TXDLY           0
#define AUTOK_MSDC0_HS400_TXSKEW              1

#define AUTOK_MSDC0_DDR50_DDRCKD              1
#define AUTOK_MSDC_DDRCKD                     0

#define AUTOK_MSDC0_CLKTXDLY                  0
#define AUTOK_MSDC0_CMDTXDLY                  0
#define AUTOK_MSDC0_DAT0TXDLY                 0
#define AUTOK_MSDC0_DAT1TXDLY                 0
#define AUTOK_MSDC0_DAT2TXDLY                 0
#define AUTOK_MSDC0_DAT3TXDLY                 0
#define AUTOK_MSDC0_DAT4TXDLY                 0
#define AUTOK_MSDC0_DAT5TXDLY                 0
#define AUTOK_MSDC0_DAT6TXDLY                 0
#define AUTOK_MSDC0_DAT7TXDLY                 0

#define AUTOK_MSDC0_TXSKEW                    0

#define AUTOK_MSDC1_CLK_TX_VALUE              0
#define AUTOK_MSDC1_CLK_SDR104_TX_VALUE       0

#define AUTOK_MSDC2_CLK_TX_VALUE              0

#define AUTOK_MSDC3_SDIO_PLUS_CLKTXDLY        0
#define AUTOK_MSDC3_SDIO_PLUS_CMDTXDLY        0
#define AUTOK_MSDC3_SDIO_PLUS_DAT0TXDLY       0
#define AUTOK_MSDC3_SDIO_PLUS_DAT1TXDLY       0
#define AUTOK_MSDC3_SDIO_PLUS_DAT2TXDLY       0
#define AUTOK_MSDC3_SDIO_PLUS_DAT3TXDLY       0

#define PORT0_PB0_RD_DAT_SEL_VALID
#define PORT1_PB0_RD_DAT_SEL_VALID
#define PORT3_PB0_RD_DAT_SEL_VALID

enum TUNE_TYPE {
	TUNE_CMD = 0,
	TUNE_DATA,
	TUNE_LATCH_CK,
	TUNE_SDIO_PLUS,
};

#define autok_msdc_retry(expr, retry, cnt) \
	do { \
		int backup = cnt; \
		while (retry) { \
			if (!(expr)) \
				break; \
			if (cnt-- == 0) { \
				retry--; cnt = backup; \
			} \
		} \
	WARN_ON(retry == 0); \
} while (0)

#define autok_msdc_reset() \
	do { \
		int retry = 3, cnt = 1000; \
		MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_RST); \
		/* ensure reset operation be sequential  */ \
		mb(); \
		autok_msdc_retry(MSDC_READ32(MSDC_CFG) & MSDC_CFG_RST, retry, cnt); \
	} while (0)

#define wait_cond_tmo(cond, tmo) \
	do { \
		unsigned long timeout = jiffies + tmo; \
		while (1) { \
			if ((cond) || (tmo == 0)) \
				break; \
			if (time_after(jiffies, timeout)) \
				tmo = 0; \
		} \
	} while (0)

#define msdc_clear_fifo() \
	do { \
		int retry = 5, cnt = 1000; \
		MSDC_SET_BIT32(MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		/* ensure fifo clear operation be sequential  */ \
		mb(); \
		autok_msdc_retry(MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, retry, cnt); \
	} while (0)

struct AUTOK_PARAM_RANGE {
	unsigned int start;
	unsigned int end;
};

struct AUTOK_PARAM_INFO {
	struct AUTOK_PARAM_RANGE range;
	char *param_name;
};

struct BOUND_INFO {
	unsigned int Bound_Start;
	unsigned int Bound_End;
	unsigned int Bound_width;
	bool is_fullbound;
};

#define BD_MAX_CNT 4	/* Max Allowed Boundary Number */
struct AUTOK_SCAN_RES {
	/* Bound info record, currently only allow max to 2 bounds exist,
	 * but in extreme case, may have 4 bounds
	 */
	struct BOUND_INFO bd_info[BD_MAX_CNT];
	/* Bound cnt record, must be in rang [0,3] */
	unsigned int bd_cnt;
	/* Full boundary cnt record */
	unsigned int fbd_cnt;
};

struct AUTOK_REF_INFO {
	/* inf[0] - rising edge res, inf[1] - falling edge res */
	struct AUTOK_SCAN_RES scan_info[2];
	/* optimised sample edge select */
	unsigned int opt_edge_sel;
	/* optimised dly cnt sel */
	unsigned int opt_dly_cnt;
	/* 1clk cycle equal how many delay cell cnt, if cycle_cnt is 0,
	 * that is cannot calc cycle_cnt by current Boundary info
	 */
	unsigned int cycle_cnt;
};

enum AUTOK_SCAN_WIN {
	CMD_RISE,
	CMD_FALL,
	DAT_RISE,
	DAT_FALL,
	DS_WIN,
};
unsigned int autok_debug_level = AUTOK_DBG_RES;

const struct AUTOK_PARAM_INFO autok_param_info[] = {
	{{0, 1}, "CMD_EDGE"},
	{{0, 1}, "CMD_FIFO_EDGE"},
	{{0, 1}, "RDATA_EDGE"},         /* async fifo mode Pad dat edge must fix to 0 */
	{{0, 1}, "RD_FIFO_EDGE"},
	{{0, 1}, "WD_FIFO_EDGE"},

	{{0, 31}, "CMD_RD_D_DLY1"},     /* Cmd Pad Tune Data Phase */
	{{0, 1}, "CMD_RD_D_DLY1_SEL"},
	{{0, 31}, "CMD_RD_D_DLY2"},
	{{0, 1}, "CMD_RD_D_DLY2_SEL"},

	{{0, 31}, "DAT_RD_D_DLY1"},     /* Data Pad Tune Data Phase */
	{{0, 1}, "DAT_RD_D_DLY1_SEL"},
	{{0, 31}, "DAT_RD_D_DLY2"},
	{{0, 1}, "DAT_RD_D_DLY2_SEL"},

	{{0, 7}, "INT_DAT_LATCH_CK"},   /* Latch CK Delay for data read when clock stop */

	{{0, 31}, "EMMC50_DS_Z_DLY1"},	/* eMMC50 Related tuning param */
	{{0, 1}, "EMMC50_DS_Z_DLY1_SEL"},
	{{0, 31}, "EMMC50_DS_Z_DLY2"},
	{{0, 1}, "EMMC50_DS_Z_DLY2_SEL"},
	{{0, 31}, "EMMC50_DS_ZDLY_DLY"},

	/* ================================================= */
	/* Timming Related Mux & Common Setting Config */
	{{0, 1}, "READ_DATA_SMPL_SEL"},         /* all data line path share sample edge */
	{{0, 1}, "WRITE_DATA_SMPL_SEL"},
	{{0, 1}, "DATA_DLYLINE_SEL"},           /* clK tune all data Line share dly */
	{{0, 1}, "MSDC_WCRC_ASYNC_FIFO_SEL"},   /* data tune mode select */
	{{0, 1}, "MSDC_RESP_ASYNC_FIFO_SEL"},   /* data tune mode select */
	/* eMMC50 Function Mux */
	{{0, 1}, "EMMC50_WDATA_MUX_EN"},        /* write path switch to emmc45 */
	{{0, 1}, "EMMC50_CMD_MUX_EN"},          /* response path switch to emmc45 */
	{{0, 1}, "EMMC50_CMD_RESP_LATCH"},
	{{0, 1}, "EMMC50_WDATA_EDGE"},
	/* Common Setting Config */
	{{0, 31}, "CKGEN_MSDC_DLY_SEL"},
	{{1, 7}, "CMD_RSP_TA_CNTR"},
	{{1, 7}, "WRDAT_CRCS_TA_CNTR"},
	{{0, 31}, "PAD_CLK_TXDLY_AUTOK"},       /* tx clk dly fix to 0 for HQA res */
};

/**********************************************************
* AutoK Basic Interface Implenment                        *
**********************************************************/
static int autok_send_tune_cmd(struct msdc_host *host, unsigned int opcode, enum TUNE_TYPE tune_type_value)
{
	void __iomem *base = host->base;
	unsigned int value;
	unsigned int rawcmd = 0;
	unsigned int arg = 0;
	unsigned int sts = 0;
	unsigned int wints = 0;
	unsigned long tmo = 0;
	unsigned long write_tmo = 0;
	unsigned int left = 0;
	unsigned int fifo_have = 0;
	unsigned int fifo_1k_cnt = 0;
	unsigned int i = 0;
	int ret = E_RESULT_PASS;

	switch (opcode) {
	case MMC_SEND_EXT_CSD:
		rawcmd =  (512 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (8);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_STOP_TRANSMISSION:
		rawcmd = (1 << 14)  | (7 << 7) | (12);
		arg = 0;
		break;
	case MMC_SEND_STATUS:
		rawcmd = (1 << 7) | (13);
		arg = (1 << 16);
		break;
	case CHECK_QSR:
		rawcmd = (1 << 7) | (13);
		arg = (1 << 16) | (1 << 15);
		break;
	case MMC_READ_SINGLE_BLOCK:
		left = 512;
		rawcmd =  (512 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (17);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SEND_TUNING_BLOCK:
		left = 64;
		rawcmd =  (64 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (19);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SEND_TUNING_BLOCK_HS200:
		left = 128;
		rawcmd =  (128 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (21);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_WRITE_BLOCK:
		rawcmd =  (512 << 16) | (1 << 13) | (1 << 11) | (1 << 7) | (24);
		if (host->mmc && host->mmc->card)
			arg = host->mmc->card->ext_csd.sectors
				+ TUNE_DATA_TX_ADDR_AT_DEV_END;
		else
			arg = TUNE_DATA_TX_ADDR;
		break;
	case SD_IO_RW_DIRECT:
		rawcmd = (1 << 7) | (52);
		arg = (0x80000000) | (0 << 28) | (SDIO_CCCR_ABORT << 9) | (0);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case SD_IO_RW_EXTENDED:
		rawcmd =  (4 << 16) | (1 << 13) | (1 << 11) | (1 << 7) | (53);
		arg = (0x80000000) | (0 << 28) | (0xE0 << 9) | (0 << 26) | (0 << 27) | (4);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	}

	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo1 cmd%d goto end...\n", opcode);
		ret |= E_RESULT_FATAL_ERR;
		goto end;
	}

	/* clear fifo */
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA)
		|| (tune_type_value == TUNE_SDIO_PLUS)) {
		if ((tune_type_value == TUNE_CMD) && (host->hw->host_function == MSDC_EMMC))
			MSDC_WRITE32(MSDC_INT, MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR);
		else {
			autok_msdc_reset();
			msdc_clear_fifo();
			MSDC_WRITE32(MSDC_INT, 0xffffffff);
		}
	}

	/* start command */
	MSDC_WRITE32(SDC_ARG, arg);
	MSDC_WRITE32(SDC_CMD, rawcmd);

	/* wait interrupt status */
	wints = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	tmo = AUTOK_CMD_TIMEOUT;
	wait_cond_tmo(((sts = MSDC_READ32(MSDC_INT)) & wints), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]CMD%d wait int tmo\r\n", opcode);
		ret |= E_RESULT_CMD_TMO;
		goto end;
	}

	MSDC_WRITE32(MSDC_INT, (sts & wints));
	if (sts == 0) {
		ret |= E_RESULT_CMD_TMO;
		goto end;
	}

	if (sts & MSDC_INT_CMDRDY) {
		if (tune_type_value == TUNE_CMD) {
			ret |= E_RESULT_PASS;
			goto end;
		}
	} else if (sts & MSDC_INT_RSPCRCERR) {
		ret |= E_RESULT_RSP_CRC;
		if (tune_type_value != TUNE_SDIO_PLUS)
			goto end;
	} else if (sts & MSDC_INT_CMDTMO) {
		AUTOK_RAWPRINT("[AUTOK]CMD%d HW tmo\r\n", opcode);
		ret |= E_RESULT_CMD_TMO;
		if (tune_type_value != TUNE_SDIO_PLUS)
			goto end;
	}

	if ((tune_type_value != TUNE_LATCH_CK) && (tune_type_value != TUNE_DATA)
		&& (tune_type_value != TUNE_SDIO_PLUS))
		goto skip_tune_latch_ck_and_tune_data;
	tmo = jiffies + AUTOK_DAT_TIMEOUT;
	while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY) && (tmo != 0)) {
		if (time_after(jiffies, tmo))
			tmo = 0;
		if (tune_type_value == TUNE_LATCH_CK) {
			fifo_have = msdc_rxfifocnt();
			if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) || (opcode == MMC_READ_SINGLE_BLOCK)
				|| (opcode == MMC_SEND_EXT_CSD) || (opcode == MMC_SEND_TUNING_BLOCK)) {
				MSDC_SET_FIELD(MSDC_DBG_SEL, 0xffff << 0, 0x0b);
				MSDC_GET_FIELD(MSDC_DBG_OUT, 0x7ff << 0, fifo_1k_cnt);
				if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K) && (fifo_have >= MSDC_FIFO_SZ)) {
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
				}
			}
		} else if ((tune_type_value == TUNE_DATA) && (opcode == MMC_WRITE_BLOCK)) {
			for (i = 0; i < 64; i++) {
				MSDC_WRITE32(MSDC_TXDATA, 0x5af00fa5);
				MSDC_WRITE32(MSDC_TXDATA, 0x33cc33cc);
			}

			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), write_tmo);
			if (write_tmo == 0) {
				AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo2 while write cmd%d goto end...\n", opcode);
				ret |= E_RESULT_FATAL_ERR;
				goto end;
			}
		} else if ((tune_type_value == TUNE_SDIO_PLUS) && (opcode == SD_IO_RW_EXTENDED)) {
			MSDC_WRITE32(MSDC_TXDATA, 0x5a5a5a5a);

			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), write_tmo);
			if (write_tmo == 0) {
				AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo2 while write cmd%d goto end...\n", opcode);
				ret |= E_RESULT_FATAL_ERR;
				goto end;
			}
		}
	}
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo3 cmd%d goto end...\n", opcode);
		ret |= E_RESULT_FATAL_ERR;
		goto end;
	}

	sts = MSDC_READ32(MSDC_INT);
	wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	if (sts) {
		/* clear status */
		MSDC_WRITE32(MSDC_INT, (sts & wints));
		if (sts & MSDC_INT_XFER_COMPL)
			ret |= E_RESULT_PASS;
		if (MSDC_INT_DATCRCERR & sts)
			ret |= E_RESULT_DAT_CRC;
		if (MSDC_INT_DATTMO & sts)
			ret |= E_RESULT_DAT_TMO;
	}

skip_tune_latch_ck_and_tune_data:
	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo4 cmd%d goto end...\n", opcode);
		ret |= E_RESULT_FATAL_ERR;
		goto end;
	}
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA))
		msdc_clear_fifo();

end:
	if (opcode == MMC_STOP_TRANSMISSION) {
		tmo = AUTOK_DAT_TIMEOUT;
		wait_cond_tmo(((MSDC_READ32(MSDC_PS) & 0x10000) == 0x10000), tmo);
		if (tmo == 0) {
			AUTOK_RAWPRINT("[AUTOK]DTA0 busy tmo cmd%d goto end...\n", opcode);
			ret |= E_RESULT_FATAL_ERR;
		}
	}

	return ret;
}

static int autok_simple_score64(char *res_str64, u64 result64)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result64 == 0) {
		/* maybe result is 0 */
		strcpy(res_str64, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
		return 64;
	}
	if (result64 == 0xFFFFFFFFFFFFFFFF) {
		strcpy(res_str64,
		       "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
		return 0;
	}

	/* calc continue zero number */
	while (bit < 64) {
		if (result64 & ((u64) (1LL << bit))) {
			res_str64[bit] = 'X';
			bit++;
			if (old < num)
				old = num;
			num = 0;
			continue;
		}
		res_str64[bit] = 'O';
		bit++;
		num++;
	}
	if (num > old)
		old = num;

	res_str64[64] = '\0';
	return old;
}

enum {
	RD_SCAN_NONE,
	RD_SCAN_PAD_BOUND_S,
	RD_SCAN_PAD_BOUND_E,
	RD_SCAN_PAD_MARGIN,
};

static int autok_check_scan_res64(u64 rawdat, struct AUTOK_SCAN_RES *scan_res, unsigned int bd_filter)
{
	unsigned int bit;
	unsigned int filter = 4;
	struct BOUND_INFO *pBD = (struct BOUND_INFO *)scan_res->bd_info;
	unsigned int RawScanSta = RD_SCAN_NONE;

	for (bit = 0; bit < 64; bit++) {
		if (rawdat & (1LL << bit)) {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				pBD->Bound_Start = 0;
				pBD->Bound_width = 1;
				scan_res->bd_cnt += 1;
				break;
			case RD_SCAN_PAD_MARGIN:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				pBD->Bound_Start = bit;
				pBD->Bound_width = 1;
				scan_res->bd_cnt += 1;
				break;
			case RD_SCAN_PAD_BOUND_E:
				if ((filter) && ((bit - pBD->Bound_End) <= bd_filter)) {
					AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
					       "[AUTOK]WARN: Try to filter the holes on raw data \r\n");
					RawScanSta = RD_SCAN_PAD_BOUND_S;

					pBD->Bound_width += (bit - pBD->Bound_End);
					pBD->Bound_End = 0;
					filter--;

					/* update full bound info */
					if (pBD->is_fullbound) {
						pBD->is_fullbound = 0;
						scan_res->fbd_cnt -= 1;
					}
				} else {
					/* No filter Check and Get the next boundary information */
					RawScanSta = RD_SCAN_PAD_BOUND_S;
					pBD++;
					pBD->Bound_Start = bit;
					pBD->Bound_width = 1;
					scan_res->bd_cnt += 1;
					if (scan_res->bd_cnt > BD_MAX_CNT) {
						AUTOK_RAWPRINT
						    ("[AUTOK]WARN: more than %d Boundary Exist\r\n",
						     BD_MAX_CNT);
						/* return -1; */
					}
				}
				break;
			case RD_SCAN_PAD_BOUND_S:
				pBD->Bound_width++;
				break;
			default:
				break;
			}
		} else {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_MARGIN;
				break;
			case RD_SCAN_PAD_BOUND_S:
				RawScanSta = RD_SCAN_PAD_BOUND_E;
				pBD->Bound_End = bit - 1;
				/* update full bound info */
				if (pBD->Bound_Start > 0) {
					pBD->is_fullbound = 1;
					scan_res->fbd_cnt += 1;
				}
				break;
			case RD_SCAN_PAD_MARGIN:
			case RD_SCAN_PAD_BOUND_E:
			default:
				break;
			}
		}
	}
	if ((pBD->Bound_End == 0) && (pBD->Bound_width != 0))
		pBD->Bound_End = pBD->Bound_Start + pBD->Bound_width - 1;

	return 0;
}

static int autok_pad_dly_sel(struct AUTOK_REF_INFO *pInfo)
{
	struct AUTOK_SCAN_RES *pBdInfo_R = NULL; /* scan result @ rising edge */
	struct AUTOK_SCAN_RES *pBdInfo_F = NULL; /* scan result @ falling edge */
	struct BOUND_INFO *pBdPrev = NULL; /* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdNext = NULL; /* Save the second boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdTmp = NULL;
	unsigned int FBound_Cnt_R = 0;	/* Full Boundary count */
	unsigned int Bound_Cnt_R = 0;
	unsigned int Bound_Cnt_F = 0;
	unsigned int cycle_cnt = 64;
	int uBD_mid_prev = 0;
	int uBD_mid_next = 0;
	int uBD_width = 3;
	int uDlySel_F = 0;
	int uDlySel_R = 0;
	int uMgLost_F = 0; /* for falling edge margin compress */
	int uMgLost_R = 0; /* for rising edge margin compress */
	unsigned int i;
	unsigned int ret = 0;

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	FBound_Cnt_R = pBdInfo_R->fbd_cnt;
	Bound_Cnt_R = pBdInfo_R->bd_cnt;
	Bound_Cnt_F = pBdInfo_F->bd_cnt;

	switch (FBound_Cnt_R) {
	case 4:	/* SSSS Corner may cover 2~3T */
	case 3:
		AUTOK_RAWPRINT("[ATUOK]Warning: Too Many Full boundary count:%d\r\n", FBound_Cnt_R);
	case 2:	/* mode_1 : 2 full boudary */
		for (i = 0; i < BD_MAX_CNT; i++) {
			if (pBdInfo_R->bd_info[i].is_fullbound) {
				if (pBdPrev == NULL) {
					pBdPrev = &(pBdInfo_R->bd_info[i]);
				} else {
					pBdNext = &(pBdInfo_R->bd_info[i]);
					break;
				}
			}
		}

		if (pBdPrev && pBdNext) {
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
			/* while in 2 full bound case, bd_width calc */
			uBD_width = (pBdPrev->Bound_width + pBdNext->Bound_width) / 2;
			cycle_cnt = uBD_mid_next - uBD_mid_prev;
			/* delay count sel at rising edge */
			if (uBD_mid_prev >= cycle_cnt / 2) {
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if ((cycle_cnt / 2 - uBD_mid_prev) > AUTOK_MARGIN_THOLD) {
				uDlySel_R = uBD_mid_prev + cycle_cnt / 2;
				uMgLost_R = 0;
			} else {
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			}
			/* delay count sel at falling edge */
			pBdTmp = &(pBdInfo_R->bd_info[0]);
			if (pBdTmp->is_fullbound) {
				/* ooooxxxooooooxxxooo */
				uDlySel_F = uBD_mid_prev;
				uMgLost_F = 0;
			} else {
				/* xooooooxxxoooooooxxxoo */
				if (pBdTmp->Bound_End > uBD_width / 2) {
					uDlySel_F = (pBdTmp->Bound_End) - (uBD_width / 2);
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F = (uBD_width / 2) - (pBdTmp->Bound_End);
				}
			}
		} else {
			/* error can not find 2 foull boary */
			AUTOK_RAWPRINT("[AUTOK]error can not find 2 foull boudary @ Mode_1");
			return -1;
		}
		break;

	case 1:	/* rising edge find one full boundary */
		if (Bound_Cnt_R > 1) {
			/* mode_2: 1 full boundary and boundary count > 1 */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_R->bd_info[1]);

			if (pBdPrev->is_fullbound)
				uBD_width = pBdPrev->Bound_width;
			else
				uBD_width = pBdNext->Bound_width;

			if ((pBdPrev->is_fullbound) || (pBdNext->is_fullbound)) {
				if (pBdPrev->Bound_Start > 0)
					cycle_cnt = pBdNext->Bound_Start - pBdPrev->Bound_Start;
				else
					cycle_cnt = pBdNext->Bound_End - pBdPrev->Bound_End;

				/* delay count sel@rising & falling edge */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
					uDlySel_F = uBD_mid_prev;
					uMgLost_F = 0;
					if (uBD_mid_prev >= cycle_cnt / 2) {
						uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
						uMgLost_R = 0;
					} else if ((cycle_cnt / 2 - uBD_mid_prev) >
						   AUTOK_MARGIN_THOLD) {
						uDlySel_R = uBD_mid_prev + cycle_cnt / 2;
						uMgLost_R = 0;
					} else {
						uDlySel_R = 0;
						uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
					}
				} else {
					/* first boundary not full boudary */
					uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
					uDlySel_R = uBD_mid_next - cycle_cnt / 2;
					uMgLost_R = 0;
					if (pBdPrev->Bound_End > uBD_width / 2) {
						uDlySel_F = (pBdPrev->Bound_End) - (uBD_width / 2);
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F = (uBD_width / 2) - (pBdPrev->Bound_End);
					}
				}
			} else {
				return -1; /* full bound must in first 2 boundary */
			}
		} else if (Bound_Cnt_F > 0) {
			/* mode_3: 1 full boundary and only one boundary exist @rising edge */
			pBdPrev = &(pBdInfo_R->bd_info[0]); /* this boundary is full bound */
			pBdNext = &(pBdInfo_F->bd_info[0]);
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdNext->Bound_Start == 0) {
				cycle_cnt = (pBdPrev->Bound_End - pBdNext->Bound_End) * 2;
			} else if (pBdNext->Bound_End == 63) {
				cycle_cnt = (pBdNext->Bound_Start - pBdPrev->Bound_Start) * 2;
			} else {
				uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;

				if (uBD_mid_next > uBD_mid_prev)
					cycle_cnt = (uBD_mid_next - uBD_mid_prev) * 2;
				else
					cycle_cnt = (uBD_mid_prev - uBD_mid_next) * 2;
			}

			uDlySel_F = uBD_mid_prev;
			uMgLost_F = 0;

			if (uBD_mid_prev >= cycle_cnt / 2) { /* case 1 */
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if (cycle_cnt / 2 - uBD_mid_prev <= AUTOK_MARGIN_THOLD) { /* case 2 */
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			} else if (cycle_cnt / 2 + uBD_mid_prev <= 63) { /* case 3 */
				uDlySel_R = cycle_cnt / 2 + uBD_mid_prev;
				uMgLost_R = 0;
			} else if (32 - uBD_mid_prev <= AUTOK_MARGIN_THOLD) { /* case 4 */
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			} else { /* case 5 */
				uDlySel_R = 63;
				uMgLost_R = uBD_mid_prev + cycle_cnt / 2 - 63;
			}
		} else {
			/* mode_4: falling edge no boundary found & rising edge only one full boundary exist */
			pBdPrev = &(pBdInfo_R->bd_info[0]);	/* this boundary is full bound */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdPrev->Bound_End > (64 - pBdPrev->Bound_Start))
				cycle_cnt = 2 * (pBdPrev->Bound_End + 1);
			else
				cycle_cnt = 2 * (64 - pBdPrev->Bound_Start);

			uDlySel_R = 0xFF;
			uMgLost_R = 0xFF; /* Margin enough donot care margin lost */
			uDlySel_F = uBD_mid_prev;
			uMgLost_F = 0xFF; /* Margin enough donot care margin lost */

			AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
		}
		break;

	case 0:	/* rising edge cannot find full boudary */
		if (Bound_Cnt_R == 2) {
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_F->bd_info[0]); /* this boundary is full bound */

			if (pBdNext->is_fullbound) {
				/* mode_5: rising_edge 2 boundary (not full bound), falling edge 1 full boundary */
				uBD_width = pBdNext->Bound_width;
				cycle_cnt = 2 * (pBdNext->Bound_End - pBdPrev->Bound_End);
				uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
				uDlySel_R = uBD_mid_next;
				uMgLost_R = 0;
				if (pBdPrev->Bound_End >= uBD_width / 2) {
					uDlySel_F = pBdPrev->Bound_End - uBD_width / 2;
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F = uBD_width / 2 - pBdPrev->Bound_End;
				}
			} else {
				/* for falling edge there must be one full boundary between two bounary_mid at rising
				* this is a corner case, falling boundary may  scan miss.
				* xoooooooooooooooox or  xoooooooooooooooox  or xoooooooooooooooox
				* oooooooooooooooooo       xxoooooooooooooooo      ooooooooooooooooox
				*/
				pInfo->cycle_cnt = pBdInfo_R->bd_info[1].Bound_End
				- pBdInfo_R->bd_info[0].Bound_Start;
				if (Bound_Cnt_F == 0) {
					pInfo->opt_edge_sel = 1;
					pInfo->opt_dly_cnt = 0;
				} else {
					pInfo->opt_edge_sel = 0;
					pInfo->opt_dly_cnt = (pBdInfo_R->bd_info[1].Bound_End
						+ pBdInfo_R->bd_info[0].Bound_Start) / 2;
				}
				return ret;
			}
		} else if (Bound_Cnt_R == 1) {
			if (Bound_Cnt_F > 1) {
				/* when rising_edge have only one boundary (not full bound),
				* falling edge should not more than 1Bound exist
				* this is a corner case, rising boundary may  scan miss.
				* xooooooooooooooooo
				* oooxooooooooxooooo
				*/
				pInfo->cycle_cnt = (pBdInfo_F->bd_info[1].Bound_End
				+ pBdInfo_F->bd_info[1].Bound_Start) / 2
				- (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2;
				pInfo->opt_edge_sel = 1;
				pInfo->opt_dly_cnt = ((pBdInfo_F->bd_info[1].Bound_End
				+ pBdInfo_F->bd_info[1].Bound_Start) / 2
				+ (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2) / 2;
				return ret;
			} else if (Bound_Cnt_F == 1) {
				/* mode_6: rising edge only 1 boundary (not full Bound)
				 * & falling edge have only 1 bound too
				 */
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				pBdNext = &(pBdInfo_F->bd_info[0]);
				if (pBdNext->is_fullbound) {
					uBD_width = pBdNext->Bound_width;
				} else {
					if (pBdNext->Bound_width > pBdPrev->Bound_width)
						uBD_width = (pBdNext->Bound_width + 1);
					else
						uBD_width = (pBdPrev->Bound_width + 1);

					if (uBD_width < AUTOK_BD_WIDTH_REF)
						uBD_width = AUTOK_BD_WIDTH_REF;
				} /* Boundary width calc done */

				if (pBdPrev->Bound_Start == 0) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_Start == 0) {
						/* Current Desing Not Allowed
						* this is a corner case, boundary may  scan error.
						* xooooooooooooooooo
						* xooooooooooooooooo
						*/
						pInfo->cycle_cnt = 2 * (64 - (pBdInfo_R->bd_info[0].Bound_End
						+ pBdInfo_R->bd_info[0].Bound_Start) / 2);
						pInfo->opt_edge_sel = 0;
						pInfo->opt_dly_cnt = 31;
						return ret;
					}

					cycle_cnt =
					    (pBdNext->Bound_Start - pBdPrev->Bound_End +
					     uBD_width) * 2;
				} else if (pBdPrev->Bound_End == 63) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_End == 63) {
						/* Current Desing Not Allowed
						* this is a corner case, boundary may  scan error.
						* ooooooooooooooooox
						* ooooooooooooooooox
						*/
						pInfo->cycle_cnt = pBdInfo_R->bd_info[0].Bound_End
						+ pBdInfo_R->bd_info[0].Bound_Start;
						pInfo->opt_edge_sel = 0;
						pInfo->opt_dly_cnt = 31;
						return ret;
					}

					cycle_cnt =
					    (pBdPrev->Bound_Start - pBdNext->Bound_End +
					     uBD_width) * 2;
				} /* cycle count calc done */

				/* calc optimise delay count */
				if (pBdPrev->Bound_Start == 0) {
					/* falling edge sel */
					if (pBdPrev->Bound_End >= uBD_width / 2) {
						uDlySel_F = pBdPrev->Bound_End - uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F = uBD_width / 2 - pBdPrev->Bound_End;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_End - uBD_width / 2 + cycle_cnt / 2 > 63) {
						uDlySel_R = 63;
						uMgLost_R =
						    pBdPrev->Bound_End - uBD_width / 2 +
						    cycle_cnt / 2 - 63;
					} else {
						uDlySel_R =
						    pBdPrev->Bound_End - uBD_width / 2 +
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else if (pBdPrev->Bound_End == 63) {
					/* falling edge sel */
					if (pBdPrev->Bound_Start + uBD_width / 2 < 63) {
						uDlySel_F = pBdPrev->Bound_Start + uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 63;
						uMgLost_F =
						    pBdPrev->Bound_Start + uBD_width / 2 - 63;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_Start + uBD_width / 2 - cycle_cnt / 2 < 0) {
						uDlySel_R = 0;
						uMgLost_R =
						    cycle_cnt / 2 - (pBdPrev->Bound_Start +
								     uBD_width / 2);
					} else {
						uDlySel_R =
						    pBdPrev->Bound_Start + uBD_width / 2 -
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else {
					return -1;
				}
			} else if (Bound_Cnt_F == 0) {
				/* mode_7: rising edge only one bound (not full), falling no boundary */
				cycle_cnt = 128;
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				if (pBdPrev->Bound_Start == 0) {
					uDlySel_F = 0;
					uDlySel_R = 63;
				} else if (pBdPrev->Bound_End == 63) {
					uDlySel_F = 63;
					uDlySel_R = 0xFF;
				} else {
					return -1;
				}
				uMgLost_F = 0xFF;
				uMgLost_R = 0xFF;

				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
			}
		} else if (Bound_Cnt_R == 0) { /* Rising Edge No Boundary found */
			if (Bound_Cnt_F > 1) {
				/* falling edge not allowed two boundary Exist for this case
				* this is a corner case,rising boundary may  scan miss.
				* oooooooooooooooooo
				* oooxooooooooxooooo
				*/
				pInfo->cycle_cnt = (pBdInfo_F->bd_info[1].Bound_End
				+ pBdInfo_F->bd_info[1].Bound_Start) / 2
				- (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2;
				pInfo->opt_edge_sel = 0;
				pInfo->opt_dly_cnt = (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2;
				return ret;
			} else if (Bound_Cnt_F > 0) {
				/* mode_8: falling edge have one Boundary exist */
				pBdPrev = &(pBdInfo_F->bd_info[0]);

				/* this boundary is full bound */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev =
					    (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;

					if (pBdPrev->Bound_End > (64 - pBdPrev->Bound_Start))
						cycle_cnt = 2 * (pBdPrev->Bound_End + 1);
					else
						cycle_cnt = 2 * (64 - pBdPrev->Bound_Start);

					uDlySel_R = uBD_mid_prev;
					uMgLost_R = 0xFF;
					uDlySel_F = 0xFF;
					uMgLost_F = 0xFF;
				} else {
					cycle_cnt = 128;

					uDlySel_R = (pBdPrev->Bound_Start == 0) ? 0 : 63;
					uMgLost_R = 0xFF;
					uDlySel_F = 0xFF;
					uMgLost_F = 0xFF;
				}

				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
			} else {
				/* falling edge no boundary exist no need tuning */
				cycle_cnt = 128;
				uDlySel_F = 0;
				uMgLost_F = 0xFF;
				uDlySel_R = 0;
				uMgLost_R = 0xFF;
				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
			}
		} else {
			/* Error if bound_cnt > 3 there must be at least one full boundary exist */
			return -1;
		}
		break;

	default:
		/* warning if boundary count > 4 (from current hw design, this case cannot happen) */
		return -1;
	}

	/* Select Optimised Sample edge & delay count (the small one) */
	pInfo->cycle_cnt = cycle_cnt;
	if (uDlySel_R <= uDlySel_F) {
		pInfo->opt_edge_sel = 0;
		pInfo->opt_dly_cnt = uDlySel_R;
	} else {
		pInfo->opt_edge_sel = 1;
		pInfo->opt_dly_cnt = uDlySel_F;

	}
	AUTOK_RAWPRINT("[AUTOK]Analysis Result: 1T = %d\r\n", cycle_cnt);
	return ret;
}

#if SINGLE_EDGE_ONLINE_TUNE
static int
autok_pad_dly_sel_single_edge(struct AUTOK_SCAN_RES *pInfo, unsigned int cycle_cnt_ref,
			      unsigned int *pDlySel)
{
	struct BOUND_INFO *pBdPrev = NULL; /* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdNext = NULL; /* Save the second boundary info for calc optimised dly count */
	unsigned int Bound_Cnt = 0;
	unsigned int uBD_mid_prev = 0;
	int uDlySel = 0;
	int uMgLost = 0;
	unsigned int ret = 0;

	Bound_Cnt = pInfo->bd_cnt;
	if (Bound_Cnt > 1) {
		pBdPrev = &(pInfo->bd_info[0]);
		pBdNext = &(pInfo->bd_info[1]);
		if (!(pBdPrev->is_fullbound)) {
			/* mode_1: at least 2 Bound and Boud0_Start == 0 */
			uDlySel = (pBdPrev->Bound_End + pBdNext->Bound_Start) / 2;
			uMgLost = (uDlySel > 31) ? (uDlySel - 31) : 0;
			uDlySel = (uDlySel > 31) ? 31 : uDlySel;

		} else {
			/* mode_2: at least 2 Bound found and Bound0_Start != 0 */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (uBD_mid_prev >= cycle_cnt_ref / 2) {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			} else if (cycle_cnt_ref / 2 - uBD_mid_prev < AUTOK_MARGIN_THOLD) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else {
				uDlySel = (pBdPrev->Bound_End + pBdNext->Bound_Start) / 2;
				if ((uDlySel > 31) && (uDlySel - 31 < AUTOK_MARGIN_THOLD)) {
					uDlySel = 31;
					uMgLost = uDlySel - 31;
				} else {
					/* uDlySel = uDlySel; */
					uMgLost = 0;
				}
			}
		}
	} else if (Bound_Cnt > 0) {
		/* only one bound fond */
		pBdPrev = &(pInfo->bd_info[0]);
		if (pBdPrev->is_fullbound) {
			/* mode_3: Bound_S != 0 */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (uBD_mid_prev >= cycle_cnt_ref / 2) {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			} else if (cycle_cnt_ref / 2 - uBD_mid_prev < AUTOK_MARGIN_THOLD) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if ((uBD_mid_prev > 31 - AUTOK_MARGIN_THOLD)
				   || (pBdPrev->Bound_Start >= 16)) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if (uBD_mid_prev + cycle_cnt_ref / 2 <= 63) {
				/* Left Margin not enough must need to select the right side */
				uDlySel = uBD_mid_prev + cycle_cnt_ref / 2;
				uMgLost = 0;
			} else {
				uDlySel = 63;
				uMgLost = uBD_mid_prev + cycle_cnt_ref / 2 - 63;
			}
		} else if (pBdPrev->Bound_Start == 0) {
			/* mode_4 : Only one Boud and Boud_S = 0  (Currently 1T nearly equal 64 ) */

			/* May not exactly by for Cycle_Cnt enough can don't care */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (pBdPrev->Bound_Start + cycle_cnt_ref / 2 >= 31) {
				uDlySel = 31;
				uMgLost = uBD_mid_prev + cycle_cnt_ref / 2 - 31;
			} else {
				uDlySel = uBD_mid_prev + cycle_cnt_ref / 2;
				uMgLost = 0;
			}
		} else {
			/* mode_5: Only one Boud and Boud_E = 64 */

			/* May not exactly by for Cycle_Cnt enough can don't care */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (pBdPrev->Bound_Start < cycle_cnt_ref / 2) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if (uBD_mid_prev - cycle_cnt_ref / 2 > 31) {
				uDlySel = 31;
				uMgLost = uBD_mid_prev - cycle_cnt_ref / 2 - 31;
			} else {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			}
		}
	} else { /*mode_6: no bound foud */
		uDlySel = 31;
		uMgLost = 0xFF;
	}
	*pDlySel = uDlySel;
	if (uDlySel > 31) {
		AUTOK_RAWPRINT
		    ("[AUTOK]Warning Dly Sel %d > 31 easily effected by Voltage Swing\r\n",
		     uDlySel);
	}

	return ret;
}
#endif

static int autok_ds_dly_sel(struct AUTOK_SCAN_RES *pInfo, unsigned int *pDlySel, u8 *param)
{
	unsigned int ret = 0;
	int uDlySel = 0;
	unsigned int Bound_Cnt = pInfo->bd_cnt;

	if (pInfo->fbd_cnt > 0) {
		/* no more than 2 boundary exist */
		AUTOK_RAWPRINT("[AUTOK]Error: Scan DS Not allow Full boundary Occurs!\r\n");
		return -1;
	}

	if (Bound_Cnt > 1) {
		/* bound count == 2 */
		uDlySel = (pInfo->bd_info[0].Bound_End + pInfo->bd_info[1].Bound_Start) / 2;
	} else if (Bound_Cnt > 0) {
		/* bound count == 1 */
		if (pInfo->bd_info[0].Bound_Start == 0) {
			if (pInfo->bd_info[0].Bound_End > 31) {
				uDlySel = (pInfo->bd_info[0].Bound_End + 64) / 2;
				AUTOK_RAWPRINT("[AUTOK]Warning: DS Delay not in range 0~31!\r\n");
			} else {
				uDlySel = (pInfo->bd_info[0].Bound_End + 31) / 2;
			}
		} else
			uDlySel = pInfo->bd_info[0].Bound_Start / 2;
	} else {
		/* bound count == 0 */
		uDlySel = 16;
	}
	*pDlySel = uDlySel;

	return ret;
}

/*************************************************************************
* FUNCTION
*  msdc_autok_adjust_param
*
* DESCRIPTION
*  This function for auto-K, adjust msdc parameter
*
* PARAMETERS
*    host: msdc host manipulator pointer
*    param: enum of msdc parameter
*    value: value of msdc parameter
*    rw: AUTOK_READ/AUTOK_WRITE
*
* RETURN VALUES
*    error code: 0 success,
*               -1 parameter input error
*               -2 read/write fail
*               -3 else error
*************************************************************************/
static int msdc_autok_adjust_param(struct msdc_host *host, enum AUTOK_PARAM param, u32 *value,
				   int rw)
{
	void __iomem *base = host->base;
#if !defined(FPGA_PLATFORM)
	void __iomem *base_top = host->base_top;
#endif
	u32 *reg;
	u32 field = 0;

	switch (param) {
	case READ_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for READ_DATA_SMPL_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_R_D_SMPL_SEL);
		break;
	case WRITE_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for WRITE_DATA_SMPL_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_W_D_SMPL_SEL);
		break;
	case DATA_DLYLINE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (DATA_K_VALUE_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_IOCON;
			field = (u32) (MSDC_IOCON_DDLSEL);
		}
		break;
	case MSDC_DAT_TUNE_SEL:	/* 0-Dat tune 1-CLk tune ; */
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT_TUNE_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_RXDLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_RXDLYSEL);
		}
		break;
	case MSDC_WCRC_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for WCRC_ASYNC_FIFO_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTS);
		break;
	case MSDC_RESP_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for RESP_ASYNC_FIFO_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGRESP);
		break;
	case CMD_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_RSPL);
		break;
	case CMD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_FIFO_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CMD_EDGE_SEL);
		break;
	case RDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for RDATA_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_R_D_SMPL);
		break;
	case RD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for RD_FIFO_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_RD_DAT_SEL);
		break;
	case WD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for WD_FIFO_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTSEDGE);
		break;
	case CMD_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RXDLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_CMDRDLY);
		}
		break;
	case CMD_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY_SEL is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RD_RXDLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_CMDRRDLYSEL);
		}
		break;
	case CMD_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY2 is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RXDLY2);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_CMDRDLY2);
		}
		break;
	case CMD_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY2_SEL is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RD_RXDLY2_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_CMDRRDLY2SEL);
		}
		break;
	case DAT_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_DATRRDLY);
		}
		break;
	case DAT_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT_RD_DLY_SEL is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_DATRRDLYSEL);
		}
		break;
	case DAT_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT_RD_DLY2 is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY2);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2);
		}
		break;
	case DAT_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT_RD_DLY2_SEL is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY2_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2SEL);
		}
		break;
	case INT_DAT_LATCH_CK:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s] Input value(%d) for INT_DAT_LATCH_CK is out of range, it should be [0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_INT_DAT_LATCH_CK_SEL);
		break;
	case CKGEN_MSDC_DLY_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for CKGEN_MSDC_DLY_SEL is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_CKGEN_MSDC_DLY_SEL);
		break;
	case CMD_RSP_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RSP_TA_CNTR is out of range, it should be [0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PB1_CMD_RSP_TA_CNTR);
		break;
	case WRDAT_CRCS_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s] Input value(%d) for WRDAT_CRCS_TA_CNTR is out of range, it should be [0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PB1_WRDAT_CRCS_TA_CNTR);
		break;
	case PAD_CLK_TXDLY_AUTOK:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for PAD_CLK_TXDLY_AUTOK is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_CTL0;
			field = (u32) (PAD_CLK_TXDLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_CLKTXDLY);
		}
		break;
	case EMMC50_WDATA_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_WDATA_MUX_EN is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_SEL);
		break;
	case EMMC50_CMD_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_CMD_MUX_EN is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CMD_RESP_SEL);
		break;
	case EMMC50_CMD_RESP_LATCH:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_CMD_RESP_LATCH is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_PADCMD_LATCHCK);
		break;
	case EMMC50_WDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_WDATA_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_EDGE);
		break;
	case EMMC50_DS_Z_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY1 is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY1);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY1);
		}
		break;
	case EMMC50_DS_Z_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY1_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLYSEL);
		}
		break;
	case EMMC50_DS_Z_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY2 is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY2);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2);
		}
		break;
	case EMMC50_DS_Z_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY2_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY2_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL);
		}
		break;
	case EMMC50_DS_ZDLY_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY3 is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY3);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY3);
		}
		break;
	default:
		pr_debug("[%s] Value of [enum AUTOK_PARAM param] is wrong\n", __func__);
		return -1;
	}

	if (rw == AUTOK_READ)
		MSDC_GET_FIELD(reg, field, *value);
	else if (rw == AUTOK_WRITE) {
		MSDC_SET_FIELD(reg, field, *value);

		if (param == CKGEN_MSDC_DLY_SEL)
			mdelay(1);
	} else {
		pr_debug("[%s] Value of [int rw] is wrong\n", __func__);
		return -1;
	}

	return 0;
}

static int autok_param_update(enum AUTOK_PARAM param_id, unsigned int result, u8 *autok_tune_res)
{
	if (param_id < TUNING_PARAM_COUNT) {
		if ((result > autok_param_info[param_id].range.end) ||
		    (result < autok_param_info[param_id].range.start)) {
			AUTOK_RAWPRINT("[AUTOK]param outof range : %d not in [%d,%d]\r\n",
				       result, autok_param_info[param_id].range.start,
				       autok_param_info[param_id].range.end);
			return -1;
		}
		autok_tune_res[param_id] = (u8) result;
		return 0;
	}
	AUTOK_RAWPRINT("[AUTOK]param not found\r\n");

	return -1;
}

static int autok_param_apply(struct msdc_host *host, u8 *autok_tune_res)
{
	unsigned int i = 0;
	unsigned int value = 0;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		value = (u8) autok_tune_res[i];
		msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
	}

	return 0;
}

static int autok_result_dump(struct msdc_host *host, u8 *autok_tune_res)
{
	AUTOK_RAWPRINT("[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[0], autok_tune_res[1], autok_tune_res[5], autok_tune_res[7]);
	AUTOK_RAWPRINT("[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
		autok_tune_res[2], autok_tune_res[3], autok_tune_res[4]);
	AUTOK_RAWPRINT("[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[13], autok_tune_res[9], autok_tune_res[11]);
	AUTOK_RAWPRINT("[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
		autok_tune_res[14], autok_tune_res[16], autok_tune_res[18]);

	return 0;
}

#if AUTOK_PARAM_DUMP_ENABLE
static int autok_register_dump(struct msdc_host *host)
{
	unsigned int i = 0;
	unsigned int value = 0;
	u8 autok_tune_res[TUNING_PARAM_COUNT];

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		msdc_autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}
	AUTOK_RAWPRINT("[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[0], autok_tune_res[1], autok_tune_res[5], autok_tune_res[7]);
	AUTOK_RAWPRINT("[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
		autok_tune_res[2], autok_tune_res[3], autok_tune_res[4]);
	AUTOK_RAWPRINT("[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[13], autok_tune_res[9], autok_tune_res[11]);
	AUTOK_RAWPRINT("[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
		autok_tune_res[14], autok_tune_res[16], autok_tune_res[18]);

	return 0;
}
#endif

void autok_tuning_parameter_init(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	/* void __iomem *base = host->base; */

	/* MSDC_SET_FIELD(MSDC_PATCH_BIT2, 7<<29, 2); */
	/* MSDC_SET_FIELD(MSDC_PATCH_BIT2, 7<<16, 4); */

	ret = autok_param_apply(host, res);
}

/*******************************************************
* Function: msdc_autok_adjust_paddly                   *
* Param : value - delay cnt from 0 to 63               *
*         pad_sel - 0 for cmd pad and 1 for data pad   *
*******************************************************/
#define CMD_PAD_RDLY 0
#define DAT_PAD_RDLY 1
#define DS_PAD_RDLY 2
static void msdc_autok_adjust_paddly(struct msdc_host *host, unsigned int *value,
				     unsigned int pad_sel)
{
	unsigned int uCfgL = 0;
	unsigned int uCfgLSel = 0;
	unsigned int uCfgH = 0;
	unsigned int uCfgHSel = 0;
	unsigned int dly_cnt = *value;

	uCfgL = (dly_cnt > 31) ? (31) : dly_cnt;
	uCfgH = (dly_cnt > 31) ? (dly_cnt - 32) : 0;

	uCfgLSel = (uCfgL > 0) ? 1 : 0;
	uCfgHSel = (uCfgH > 0) ? 1 : 0;
	switch (pad_sel) {
	case CMD_PAD_RDLY:
		msdc_autok_adjust_param(host, CMD_RD_D_DLY1, &uCfgL, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_RD_D_DLY2, &uCfgH, AUTOK_WRITE);

		msdc_autok_adjust_param(host, CMD_RD_D_DLY1_SEL, &uCfgLSel, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_RD_D_DLY2_SEL, &uCfgHSel, AUTOK_WRITE);
		break;
	case DAT_PAD_RDLY:
		msdc_autok_adjust_param(host, DAT_RD_D_DLY1, &uCfgL, AUTOK_WRITE);
		msdc_autok_adjust_param(host, DAT_RD_D_DLY2, &uCfgH, AUTOK_WRITE);

		msdc_autok_adjust_param(host, DAT_RD_D_DLY1_SEL, &uCfgLSel, AUTOK_WRITE);
		msdc_autok_adjust_param(host, DAT_RD_D_DLY2_SEL, &uCfgHSel, AUTOK_WRITE);
		break;
	case DS_PAD_RDLY:
		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY1, &uCfgL, AUTOK_WRITE);
		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY2, &uCfgH, AUTOK_WRITE);

		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY1_SEL, &uCfgLSel, AUTOK_WRITE);
		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY2_SEL, &uCfgHSel, AUTOK_WRITE);
		break;
	}
}

static void autok_paddly_update(unsigned int pad_sel, unsigned int dly_cnt, u8 *autok_tune_res)
{
	unsigned int uCfgL = 0;
	unsigned int uCfgLSel = 0;
	unsigned int uCfgH = 0;
	unsigned int uCfgHSel = 0;

	uCfgL = (dly_cnt > 31) ? (31) : dly_cnt;
	uCfgH = (dly_cnt > 31) ? (dly_cnt - 32) : 0;

	uCfgLSel = (uCfgL > 0) ? 1 : 0;
	uCfgHSel = (uCfgH > 0) ? 1 : 0;
	switch (pad_sel) {
	case CMD_PAD_RDLY:
		autok_param_update(CMD_RD_D_DLY1, uCfgL, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2, uCfgH, autok_tune_res);

		autok_param_update(CMD_RD_D_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	case DAT_PAD_RDLY:
		autok_param_update(DAT_RD_D_DLY1, uCfgL, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2, uCfgH, autok_tune_res);

		autok_param_update(DAT_RD_D_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	case DS_PAD_RDLY:
		autok_param_update(EMMC50_DS_Z_DLY1, uCfgL, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2, uCfgH, autok_tune_res);

		autok_param_update(EMMC50_DS_Z_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	}
}

static void msdc_autok_window_apply(enum AUTOK_SCAN_WIN scan_win, u64 sacn_window, unsigned char *autok_tune_res)
{
	switch (scan_win) {
	case CMD_RISE:
		autok_tune_res[CMD_SCAN_R0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[CMD_SCAN_R1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[CMD_SCAN_R2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[CMD_SCAN_R3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[CMD_SCAN_R4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[CMD_SCAN_R5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[CMD_SCAN_R6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[CMD_SCAN_R7] = (sacn_window >> 56) & 0xff;
		break;
	case CMD_FALL:
		autok_tune_res[CMD_SCAN_F0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[CMD_SCAN_F1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[CMD_SCAN_F2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[CMD_SCAN_F3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[CMD_SCAN_F4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[CMD_SCAN_F5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[CMD_SCAN_F6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[CMD_SCAN_F7] = (sacn_window >> 56) & 0xff;
		break;
	case DAT_RISE:
		autok_tune_res[DAT_SCAN_R0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DAT_SCAN_R1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DAT_SCAN_R2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DAT_SCAN_R3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DAT_SCAN_R4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DAT_SCAN_R5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DAT_SCAN_R6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DAT_SCAN_R7] = (sacn_window >> 56) & 0xff;
		break;
	case DAT_FALL:
		autok_tune_res[DAT_SCAN_F0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DAT_SCAN_F1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DAT_SCAN_F2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DAT_SCAN_F3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DAT_SCAN_F4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DAT_SCAN_F5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DAT_SCAN_F6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DAT_SCAN_F7] = (sacn_window >> 56) & 0xff;
		break;
	case DS_WIN:
		autok_tune_res[DS_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DS_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DS_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DS_SCAN_3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DS_SCAN_4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DS_SCAN_5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DS_SCAN_6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DS_SCAN_7] = (sacn_window >> 56) & 0xff;
		break;
	}
}

static void msdc_autok_version_apply(unsigned char *autok_tune_res)
{
	autok_tune_res[AUTOK_VER0] = (AUTOK_VERSION >> 0) & 0xff;
	autok_tune_res[AUTOK_VER1] = (AUTOK_VERSION >> 8) & 0xff;
	autok_tune_res[AUTOK_VER2] = (AUTOK_VERSION >> 16) & 0xff;
	autok_tune_res[AUTOK_VER3] = (AUTOK_VERSION >> 24) & 0xff;
}

/*******************************************************
* Exectue tuning IF Implenment                         *
*******************************************************/
static int autok_write_param(struct msdc_host *host, enum AUTOK_PARAM param, u32 value)
{
	msdc_autok_adjust_param(host, param, &value, AUTOK_WRITE);

	return 0;
}

int autok_path_sel(struct msdc_host *host)
{
	void __iomem *base = host->base;

	autok_write_param(host, READ_DATA_SMPL_SEL, 0);
	autok_write_param(host, WRITE_DATA_SMPL_SEL, 0);

	/* clK tune all data Line share dly */
	autok_write_param(host, DATA_DLYLINE_SEL, 0);

	/* data tune mode select */
#if defined(CHIP_DENALI_3_DAT_TUNE)
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 1);
#else
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 0);
#endif
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 1);
	autok_write_param(host, MSDC_RESP_ASYNC_FIFO_SEL, 0);

	/* eMMC50 Function Mux */
	/* write path switch to emmc45 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 0);

	/* response path switch to emmc45 */
	autok_write_param(host, EMMC50_CMD_MUX_EN, 0);
	/* response use DS latch */
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 0);
	autok_write_param(host, EMMC50_WDATA_EDGE, 0);
	MSDC_SET_FIELD(EMMC50_CFG1, MSDC_EMMC50_CFG1_DSCFG, 0);

	/* Common Setting Config */
	autok_write_param(host, CKGEN_MSDC_DLY_SEL, AUTOK_CKGEN_VALUE);
	autok_write_param(host, CMD_RSP_TA_CNTR, AUTOK_CMD_TA_VALUE);
	autok_write_param(host, WRDAT_CRCS_TA_CNTR, AUTOK_CRC_TA_VALUE);

	MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA, AUTOK_BUSY_MA_VALUE);
	/* DDR50 byte swap issue design fix feature enable */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, 1 << 19, 1);

	return 0;
}
EXPORT_SYMBOL(autok_path_sel);

int autok_init_ddr208(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_DDR208_PORT3_VALUE);
	/* LATCH_TA_EN Config for CMD Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_DDR208_PORT3_VALUE);
	/* response path switch to emmc50 */
	autok_write_param(host, EMMC50_CMD_MUX_EN, 1);
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 1);
	MSDC_SET_FIELD(EMMC50_CFG1, MSDC_EMMC50_CFG1_DSCFG, 1);
	/* write path switch to emmc50 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 1);
	/* Specifical for HS400 Path Sel */
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 0);
	return 0;
}
EXPORT_SYMBOL(autok_init_ddr208);

int autok_init_sdr104(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	if (host->sclk <= 100000000) {
		/* LATCH_TA_EN Config for WCRC Path HS FS mode */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_HS_VALUE);
		/* LATCH_TA_EN Config for CMD Path HS FS mode */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_HS_VALUE);
	} else {
		/* LATCH_TA_EN Config for WCRC Path SDR104 mode */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_SDR104_PORT1_VALUE);
		/* LATCH_TA_EN Config for CMD Path SDR104 mode */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_SDR104_PORT1_VALUE);
	}
	/* enable dvfs feature */
	/* if (host->hw->host_function == MSDC_SDIO) */
	/*	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, 1); */
	MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL, 6);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT, 0);
	MSDC_SET_FIELD(SDC_FIFO_CFG, SDC_FIFO_CFG_WR_VALID_SEL, 0);
	MSDC_SET_FIELD(SDC_FIFO_CFG, SDC_FIFO_CFG_RD_VALID_SEL, 0);

	return 0;
}
EXPORT_SYMBOL(autok_init_sdr104);

int autok_init_hs200(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_HS200_PORT0_VALUE);
	/* LATCH_TA_EN Config for CMD Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_HS200_PORT0_VALUE);

	MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL, 6);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT, 0);
	MSDC_SET_FIELD(SDC_FIFO_CFG, SDC_FIFO_CFG_WR_VALID_SEL, 0);
	MSDC_SET_FIELD(SDC_FIFO_CFG, SDC_FIFO_CFG_RD_VALID_SEL, 0);

	return 0;
}
EXPORT_SYMBOL(autok_init_hs200);

int autok_init_hs400(struct msdc_host *host)
{
	void __iomem *base = host->base;
	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_HS400_PORT0_VALUE);
	/* LATCH_TA_EN Config for CMD Path HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_HS400_PORT0_VALUE);
	/* write path switch to emmc50 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 1);
	/* Specifical for HS400 Path Sel */
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 0);

	return 0;
}
EXPORT_SYMBOL(autok_init_hs400);

int execute_online_tuning_hs400(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	unsigned int response;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k, cycle_value;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	unsigned int opcode = MMC_SEND_STATUS;
	unsigned int uDatDly = 0;

	autok_init_hs400(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64)(1LL << j);
					break;
				} else if ((ret & E_RESULT_FATAL_ERR) != 0)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (uCmdEdge)
			msdc_autok_window_apply(CMD_FALL, RawData64, p_autok_tune_res);
		else
			msdc_autok_window_apply(CMD_RISE, RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0)
			return -1;


		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}
	/* DLY3 keep default value 20 */
	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = 31;
	cycle_value = uCmdDatInfo.cycle_cnt;
	/* Step2 : Tuning DS Clk Path-ZCLK only tune DLY1 */
#ifdef CMDQ
	opcode = MMC_SEND_EXT_CSD; /* can also use MMC_READ_SINGLE_BLOCK */
#else
	opcode = MMC_READ_SINGLE_BLOCK;
#endif
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* check device status */
	ret = autok_send_tune_cmd(host, 13, TUNE_CMD);
	if (ret == E_RESULT_PASS) {
		response = MSDC_READ32(SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]current device status 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD error while check device status\r\n");
	/* check QSR status */
	ret = autok_send_tune_cmd(host, CHECK_QSR, TUNE_CMD);
	if (ret == E_RESULT_PASS) {
		response = MSDC_READ32(SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]current QSR 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD error while check QSR\r\n");
	/* tune data pad delay , find data pad boundary */
	for (j = 0; j < 32; j++) {
		msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT
				    ("[AUTOK]Error Autok CMD Failed while tune DATA PAD Delay\r\n");
				return -1;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0)
				break;
			else if ((ret & E_RESULT_FATAL_ERR) != 0)
				return -1;
		}
		if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
			p_autok_tune_res[DAT_RD_D_DLY1] = j;
			if (j)
				p_autok_tune_res[DAT_RD_D_DLY1_SEL] = 1;
			break;
		}
	}
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));
	pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[0]);
	RawData64 = 0LL;
	/* tune DS delay , base on data pad boundary */
	for (j = 0; j < 32; j++) {
		msdc_autok_adjust_paddly(host, &j, DS_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT
				    ("[AUTOK]Error Autok CMD Failed while tune DS Delay\r\n");
				return -1;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
				RawData64 |= (u64) (1LL << j);
				break;
			} else if ((ret & E_RESULT_FATAL_ERR) != 0)
				return -1;
		}
	}
	RawData64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] DLY1/2 %d \t %d \t %s\r\n", uCmdEdge, score,
		       tune_result_str64);
	msdc_autok_window_apply(DS_WIN, RawData64, p_autok_tune_res);
	if (autok_check_scan_res64(RawData64, pBdInfo, 0) != 0)
		return -1;


	if (autok_ds_dly_sel(pBdInfo, &uDatDly, p_autok_tune_res) == 0) {
		autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);
	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}

	return 0;
}

int execute_cmd_online_tuning(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
#if !defined(FPGA_PLATFORM)
	void __iomem *base_top = host->base_top;
#endif
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k; /* cycle_value */
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[5];
	unsigned int opcode = MMC_SEND_STATUS;

	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Tuning Cmd Path */
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64)(1LL << j);
					break;
				} else if ((ret & E_RESULT_FATAL_ERR) != 0)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
				tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0)
			return -1;

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdDatInfo.opt_edge_sel, AUTOK_WRITE);
		msdc_autok_adjust_paddly(host, &uCmdDatInfo.opt_dly_cnt, CMD_PAD_RDLY);
		MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, p_autok_tune_res[0]);
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			MSDC_GET_FIELD(EMMC_TOP_CMD, PAD_CMD_RXDLY, p_autok_tune_res[1]);
			MSDC_GET_FIELD(EMMC_TOP_CMD, PAD_CMD_RD_RXDLY_SEL, p_autok_tune_res[2]);
			MSDC_GET_FIELD(EMMC_TOP_CMD, PAD_CMD_RXDLY2, p_autok_tune_res[3]);
			MSDC_GET_FIELD(EMMC_TOP_CMD, PAD_CMD_RD_RXDLY2_SEL, p_autok_tune_res[4]);
#else
			return 0;
#endif
		} else {
			MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, p_autok_tune_res[1]);
			MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL, p_autok_tune_res[2]);
			MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRDLY2, p_autok_tune_res[3]);
			MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL, p_autok_tune_res[4]);
		}
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	AUTOK_RAWPRINT("[AUTOK]CMD [EDGE:%d DLY1:%d DLY2:%d]\r\n",
		p_autok_tune_res[0], p_autok_tune_res[1], p_autok_tune_res[3]);

	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
			sizeof(p_autok_tune_res) / sizeof(u8));
	}

	return 0;
}

/* online tuning for latch ck */
int autok_execute_tuning_latch_ck(struct msdc_host *host, unsigned int opcode,
	unsigned int latch_ck_initail_value)
{
	unsigned int ret = 0;
	unsigned int j, k;
	void __iomem *base = host->base;
	unsigned int tune_time;

	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	switch (host->hw->host_function) {
	case MSDC_EMMC:
		tune_time = AUTOK_LATCH_CK_EMMC_TUNE_TIMES;
		break;
	case MSDC_SD:
		tune_time = AUTOK_LATCH_CK_SD_TUNE_TIMES;
		break;
	case MSDC_SDIO:
		tune_time = AUTOK_LATCH_CK_SDIO_TUNE_TIMES;
		break;
	default:
		tune_time = AUTOK_LATCH_CK_SDIO_TUNE_TIMES;
		break;
	}
	for (j = latch_ck_initail_value; j < 8; j += (host->hclk / host->sclk)) {
		host->tune_latch_ck_cnt = 0;
		msdc_clear_fifo();
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
		for (k = 0; k < tune_time; k++) {
			if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
				switch (k) {
				case 0:
					host->tune_latch_ck_cnt = 1;
					break;
				default:
					host->tune_latch_ck_cnt = k;
					break;
				}
			} else if (opcode == MMC_SEND_TUNING_BLOCK) {
				switch (k) {
				case 0:
				case 1:
				case 2:
					host->tune_latch_ck_cnt = 1;
					break;
				default:
					host->tune_latch_ck_cnt = k - 1;
					break;
				}
			} else if (opcode == MMC_SEND_EXT_CSD) {
				host->tune_latch_ck_cnt = k + 1;
			} else
				host->tune_latch_ck_cnt++;
			ret = autok_send_tune_cmd(host, opcode, TUNE_LATCH_CK);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Error Autok CMD Failed while tune LATCH CK\r\n");
				break;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Error Autok  tune LATCH_CK error %d\r\n", j);
				break;
			}
		}
		if (ret == 0) {
			MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
			break;
		}
	}
	host->tune_latch_ck_cnt = 0;

	return j;

}

/* online tuning for eMMC4.5(hs200) */
int execute_online_tuning_hs200(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	unsigned int response;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	unsigned int opcode = MMC_SEND_STATUS;

	autok_init_hs200(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RESULT_FATAL_ERR) != 0)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (uCmdEdge)
			msdc_autok_window_apply(CMD_FALL, RawData64, p_autok_tune_res);
		else
			msdc_autok_window_apply(CMD_RISE, RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			return -1;
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	/* Step2 Tuning Data Path (Only Rising Edge Used) */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* check device status */
	ret = autok_send_tune_cmd(host, 13, TUNE_CMD);
	if (ret == E_RESULT_PASS) {
		response = MSDC_READ32(SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]current device status 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD error while check device status\r\n");
	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uDatEdge]);
		msdc_autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT("[AUTOK]Error Autok CMD Failed while tune Read\r\n");
					return -1;
				} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RESULT_FATAL_ERR) != 0)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n", uDatEdge, score,
			       tune_result_str64);
		if (uDatEdge)
			msdc_autok_window_apply(DAT_FALL, RawData64, p_autok_tune_res);
		else
			msdc_autok_window_apply(DAT_RISE, RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			return -1;
		}

		uDatEdge ^= 0x1;
	} while (uDatEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(RD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(DAT_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
		autok_param_update(WD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK  */
#if 0
	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	p_autok_tune_res[INT_DAT_LATCH_CK] = autok_execute_tuning_latch_ck(host, opcode,
		p_autok_tune_res[INT_DAT_LATCH_CK]);
#endif

	autok_result_dump(host, p_autok_tune_res);

#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}

	return 0;
}

/* online tuning for SDIO3.0 plus */
int execute_online_tuning_sdio30_plus(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	unsigned int uDatDly = 0;

	autok_init_ddr208(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

#if SDIO_PLUS_CMD_TUNE
	autok_write_param(host, EMMC50_CMD_MUX_EN, 0);
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 0);
	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & E_RESULT_RSP_CRC) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RESULT_CMD_TMO) != 0) {
					autok_msdc_reset();
					msdc_clear_fifo();
					MSDC_WRITE32(MSDC_INT, 0xffffffff);
					RawData64 |= (u64) (1LL << j);
					break;
				} else if (ret == E_RESULT_FATAL_ERR)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (uCmdEdge)
			msdc_autok_window_apply(CMD_FALL, RawData64, p_autok_tune_res);
		else
			msdc_autok_window_apply(CMD_RISE, RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0)
			return -1;
		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}
#endif
	/* DLY3 keep default value 20 */
	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = 31;
	/* Step2 : Tuning DS Clk Path-ZCLK only tune DLY1 */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* tune data pad delay , find data pad boundary */
	for (j = 0; j < 32; j++) {
		msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Error CMD Failed while tune DATA\r\n");
				return -1;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0)
				break;
			else if ((ret & E_RESULT_FATAL_ERR) != 0)
				return -1;
		}
		if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
			p_autok_tune_res[DAT_RD_D_DLY1] = j;
			if (j)
				p_autok_tune_res[DAT_RD_D_DLY1_SEL] = 1;
			break;
		}
	}
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));
	pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[0]);
	RawData64 = 0LL;
	/* tune DS delay , base on data pad boundary */
	for (j = 0; j < 32; j++) {
		msdc_autok_adjust_paddly(host, &j, DS_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_SDIO_PLUS);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				RawData64 |= (u64) (1LL << j);
				break;
			} else if ((ret & E_RESULT_FATAL_ERR) != 0)
				return -1;
		}
	}
	RawData64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] DLY1/2 %d \t %d \t %s\r\n", uCmdEdge, score,
		       tune_result_str64);
	msdc_autok_window_apply(DS_WIN, RawData64, p_autok_tune_res);
	if (autok_check_scan_res64(RawData64, pBdInfo, 0) != 0)
		return -1;

	if (autok_ds_dly_sel(pBdInfo, &uDatDly, p_autok_tune_res) == 0) {
		autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);
	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
			   sizeof(p_autok_tune_res) / sizeof(u8));
	}
	host->autok_error = 0;

	return 0;
}

/* online tuning for SDIO/SD */
int execute_online_tuning(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];

	autok_init_sdr104(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & E_RESULT_RSP_CRC) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RESULT_CMD_TMO) != 0) {
					autok_msdc_reset();
					msdc_clear_fifo();
					MSDC_WRITE32(MSDC_INT, 0xffffffff);
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RESULT_FATAL_ERR) != 0)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (uCmdEdge)
			msdc_autok_window_apply(CMD_FALL, RawData64, p_autok_tune_res);
		else
			msdc_autok_window_apply(CMD_RISE, RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			return -1;
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	/* Step2 : Tuning Data Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uDatEdge]);
		msdc_autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT
					    ("[AUTOK]Error Autok CMD Failed while tune Read\r\n");
					host->autok_error = -1;
					return -1;
				} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RESULT_FATAL_ERR) != 0)
					return -1;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n", uDatEdge, score,
			       tune_result_str64);
		if (uDatEdge)
			msdc_autok_window_apply(DAT_FALL, RawData64, p_autok_tune_res);
		else
			msdc_autok_window_apply(DAT_RISE, RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64, pBdInfo, AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			return -1;
		}

		uDatEdge ^= 0x1;
	} while (uDatEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(RD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(DAT_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
		autok_param_update(WD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK */
#if 0
	opcode = MMC_SEND_TUNING_BLOCK;
	p_autok_tune_res[INT_DAT_LATCH_CK] = autok_execute_tuning_latch_ck(host, opcode,
		p_autok_tune_res[INT_DAT_LATCH_CK]);
#endif

	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}
	host->autok_error = 0;

	return 0;
}

void autok_msdc_tx_setting(struct msdc_host *host, struct mmc_ios *ios)
{
	void __iomem *base = host->base;
#if !defined(FPGA_PLATFORM)
	void __iomem *base_top = host->base_top;
#endif

	if (host->hw->host_function == MSDC_EMMC) {
		if (ios->timing == MMC_TIMING_MMC_HS400) {
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_TXSKEW_SEL,
				AUTOK_MSDC0_HS400_TXSKEW);
#if !defined(FPGA_PLATFORM)
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0,
				PAD_CLK_TXDLY,
				AUTOK_MSDC0_HS400_CLKTXDLY);
			MSDC_SET_FIELD(EMMC_TOP_CMD,
				PAD_CMD_TX_DLY,
				AUTOK_MSDC0_HS400_CMDTXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT0_TUNE,
				PAD_DAT0_TX_DLY,
				AUTOK_MSDC0_HS400_DAT0TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT1_TUNE,
				PAD_DAT1_TX_DLY,
				AUTOK_MSDC0_HS400_DAT1TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT2_TUNE,
				PAD_DAT2_TX_DLY,
				AUTOK_MSDC0_HS400_DAT2TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT3_TUNE,
				PAD_DAT3_TX_DLY,
				AUTOK_MSDC0_HS400_DAT3TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT4_TUNE,
				PAD_DAT4_TX_DLY,
				AUTOK_MSDC0_HS400_DAT4TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT5_TUNE,
				PAD_DAT5_TX_DLY,
				AUTOK_MSDC0_HS400_DAT5TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT6_TUNE,
				PAD_DAT6_TX_DLY,
				AUTOK_MSDC0_HS400_DAT6TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT7_TUNE,
				PAD_DAT7_TX_DLY,
				AUTOK_MSDC0_HS400_DAT7TXDLY);
#endif
		} else if (ios->timing == MMC_TIMING_MMC_HS200) {
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_TXSKEW_SEL,
				AUTOK_MSDC0_HS400_TXSKEW);
		} else {
			if (ios->timing == MMC_TIMING_MMC_DDR52) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_DDR50CKD,
					AUTOK_MSDC0_DDR50_DDRCKD);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_DDR50CKD, 0);
			}
#if !defined(FPGA_PLATFORM)
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0,
				PAD_CLK_TXDLY,
				AUTOK_MSDC0_CLKTXDLY);
			MSDC_SET_FIELD(EMMC_TOP_CMD,
				PAD_CMD_TX_DLY,
				AUTOK_MSDC0_CMDTXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT0_TUNE,
				PAD_DAT0_TX_DLY,
				AUTOK_MSDC0_DAT0TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT1_TUNE,
				PAD_DAT1_TX_DLY,
				AUTOK_MSDC0_DAT1TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT2_TUNE,
				PAD_DAT2_TX_DLY,
				AUTOK_MSDC0_DAT2TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT3_TUNE,
				PAD_DAT3_TX_DLY,
				AUTOK_MSDC0_DAT3TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT4_TUNE,
				PAD_DAT4_TX_DLY,
				AUTOK_MSDC0_DAT4TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT5_TUNE,
				PAD_DAT5_TX_DLY,
				AUTOK_MSDC0_DAT5TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT6_TUNE,
				PAD_DAT6_TX_DLY,
				AUTOK_MSDC0_DAT6TXDLY);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT7_TUNE,
				PAD_DAT7_TX_DLY,
				AUTOK_MSDC0_DAT7TXDLY);
#endif
		}
	} else if (host->hw->host_function == MSDC_SD) {
		MSDC_SET_FIELD(MSDC_IOCON,
			MSDC_IOCON_DDR50CKD, AUTOK_MSDC_DDRCKD);
		if (ios->timing == MMC_TIMING_UHS_SDR104) {
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				AUTOK_MSDC1_CLK_SDR104_TX_VALUE);
		} else {
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				AUTOK_MSDC1_CLK_TX_VALUE);
		}
	} else if (host->hw->host_function == MSDC_SDIO) {
		MSDC_SET_FIELD(MSDC_PAD_TUNE0,
			MSDC_PAD_TUNE0_CLKTXDLY,
			AUTOK_MSDC3_SDIO_PLUS_CLKTXDLY);
		MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE,
			MSDC_EMMC50_PAD_CMD_TUNE_TXDLY,
			AUTOK_MSDC3_SDIO_PLUS_CMDTXDLY);
		MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE,
			MSDC_EMMC50_PAD_DAT0_TXDLY,
			AUTOK_MSDC3_SDIO_PLUS_DAT0TXDLY);
		MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE,
			MSDC_EMMC50_PAD_DAT1_TXDLY,
			AUTOK_MSDC3_SDIO_PLUS_DAT1TXDLY);
		MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE,
			MSDC_EMMC50_PAD_DAT2_TXDLY,
			AUTOK_MSDC3_SDIO_PLUS_DAT2TXDLY);
		MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE,
			MSDC_EMMC50_PAD_DAT3_TXDLY,
			AUTOK_MSDC3_SDIO_PLUS_DAT3TXDLY);
	}
}
EXPORT_SYMBOL(autok_msdc_tx_setting);

void autok_low_speed_switch_edge(struct msdc_host *host, struct mmc_ios *ios, enum ERROR_TYPE error_type)
{
	void __iomem *base = host->base;
	unsigned int orig_resp_edge, orig_crc_fifo_edge, orig_read_edge, orig_read_fifo_edge;
	unsigned int cur_resp_edge, cur_crc_fifo_edge, cur_read_edge, cur_read_fifo_edge;

	AUTOK_RAWPRINT("[AUTOK][low speed switch edge]=========start========\r\n");
	if (host->hw->host_function == MSDC_EMMC) {
		switch (error_type) {
		case CMD_ERROR:
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, orig_resp_edge);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, orig_resp_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]pre_edge = %d cur_edge = %d\r\n"
				, orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT0_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_MMC_DDR52) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, orig_read_edge);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][read err]PB0_BIT3_VALID DDR pre_edge = %d",
					orig_read_edge);
				AUTOK_RAWPRINT("cur_edge = %d cur_fifo_edge = %d\r\n",
					cur_read_edge, cur_read_fifo_edge);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, 0);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, orig_read_fifo_edge);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, orig_read_fifo_edge ^ 0x1);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_VALID orig_fifo_edge = %d",
					orig_read_fifo_edge);
				AUTOK_RAWPRINT("cur_edge = %d cur_fifo_edge = %d\r\n",
					cur_read_edge, cur_read_fifo_edge);
			}
#else
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_INVALID pre_edge = %d cur_edge = %d\r\n"
				, orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, orig_crc_fifo_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, orig_crc_fifo_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][write err]orig_fifo_edge = %d cur_fifo_edge = %d\r\n"
				, orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	} else if (host->hw->host_function == MSDC_SD) {
		switch (error_type) {
		case CMD_ERROR:
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, orig_resp_edge);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, orig_resp_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]pre_edge = %d cur_edge = %d\r\n"
					, orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT1_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_UHS_DDR50) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, orig_read_edge);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_VALID DDR pre_edge = %d",
					orig_read_edge);
				AUTOK_RAWPRINT(" cur_edge = %d cur_fifo_edge = %d\r\n",
					cur_read_edge, cur_read_fifo_edge);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, 0);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, orig_read_fifo_edge);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, orig_read_fifo_edge ^ 0x1);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_VALID orig_fifo_edge = %d",
					orig_read_fifo_edge);
				AUTOK_RAWPRINT(" cur_edge = %d cur_fifo_edge = %d\r\n",
					cur_read_edge, cur_read_fifo_edge);
			}
#else
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_INVALID pre_edge = %d cur_edge = %d\r\n"
				, orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, orig_crc_fifo_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, orig_crc_fifo_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][write err]orig_fifo_edge = %d cur_fifo_edge = %d\r\n"
				, orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	} else if (host->hw->host_function == MSDC_SDIO) {
		switch (error_type) {
		case CMD_ERROR:
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, orig_resp_edge);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, orig_resp_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL, cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]pre_edge = %d cur_edge = %d\r\n"
				, orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT3_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_UHS_DDR50) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, orig_read_edge);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_VALID DDR pre_edge = %d",
					orig_read_edge);
				AUTOK_RAWPRINT(" cur_edge = %d cur_fifo_edge = %d\r\n",
					cur_read_edge, cur_read_fifo_edge);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, 0);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, orig_read_fifo_edge);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, orig_read_fifo_edge ^ 0x1);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_VALID orig_fifo_edge = %d",
					orig_read_fifo_edge);
				AUTOK_RAWPRINT(" cur_edge = %d cur_fifo_edge = %d\r\n",
					cur_read_edge, cur_read_fifo_edge);
			}
#else
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][read err]PB0[3]_INVALID pre_edge = %d cur_edge = %d\r\n"
				, orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, orig_crc_fifo_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, orig_crc_fifo_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE, cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][write err]orig_fifo_edge = %d cur_fifo_edge = %d\r\n"
				, orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	}
	AUTOK_RAWPRINT("[AUTOK][low speed switch edge]=========end========\r\n");
}
EXPORT_SYMBOL(autok_low_speed_switch_edge);

int autok_offline_tuning_TX(struct msdc_host *host)
{
	int ret = 0;
	void __iomem *base = host->base;
#if !defined(FPGA_PLATFORM)
	void __iomem *base_top = host->base_top;
#endif
	unsigned int response;
	unsigned int tune_tx_value;
	unsigned char tune_cnt;
	unsigned char i;
	unsigned char tune_crc_cnt[32];
	unsigned char tune_pass_cnt[32];
	unsigned char tune_tmo_cnt[32];
	char tune_result[33];
	unsigned int cmd_tx;
	unsigned int dat_tx[8] = {0};

	AUTOK_RAWPRINT("[AUTOK][tune cmd TX]=========start========\r\n");
	/* store tx setting */
	if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
		MSDC_GET_FIELD(EMMC_TOP_CMD, PAD_CMD_TX_DLY, cmd_tx);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT0_TUNE, PAD_DAT0_TX_DLY, dat_tx[0]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT1_TUNE, PAD_DAT1_TX_DLY, dat_tx[1]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT2_TUNE, PAD_DAT2_TX_DLY, dat_tx[2]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT3_TUNE, PAD_DAT3_TX_DLY, dat_tx[3]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT4_TUNE, PAD_DAT4_TX_DLY, dat_tx[4]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT5_TUNE, PAD_DAT5_TX_DLY, dat_tx[5]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT6_TUNE, PAD_DAT6_TX_DLY, dat_tx[6]);
		MSDC_GET_FIELD(TOP_EMMC50_PAD_DAT7_TUNE, PAD_DAT7_TX_DLY, dat_tx[7]);
#else
		return 0;
#endif
	} else {
		MSDC_GET_FIELD(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY, cmd_tx);
		MSDC_GET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY, dat_tx[0]);
		MSDC_GET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY, dat_tx[1]);
		MSDC_GET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY, dat_tx[2]);
		MSDC_GET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY, dat_tx[3]);
	}

	/* Step1 : Tuning Cmd TX */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		if (host->hw->host_function == MSDC_EMMC)
#if !defined(FPGA_PLATFORM)
			MSDC_SET_FIELD(EMMC_TOP_CMD, PAD_CMD_TX_DLY, tune_tx_value);
#else
			return 0;
#endif
		else
			MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY, tune_tx_value);
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			if (host->hw->host_function == MSDC_EMMC)
				ret = autok_send_tune_cmd(host, MMC_SEND_STATUS, TUNE_CMD);
			else
				ret = autok_send_tune_cmd(host, MMC_SEND_TUNING_BLOCK, TUNE_SDIO_PLUS);
			if ((ret & E_RESULT_CMD_TMO) != 0)
				tune_tmo_cnt[tune_tx_value]++;
			else if ((ret&(E_RESULT_RSP_CRC)) != 0)
				tune_crc_cnt[tune_tx_value]++;
			else if ((ret & (E_RESULT_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}
#if 0
		AUTOK_RAWPRINT("[AUTOK]tune_cmd_TX cmd_tx_value = %d tmo_cnt = %d crc_cnt = %d pass_cnt = %d\n",
			tune_tx_value, tune_tmo_cnt[tune_tx_value], tune_crc_cnt[tune_tx_value],
			tune_pass_cnt[tune_tx_value]);
#endif
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_cmd_TX 0 - 31      %s\r\n", tune_result);
	AUTOK_RAWPRINT("[AUTOK][tune cmd TX]=========end========\r\n");

	/* restore cmd tx setting */
	if (host->hw->host_function == MSDC_EMMC)
#if !defined(FPGA_PLATFORM)
		MSDC_SET_FIELD(EMMC_TOP_CMD, PAD_CMD_TX_DLY, cmd_tx);
#else
		return 0;
#endif
	else
		MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY, cmd_tx);
	AUTOK_RAWPRINT("[AUTOK][tune data TX]=========start========\r\n");

	/* Step2 : Tuning Data TX */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT0_TUNE, PAD_DAT0_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT1_TUNE, PAD_DAT1_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT2_TUNE, PAD_DAT2_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT3_TUNE, PAD_DAT3_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT4_TUNE, PAD_DAT4_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT5_TUNE, PAD_DAT5_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT6_TUNE, PAD_DAT6_TX_DLY, tune_tx_value);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT7_TUNE, PAD_DAT7_TX_DLY, tune_tx_value);
#else
			return 0;
#endif
		} else {
			MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY, tune_tx_value);
			MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY, tune_tx_value);
			MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY, tune_tx_value);
			MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY, tune_tx_value);
		}

		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			if (host->hw->host_function == MSDC_EMMC) {
				/* check device status */
				response = 0;
				while (((response >> 9) & 0xF) != 4) {
					ret = autok_send_tune_cmd(host, MMC_SEND_STATUS, TUNE_CMD);
					if ((ret & (E_RESULT_RSP_CRC | E_RESULT_CMD_TMO)) != 0) {
						AUTOK_RAWPRINT("[AUTOK]tune data TX cmd13 occur error\r\n");
						AUTOK_RAWPRINT("[AUTOK]tune data TX fail\r\n");
						goto end;
					}
					response = MSDC_READ32(SDC_RESP0);
					if ((((response >> 9) & 0xF) == 5) || (((response >> 9) & 0xF) == 6))
						ret = autok_send_tune_cmd(host, MMC_STOP_TRANSMISSION, TUNE_CMD);
				}

				/* send cmd24 write one block data */
				ret = autok_send_tune_cmd(host, MMC_WRITE_BLOCK, TUNE_DATA);
				response = MSDC_READ32(SDC_RESP0);
			} else {
				/* send cmd53 write data */
				ret = autok_send_tune_cmd(host, SD_IO_RW_EXTENDED, TUNE_SDIO_PLUS);
			}
			if ((ret & (E_RESULT_RSP_CRC | E_RESULT_CMD_TMO)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]tune data TX cmd%d occur error\n",
					MMC_WRITE_BLOCK);
				AUTOK_RAWPRINT("[AUTOK]tune data TX fail\n");
				goto end;
			}
			if ((ret & E_RESULT_DAT_TMO) != 0) {
				tune_tmo_cnt[tune_tx_value]++;
				/* send CMD52 abort command */
				if (host->id != 0)
					autok_send_tune_cmd(host, SD_IO_RW_DIRECT, TUNE_CMD);
			} else if ((ret & (E_RESULT_DAT_CRC)) != 0) {
				tune_crc_cnt[tune_tx_value]++;
				/* send CMD52 abort command */
				if (host->id != 0)
					autok_send_tune_cmd(host, SD_IO_RW_DIRECT, TUNE_CMD);
			} else if ((ret & (E_RESULT_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}
#if 0
		AUTOK_RAWPRINT("[AUTOK]tune_data_TX data_tx_value = %d tmo_cnt = %d crc_cnt = %d pass_cnt = %d\n",
			tune_tx_value, tune_tmo_cnt[tune_tx_value], tune_crc_cnt[tune_tx_value],
			tune_pass_cnt[tune_tx_value]);
#endif
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_data_TX 0 - 31      %s\r\n", tune_result);

	/* restore data tx setting */
	if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT0_TUNE, PAD_DAT0_TX_DLY,
			dat_tx[0]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT1_TUNE, PAD_DAT1_TX_DLY,
			dat_tx[1]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT2_TUNE, PAD_DAT2_TX_DLY,
			dat_tx[2]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT3_TUNE, PAD_DAT3_TX_DLY,
			dat_tx[3]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT4_TUNE, PAD_DAT4_TX_DLY,
			dat_tx[4]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT5_TUNE, PAD_DAT5_TX_DLY,
			dat_tx[5]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT6_TUNE, PAD_DAT6_TX_DLY,
			dat_tx[6]);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DAT7_TUNE, PAD_DAT7_TX_DLY,
			dat_tx[7]);
#else
		return 0;
#endif
	} else {
		MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY,
			dat_tx[0]);
		MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY,
			dat_tx[1]);
		MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY,
			dat_tx[2]);
		MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY,
			dat_tx[3]);
	}

	AUTOK_RAWPRINT("[AUTOK][tune data TX]=========end========\r\n");
end:
	return ret;
}

int autok_sdio30_plus_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;

	do_gettimeofday(&tm_s);

	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		msdc_autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	ret = execute_online_tuning_sdio30_plus(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok Failed========\r\n");
		AUTOK_RAWPRINT("[AUTOK] ========restore pre autok parameters========\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}
#if AUTOK_OFFLINE_TUNE_TX_ENABLE
	autok_offline_tuning_TX(host);
#endif

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(autok_sdio30_plus_tuning);

int autok_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;

	do_gettimeofday(&tm_s);

	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		msdc_autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	ret = execute_online_tuning(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok Failed========\r\n");
		AUTOK_RAWPRINT("[AUTOK] ========restore pre autok parameters========\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(autok_execute_tuning);

int hs400_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		msdc_autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	ret = execute_online_tuning_hs400(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok HS400 Failed========\r\n");
		AUTOK_RAWPRINT("[AUTOK] ========restore pre autok parameters========\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}
#if AUTOK_OFFLINE_TUNE_TX_ENABLE
	autok_offline_tuning_TX(host);
#endif

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS400]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs400_execute_tuning);

int hs400_execute_tuning_cmd(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	autok_init_hs400(host);
	ret = execute_cmd_online_tuning(host, res);
	if (ret != 0)
		AUTOK_RAWPRINT("[AUTOK only for cmd] ========Error: Autok HS400 Failed========\r\n");

	/* autok_msdc_reset(); */
	/* msdc_clear_fifo(); */
	/* MSDC_WRITE32(MSDC_INT, 0xffffffff); */
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS400 only for cmd]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs400_execute_tuning_cmd);

int hs200_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		msdc_autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	ret = execute_online_tuning_hs200(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok HS200 Failed========\r\n");
		AUTOK_RAWPRINT("[AUTOK] ========restore pre autok parameters========\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS200]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning);

int hs200_execute_tuning_cmd(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	autok_init_hs200(host);
	ret = execute_cmd_online_tuning(host, res);
	if (ret != 0)
		AUTOK_RAWPRINT("[AUTOK only for cmd] ========Error: Autok HS200 Failed========\r\n");

	/* autok_msdc_reset(); */
	/* msdc_clear_fifo(); */
	/* MSDC_WRITE32(MSDC_INT, 0xffffffff); */
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS200 only for cmd]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning_cmd);

