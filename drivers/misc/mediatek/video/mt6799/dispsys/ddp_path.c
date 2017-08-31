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


#define LOG_TAG "ddp_path"
#include "ddp_log.h"

#include <linux/types.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include "ddp_clkmgr.h"
#endif
#include "ddp_reg.h"

#include "ddp_debug.h"
#include "ddp_path.h"
#include "ddp_dump.h"
#include "primary_display.h"
#include "ddp_hal.h"
#include "disp_helper.h"
#include "ddp_path.h"

#include "m4u.h"

/*#pragma GCC optimize("O0")*/

#define BIT_NUM (8)
#define DDP_ENING_NUM (24)
#define DDP_MOUT_NUM     9
#define DDP_SEL_OUT_NUM  13
#define DDP_SEL_IN_NUM   20
#define DDP_ENG_NUM   10
#define DDP_MUTEX_MAX    5

struct module_map_s {
	enum DISP_MODULE_ENUM module;
	int bit;
	int mod_num;
};

struct m_to_b {
	int m;
	int v;
};

struct m_to_hw_b {
	int m;
	int g;
	int b;
};

struct mout_s {
	int id;
	struct m_to_b out_id_bit_map[BIT_NUM];

	unsigned long *reg;
	unsigned int reg_val;
};

struct mout_hw_s {
	int id;
	struct m_to_hw_b out_id_bit_map[BIT_NUM];
};

struct sel_s {
	int id;
	int id_bit_map[BIT_NUM];

	unsigned long *reg;
	unsigned int reg_val;
};


unsigned int module_list_scenario[DDP_SCENARIO_MAX][DDP_ENING_NUM] = {
	/*PRIMARY_DISP */
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0, DISP_MODULE_OD,
	 DISP_MODULE_DITHER0, DISP_MODULE_RDMA0, DISP_PATH0,
	 DISP_MODULE_UFOE, DISP_MODULE_PWM0, DISP_MODULE_DSI0, -1, -1, -1, -1, -1},
	/*PRIMARY_RDMA0_COLOR0_DISP */
	{
	 DISP_MODULE_RDMA0, DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0,
	 DISP_MODULE_GAMMA0, DISP_MODULE_OD, DISP_MODULE_DITHER0, DISP_PATH0, DISP_MODULE_UFOE,
	 DISP_MODULE_PWM0, DISP_MODULE_DSI0, -1, -1, -1, -1, -1, -1, -1, -1, -1},

	/*PRIMARY_RDMA0_DISP */
	{
	 DISP_MODULE_RDMA0, DISP_MODULE_PWM0, DISP_MODULE_DSI0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

	/*PRIMARY_BYPASS_RDMA */
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0,
	 DISP_MODULE_DITHER0, DISP_PATH0,
	 DISP_MODULE_UFOE, DISP_MODULE_PWM0, DISP_MODULE_DSI0, -1, -1, -1, -1, -1, -1},

	/*PRIMARY_OVL_MEMOUT */
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_WDMA0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

	/*PRIMARY_DITHER_MEMOUT */
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0,
	 DISP_MODULE_DITHER0, DISP_MODULE_WDMA0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*PRIMARY_UFOE_MEMOUT */
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0, DISP_MODULE_OD,
	 DISP_MODULE_DITHER0, DISP_MODULE_RDMA0, DISP_PATH0, DISP_MODULE_UFOE, DISP_MODULE_WDMA0, -1, -1, -1},
	/*SUB_DISP */
	{
	 DISP_MODULE_OVL1, DISP_MODULE_OVL1_2L_VIRTUAL, DISP_MODULE_OVL1_2L, DISP_MODULE_COLOR1,
	 DISP_MODULE_CCORR1, DISP_MODULE_AAL1, DISP_MODULE_GAMMA1, DISP_MODULE_DITHER1,
	 DISP_MODULE_RDMA1, DISP_PATH1, DISP_MODULE_DSC, DISP_MODULE_DPI, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*SUB_RDMA1_DISP */
	{
	 DISP_MODULE_RDMA1, DISP_PATH1, DISP_MODULE_DSC, DISP_MODULE_DPI, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*SUB_OVL_MEMOUT */
	{
	 DISP_MODULE_OVL1, DISP_MODULE_OVL1_2L_VIRTUAL, DISP_MODULE_OVL1_2L, DISP_MODULE_WDMA1, -1, -1, -1},
	/*PRIMARY_DISP ALL*/
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_WDMA0, DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0,
	 DISP_MODULE_OD, DISP_MODULE_DITHER0, DISP_MODULE_RDMA0, DISP_PATH0, DISP_MODULE_UFOE, DISP_MODULE_PWM0,
	 DISP_MODULE_DSI0, -1, -1, -1, -1},
	/*SUB_ALL */
	{
	DISP_MODULE_OVL1, DISP_MODULE_OVL1_2L_VIRTUAL, DISP_MODULE_OVL1_2L, DISP_MODULE_WDMA1, DISP_MODULE_COLOR1,
	DISP_MODULE_CCORR1, DISP_MODULE_AAL1, DISP_MODULE_GAMMA1, DISP_MODULE_DITHER1,
	DISP_MODULE_RDMA1, DISP_PATH1, DISP_MODULE_DSC, DISP_MODULE_DPI, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*DDP_SCENARIO_DITHER_1TO2*/
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0, DISP_MODULE_OD,
	 DISP_MODULE_DITHER0, DISP_MODULE_WDMA0, DISP_MODULE_RDMA0, DISP_PATH0, DISP_MODULE_UFOE, DISP_MODULE_PWM0,
	 DISP_MODULE_DSI0, -1, -1, -1, -1},
	/*DDP_SCENARIO_UFOE_1TO2*/
	{
	 DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_VIRTUAL,
	 DISP_MODULE_COLOR0, DISP_MODULE_CCORR0, DISP_MODULE_AAL0, DISP_MODULE_GAMMA0, DISP_MODULE_OD,
	 DISP_MODULE_DITHER0, DISP_MODULE_RDMA0, DISP_PATH0, DISP_MODULE_UFOE, DISP_MODULE_WDMA0, DISP_MODULE_PWM0,
	 DISP_MODULE_DSI0, -1, -1, -1, -1},

};

/* 1st para is mout's input, 2nd para is mout's output */
static struct mout_s mout_map[] = {
	/* OVL_MOUT */
	{DISP_MODULE_OVL0,
		{{DISP_MODULE_OVL0_2L_VIRTUAL, 1 << 0}, {DISP_MODULE_WDMA0, 1 << 1}, {DISP_MODULE_RSZ0, 1 << 2},
	{DISP_MODULE_OVL0_VIRTUAL, 1 << 3}, {-1, 0} },
		0, 0},
	{DISP_MODULE_OVL0_VIRTUAL,
		{{DISP_MODULE_COLOR0, 1 << 0}, {DISP_MODULE_WDMA0, 1 << 1}, {-1, 0} },
		0, 0},
	{DISP_MODULE_OVL1,
		{{DISP_MODULE_OVL1_2L_VIRTUAL, 1 << 0}, {DISP_MODULE_OVL0_VIRTUAL, 1 << 1}, {DISP_MODULE_WDMA1, 1 << 2},
			{DISP_MODULE_COLOR1, 1 << 3}, {DISP_MODULE_RSZ1, 1 << 4}, {-1, 0} },
		0, 0},
	{DISP_MODULE_OVL1_2L,
		{{DISP_MODULE_COLOR1, 1 << 0}, {DISP_MODULE_WDMA1, 1 << 1}, {DISP_MODULE_OVL0_VIRTUAL, 1 << 2},
			{DISP_MODULE_RSZ1, 1 << 3}, {DISP_MODULE_RDMA2, 1 << 4}, {-1, 0} },
		0, 0},
	/* DITHER0_MOUT */
	{DISP_MODULE_DITHER0,
		{{DISP_MODULE_RDMA0, 1 << 0}, {DISP_PATH0, 1 << 1}, {DISP_MODULE_WDMA0, 1 << 2},
			{-1, 0} },
		0, 0},
	{DISP_MODULE_DITHER1,
		{{DISP_MODULE_RDMA1, 1 << 0}, {DISP_PATH1, 1 << 1}, {DISP_MODULE_WDMA1, 1 << 2},
			{-1, 0} },
		0, 0},
	/* UFOE_MOUT */
	{DISP_MODULE_UFOE,
		{{DISP_MODULE_DSI0, 1 << 0}, {DISP_MODULE_SPLIT0, 1 << 1},
			{DISP_MODULE_DPI, 1 << 2}, {DISP_MODULE_WDMA0, 1 << 3},
			{DISP_MODULE_DSI1, 1 << 4}, {-1, 0} },
		0, 0},
	/* DSC_MOUT */
	{DISP_MODULE_DSC,
		{{DISP_MODULE_DSI0, 1 << 0}, {DISP_MODULE_DSI1, 1 << 1},
			{DISP_MODULE_WDMA0, 1 << 2}, {DISP_MODULE_DPI, 1 << 3},
			{DISP_MODULE_WDMA1, 1 << 4}, {-1, 0} },
		0, 0},
	/* DSC_2ND_MOUT */
	{DISP_MODULE_DSC,
		{{DISP_MODULE_DSI1, 1 << 0}, {DISP_MODULE_WDMA1, 1 << 1}, {-1, 0} },
		0, 0}
};

static struct sel_s sel_out_map[] = {
	/* OVL0_SOUT */
	{DISP_MODULE_OVL0_2L, {DISP_MODULE_OVL0_VIRTUAL, DISP_MODULE_OVL1, DISP_MODULE_OVL1_2L_VIRTUAL,
		DISP_MODULE_RSZ0, -1}, 0, 0},
	/* OVL1_INT_SOUT */
	{DISP_MODULE_OVL0_2L, {DISP_MODULE_OVL1, DISP_MODULE_OVL1, -1}, 0, 0},
	/* OVL0_2L_INT_SOUT */
	{DISP_MODULE_OVL0_2L_VIRTUAL, {DISP_MODULE_OVL0_2L, DISP_MODULE_OVL0_2L, -1}, 0, 0},
	/* OVL1_2L_INT_SOUT */
	{DISP_MODULE_OVL1_2L_VIRTUAL, {DISP_MODULE_OVL1_2L, DISP_MODULE_OVL1_2L, -1}, 0, 0},
	/* DISP_PATH_SOUT */
	{DISP_PATH0, {DISP_MODULE_UFOE, DISP_MODULE_DSC, DISP_MODULE_DSI0, -1}, 0, 0},
	{DISP_PATH1, {DISP_MODULE_DSC, DISP_MODULE_UFOE, DISP_MODULE_DSC_2ND, DISP_MODULE_DSI1, -1}, 0, 0},
	/* RDMA0_SOUT */
	{DISP_MODULE_RDMA0, {DISP_PATH0, DISP_MODULE_COLOR0, DISP_MODULE_DSI0,
				    DISP_MODULE_DPI, -1}, 0, 0},
	/* RDMA1_SOUT */
	{DISP_MODULE_RDMA1, {DISP_PATH1, DISP_MODULE_COLOR1, DISP_MODULE_DSI1,
				    DISP_MODULE_DPI}, 0, 0},
	/* RDMA2_SOUT */
	{DISP_MODULE_RDMA2, {DISP_MODULE_DPI, DISP_MODULE_DSC, DISP_MODULE_RSZ1, -1}, 0, 0},
	/* CCORR0_SOUT */
	{DISP_MODULE_CCORR0, {DISP_MODULE_AAL0, DISP_MODULE_RSZ0, -1}, 0, 0},
	/* CCORR1_SOUT */
	{DISP_MODULE_CCORR1, {DISP_MODULE_AAL1, DISP_MODULE_RSZ1, -1}, 0, 0},
	/* RSZ0_SOUT */
	{DISP_MODULE_RSZ0, {DISP_MODULE_OVL0_2L_VIRTUAL, DISP_MODULE_OVL0_VIRTUAL,
		DISP_MODULE_AAL0, -1}, 0, 0},
	/* RSZ1_SOUT */
	{DISP_MODULE_RSZ1, {DISP_MODULE_OVL1_2L_VIRTUAL, DISP_MODULE_COLOR1,
		DISP_MODULE_AAL1, DISP_MODULE_DSC, -1}, 0, 0},
};

/* 1st para is sout's output, 2nd para is sout's input */
static struct sel_s sel_in_map[] = {
	/* COLOR_SEL */
	{DISP_MODULE_COLOR0, {DISP_MODULE_RDMA0, DISP_MODULE_OVL0_VIRTUAL, -1}, 0, 0},
	{DISP_MODULE_COLOR1, {DISP_MODULE_RDMA1, DISP_MODULE_OVL1_2L, DISP_MODULE_OVL1,
		DISP_MODULE_RSZ1, -1}, 0, 0},

	/* WDMA_SEL */
	{DISP_MODULE_WDMA0, {DISP_MODULE_OVL0_VIRTUAL, DISP_MODULE_DITHER0, DISP_MODULE_UFOE,
		DISP_MODULE_OVL0, DISP_MODULE_DSC, -1}, 0, 0},
	{DISP_MODULE_WDMA1, {DISP_MODULE_OVL1_2L, DISP_MODULE_DITHER1, DISP_MODULE_DSC, DISP_MODULE_OVL1,
		DISP_MODULE_DSC_2ND, -1}, 0, 0},

	/* UFOE_SEL */
	{DISP_MODULE_UFOE, {DISP_PATH0, DISP_PATH1, -1}, 0, 0},

	/* DSC_SEL */
	{DISP_MODULE_DSC, {DISP_PATH0, DISP_PATH1, DISP_MODULE_RDMA2, DISP_MODULE_RSZ1, -1}, 0, 0},

	/* DSI_SEL */
	{DISP_MODULE_DSI0, {DISP_MODULE_UFOE, DISP_MODULE_SPLIT0, DISP_MODULE_RDMA0, DISP_MODULE_DSC,
		DISP_PATH0, -1}, 0, 0},
	{DISP_MODULE_DSI1, {DISP_MODULE_SPLIT0, DISP_PATH1, DISP_MODULE_DSC, DISP_MODULE_UFOE,
		DISP_MODULE_RDMA1, DISP_MODULE_DSC_2ND, -1}, 0, 0},
	{DISP_MODULE_DSIDUAL, {DISP_MODULE_UFOE, DISP_MODULE_SPLIT0, -1}, 0, 0},
	{DISP_MODULE_DSIDUAL, {DISP_MODULE_SPLIT0, -1}, 0, 0},

	/* DPI0_SEL */
	{DISP_MODULE_DPI, {DISP_MODULE_UFOE, DISP_MODULE_RDMA0, DISP_MODULE_DSC, DISP_MODULE_RDMA1,
		DISP_MODULE_RDMA2, -1}, 0, 0},

	/* DISP_PATH0_SEL */
	{DISP_PATH0, {DISP_MODULE_RDMA0, DISP_MODULE_DITHER0, -1}, 0, 0},
	{DISP_PATH1, {DISP_MODULE_RDMA1, DISP_MODULE_DITHER1, -1}, 0, 0},

	/* OVL0_SEL */
	{DISP_MODULE_OVL0_VIRTUAL, {DISP_MODULE_OVL0_2L, DISP_MODULE_OVL1_2L, DISP_MODULE_OVL1, DISP_MODULE_RSZ0,
		DISP_MODULE_OVL0, -1}, 0, 0},
	{DISP_MODULE_OVL1_2L_VIRTUAL, {DISP_MODULE_OVL0_2L, DISP_MODULE_OVL1, DISP_MODULE_RSZ1, -1}, 0, 0},
	{DISP_MODULE_OVL0_2L_VIRTUAL, {DISP_MODULE_OVL0, DISP_MODULE_RSZ0, -1}, 0, 0},

	/* RSZ0_SEL */
	{DISP_MODULE_RSZ0, {DISP_MODULE_OVL0, DISP_MODULE_OVL0_2L, DISP_MODULE_CCORR0, -1}, 0, 0},
	{DISP_MODULE_RSZ1, {DISP_MODULE_OVL1, DISP_MODULE_OVL1_2L, DISP_MODULE_CCORR1, DISP_MODULE_RDMA2, -1}, 0, 0},

	/* AAL0_SEL */
	{DISP_MODULE_AAL0, {DISP_MODULE_CCORR0, DISP_MODULE_RSZ0, -1}, 0, 0},
	{DISP_MODULE_AAL1, {DISP_MODULE_CCORR1, DISP_MODULE_RSZ1, -1}, 0, 0},


};

/* 1st para is mout's input, 2nd & 3rd para are mout's output */
static struct mout_hw_s mout_hw_map[] = {
	/* OVL0_MOUT */
	{DISP_MODULE_OVL0,
	 {{DISP_MODULE_OVL0, 2, 2}, {DISP_MODULE_OVL0_2L_VIRTUAL, 2, 3}, {DISP_MODULE_WDMA0, 2, 4},
	  {DISP_MODULE_RSZ0, 2, 5},
	  {DISP_MODULE_OVL0_VIRTUAL, 2, 6}, {-1, 0, 0} } },
	/* OVL0_PQ_MOUT */
	{DISP_MODULE_OVL0_VIRTUAL,
	 {{DISP_MODULE_OVL0_VIRTUAL, 2, 9}, {DISP_MODULE_COLOR0, 2, 7}, {DISP_MODULE_WDMA0, 2, 8},
	  {-1, 0, 0} } },
	/* OVL1_MOUT */
	{DISP_MODULE_OVL1,
	 {{DISP_MODULE_OVL1, 2, 16}, {DISP_MODULE_OVL1_2L_VIRTUAL, 2, 17},
	  {DISP_MODULE_OVL0_VIRTUAL, 2, 18}, {DISP_MODULE_WDMA1, 2, 19},
	  {DISP_MODULE_COLOR1, 2, 20}, {DISP_MODULE_RSZ1, 2, 21}, {-1, 0, 0} } },
	/* OVL1_PQ_MOUT */
	{DISP_MODULE_OVL1_2L,
	 {{DISP_MODULE_OVL1_2L, 2, 14}, {DISP_MODULE_COLOR1, 2, 22}, {DISP_MODULE_WDMA1, 2, 23},
	  {DISP_MODULE_OVL0_VIRTUAL, 2, 24},
	  {DISP_MODULE_RSZ1, 2, 25}, {DISP_MODULE_RDMA2, 2, 26}, {-1, 0, 0} } },
	/* DITHER0_MOUT */
	{DISP_MODULE_DITHER0,
	 {{DISP_MODULE_DITHER0, 1, 13}, {DISP_MODULE_RDMA0, 1, 14}, {DISP_PATH0, 1, 15},
	  {DISP_MODULE_WDMA0, 1, 16},
	  {-1, 0, 0} } },
	/* DITHER1_MOUT */
	{DISP_MODULE_DITHER1,
	 {{DISP_MODULE_DITHER1, 1, 17}, {DISP_MODULE_RDMA1, 1, 18}, {DISP_PATH1, 1, 19},
	  {DISP_MODULE_WDMA1, 1, 20},
	  {-1, 0, 0} } },
	/* UFOE_MOUT */
	{DISP_MODULE_UFOE,
	 {{DISP_MODULE_UFOE, 3, 0}, {DISP_MODULE_DSI0, 3, 1}, {DISP_MODULE_SPLIT0, 3, 2},
	  {DISP_MODULE_DPI, 3, 3}, {DISP_MODULE_WDMA0, 3, 4},
	  {DISP_MODULE_DSI1, 3, 5}, {-1, 0, 0} } },
	/* DSC_MOUT */
	{DISP_MODULE_DSC,
	 {{DISP_MODULE_DSC, 1, 30}, {DISP_MODULE_DSI0, 1, 22}, {DISP_MODULE_DSI1, 1, 23},
	  {DISP_MODULE_DPI, 1, 24}, {DISP_MODULE_WDMA1, 1, 25},
	  {-1, 0, 0} } },
	/* DSC_2ND_MOUT */
	{DISP_MODULE_DSC,
	 {{DISP_MODULE_DSC, 1, 31}, {DISP_MODULE_DSI1, 1, 28}, {DISP_MODULE_WDMA1, 1, 29}, {-1, 0, 0} } }
};

static struct mout_hw_s sel_out_hw_map[] = {
	/* OVL0_SOUT */
	{DISP_MODULE_OVL0_2L,
	 {{DISP_MODULE_OVL0_2L, 2, 0}, {DISP_MODULE_OVL0_VIRTUAL, 2, 10}, {DISP_MODULE_OVL1, 2, 11},
	  {DISP_MODULE_OVL1_2L_VIRTUAL, 2, 12},
	  {DISP_MODULE_RSZ0, 2, 13}, {-1, 0, 0} } },
	/* OVL1_INT_SOUT */
	{DISP_MODULE_OVL0_2L,
	 {{-1, 0, 0} } },
	/* OVL0_2L_INT_SOUT */
	{DISP_MODULE_OVL0_2L_VIRTUAL,
	 {{-1, 0, 0} } },
	/* OVL1_2L_INT_SOUT */
	{DISP_MODULE_OVL1_2L_VIRTUAL,
	 {{-1, 0, 0} } },
	/* PATH0_SOUT */
	{DISP_PATH0,
	 {{DISP_PATH0, 0, 17}, {DISP_MODULE_UFOE, 0, 18}, {DISP_MODULE_DSC, 0, 19},
	  {DISP_MODULE_DSI0, 0, 20},
	  {-1, 0, 0} } },
	/* PATH1_SOUT */
	{DISP_PATH1,
	 {{DISP_PATH1, 0, 21}, {DISP_MODULE_DSC, 0, 22}, {DISP_MODULE_UFOE, 0, 23},
	  {DISP_MODULE_DSC_2ND, 0, 24},
	  {DISP_MODULE_DSI1, 0, 25}, {-1, 0, 0} } },
	/* RDMA0_SOUT */
	{DISP_MODULE_RDMA0,
	 {{DISP_MODULE_RDMA0, 0, 26}, {DISP_PATH0, 0, 27}, {DISP_MODULE_COLOR0, 0, 28},
	  {DISP_MODULE_DSI0, 0, 29},
	  {DISP_MODULE_DPI, 0, 30}, {-1, 0, 0} } },
	/* RDMA1_SOUT */
	{DISP_MODULE_RDMA1,
	 {{DISP_MODULE_RDMA1, 1, 0}, {DISP_PATH1, 1, 1}, {DISP_MODULE_COLOR1, 1, 2},
	  {DISP_MODULE_DSI1, 1, 3},
	  {DISP_MODULE_DPI, 1, 4}, {-1, 0, 0} } },
	/* RDMA2_SOUT */
	{DISP_MODULE_RDMA2,
	 {{DISP_MODULE_RDMA2, 1, 5}, {DISP_MODULE_DPI, 1, 6}, {DISP_MODULE_DSC, 1, 7},
	  {DISP_MODULE_RSZ1, 1, 8},
	  {-1, 0, 0} } },
	/* CCORR0_SOUT */
	{DISP_MODULE_CCORR0,
	 {{DISP_MODULE_CCORR0, 0, 4}, {DISP_MODULE_AAL0, 0, 6}, {DISP_MODULE_RSZ0, 0, 7},
	  {-1, 0, 0} } },
	/* CCORR1_SOUT */
	{DISP_MODULE_CCORR1,
	 {{DISP_MODULE_CCORR1, 0, 5}, {DISP_MODULE_AAL1, 0, 8}, {DISP_MODULE_RSZ1, 0, 9},
	  {-1, 0, 0} } },
	/* RSZ0_SOUT */
	{DISP_MODULE_RSZ0,
	 {{DISP_MODULE_RSZ0, 3, 13}, {DISP_MODULE_OVL0_2L_VIRTUAL, 3, 15},
	  {DISP_MODULE_OVL0_VIRTUAL, 3, 16}, {DISP_MODULE_AAL0, 3, 17},
	  {-1, 0, 0} } },
	/* RSZ1_SOUT */
	{DISP_MODULE_RSZ1,
	 {{DISP_MODULE_RSZ1, 3, 14}, {DISP_MODULE_OVL1_2L_VIRTUAL, 3, 18},
	  {DISP_MODULE_COLOR1, 3, 19}, {DISP_MODULE_AAL1, 3, 20},
	  {DISP_MODULE_DSC, 3, 21}, {-1, 0, 0} } },
};

static struct mout_hw_s sel_in_hw_map[] = {
	/* COLOR0_SEL */
	{DISP_MODULE_COLOR0,
	 {{DISP_MODULE_COLOR0, 0, 1}, {DISP_MODULE_RDMA0, 0, 28}, {DISP_MODULE_OVL0_VIRTUAL, 2, 7},
	  {-1, 0, 0} } },
	/* COLOR1_SEL */
	{DISP_MODULE_COLOR1,
	 {{DISP_MODULE_COLOR1, 0, 3}, {DISP_MODULE_RDMA1, 1, 2}, {DISP_MODULE_OVL1_2L, 2, 22},
	  {DISP_MODULE_OVL1, 2, 20},
	  {DISP_MODULE_RSZ1, 3, 19}, {-1, 0, 0} } },
	/* WDMA0_SEL */
	{DISP_MODULE_WDMA0,
	 {{DISP_MODULE_WDMA0, 1, 11}, {DISP_MODULE_OVL0_VIRTUAL, 2, 8},
	  {DISP_MODULE_DITHER0, 1, 16}, {DISP_MODULE_UFOE, 3, 4},
	  {DISP_MODULE_OVL0, 2, 4}, {DISP_MODULE_DSC, 1, 24}, {-1, 0, 0} } },
	/* WDMA1_SEL */
	{DISP_MODULE_WDMA1,
	 {{DISP_MODULE_WDMA1, 1, 12}, {DISP_MODULE_OVL1_2L, 2, 23}, {DISP_MODULE_DITHER1, 1, 20},
	  {DISP_MODULE_DSC, 1, 26},
	  {DISP_MODULE_OVL1, 2, 19}, {DISP_MODULE_DSC_2ND, 1, 29}, {-1, 0, 0} } },
	/* UFOE_SEL */
	{DISP_MODULE_UFOE,
	 {{DISP_MODULE_UFOE, 3, 6}, {DISP_PATH0, 0, 18}, {DISP_PATH1, 0, 23}, {-1, 0, 0} } },
	/* DSC_SEL */
	{DISP_MODULE_DSC,
	 {{DISP_MODULE_DSC, 1, 27}, {DISP_PATH0, 0, 19}, {DISP_PATH1, 0, 22},
	  {DISP_MODULE_RDMA2, 1, 7},
	  {DISP_MODULE_RSZ1, 3, 21}, {-1, 0, 0} } },
	/* DSI0_SEL */
	{DISP_MODULE_DSI0,
	 {{DISP_MODULE_DSI0, 2, 27}, {DISP_MODULE_UFOE, 3, 1}, {DISP_MODULE_SPLIT0, 1, 9},
	  {DISP_MODULE_RDMA0, 0, 29},
	  {DISP_MODULE_DSC, 1, 22}, {DISP_PATH0, 0, 20}, {-1, 0, 0} } },
	/* DSI1_SEL */
	{DISP_MODULE_DSI1,
	 {{DISP_MODULE_DSI1, 2, 28}, {DISP_MODULE_SPLIT0, 1, 10}, {DISP_PATH1, 0, 25},
	  {DISP_MODULE_DSC, 1, 23},
	  {DISP_MODULE_UFOE, 3, 5}, {DISP_MODULE_RDMA1, 1, 3}, {DISP_MODULE_DSC_2ND, 1, 28},
	  {-1, 0, 0} } },
	/* DSIDUAL_SEL */
	{DISP_MODULE_DSIDUAL,
	 {{DISP_MODULE_DSI0, 2, 27}, {DISP_MODULE_UFOE, 3, 1}, {DISP_MODULE_SPLIT0, 1, 9},
	  {-1, 0, 0} } },
	/* DSIDUAL_SEL */
	{DISP_MODULE_DSIDUAL,
	 {{DISP_MODULE_DSI0, 2, 27}, {DISP_MODULE_SPLIT0, 1, 9}, {-1, 0, 0} } },
	/* DPI0_SEL */
	{DISP_MODULE_DPI,
	 {{DISP_MODULE_DPI, 1, 21}, {DISP_MODULE_UFOE, 3, 3}, {DISP_MODULE_RDMA0, 0, 30},
	  {DISP_MODULE_DSC, 1, 25},
	  {DISP_MODULE_RDMA1, 1, 4}, {DISP_MODULE_RDMA2, 1, 6}, {-1, 0, 0} } },
	/* DISP_PATH0_SEL */
	{DISP_PATH0,
	 {{DISP_PATH0, 0, 17}, {DISP_MODULE_RDMA0, 0, 27}, {DISP_MODULE_DITHER0, 1, 15},
	  {-1, 0, 0} } },
	/* DISP_PATH1_SEL */
	{DISP_PATH1,
	 {{DISP_PATH1, 0, 21}, {DISP_MODULE_RDMA1, 1, 1}, {DISP_MODULE_DITHER1, 1, 19},
	  {-1, 0, 0} } },
	/* OVL0_SEL */
	{DISP_MODULE_OVL0_VIRTUAL,
	 {{DISP_MODULE_OVL0_VIRTUAL, 2, 9}, {DISP_MODULE_OVL0_2L, 2, 10},
	  {DISP_MODULE_OVL1_2L, 2, 24}, {DISP_MODULE_OVL1, 2, 18},
	  {DISP_MODULE_RSZ0, 3, 16}, {DISP_MODULE_OVL0, 2, 6}, {-1, 0, 0} } },
	/* OVL1_2L_SEL */
	{DISP_MODULE_OVL1_2L_VIRTUAL,
	 {{DISP_MODULE_OVL1_2L_VIRTUAL, 2, 15}, {DISP_MODULE_OVL0_2L, 2, 12},
	  {DISP_MODULE_OVL1, 2, 17}, {DISP_MODULE_RSZ1, 3, 18},
	  {-1, 0, 0} } },
	/* OVL0_2L_SEL */
	{DISP_MODULE_OVL0_2L_VIRTUAL,
	 {{DISP_MODULE_OVL0_2L_VIRTUAL, 2, 1}, {DISP_MODULE_OVL0, 2, 3}, {DISP_MODULE_RSZ0, 3, 15},
	  {-1, 0, 0} } },
	/* RSZ0_SEL */
	{DISP_MODULE_RSZ0,
	 {{DISP_MODULE_RSZ0, 3, 11}, {DISP_MODULE_OVL0, 2, 5}, {DISP_MODULE_OVL0_2L, 2, 13},
	  {DISP_MODULE_CCORR0, 0, 7},
	  {-1, 0, 0} } },
	/* RSZ1_SEL */
	{DISP_MODULE_RSZ1,
	 {{DISP_MODULE_RSZ1, 3, 12}, {DISP_MODULE_OVL1, 2, 21}, {DISP_MODULE_OVL1_2L, 2, 25},
	  {DISP_MODULE_CCORR1, 0, 9},
	  {DISP_MODULE_RDMA2, 1, 8}, {-1, 0, 0} } },
	/* AAL0_SEL */
	{DISP_MODULE_AAL0,
	 {{DISP_MODULE_AAL0, 0, 12}, {DISP_MODULE_CCORR0, 0, 6}, {DISP_MODULE_RSZ0, 3, 17},
	  {-1, 0, 0} } },
	/* AAL1_SEL */
	{DISP_MODULE_AAL1,
	 {{DISP_MODULE_AAL1, 0, 13}, {DISP_MODULE_CCORR1, 0, 8}, {DISP_MODULE_RSZ1, 3, 20},
	  {-1, 0, 0} } },
};

static struct mout_hw_s eng_hw_map[] = {
	/* COLOR0 */
	{DISP_MODULE_COLOR0,
	 {{DISP_MODULE_CCORR0, 0, 0}, {-1, 0, 0} } },
	/* COLOR1 */
	{DISP_MODULE_COLOR1,
	 {{DISP_MODULE_CCORR1, 0, 2}, {-1, 0, 0} } },
	/* DSC */
	{DISP_MODULE_DSC,
	 {{DISP_MODULE_DSC, 3, 9}, {-1, 0, 0} } },
	/* DSI0 */
	{DISP_MODULE_DSI0,
	 {{DISP_MODULE_DSI0, 3, 7}, {-1, 0, 0} } },
	/* DSI1 */
	{DISP_MODULE_DSI1,
	 {{DISP_MODULE_DSI1, 3, 8}, {-1, 0, 0} } },
	/* AAL0 */
	{DISP_MODULE_AAL0,
	 {{DISP_MODULE_GAMMA0, 0, 10}, {-1, 0, 0} } },
	/* AAL1 */
	{DISP_MODULE_AAL1,
	 {{DISP_MODULE_GAMMA1, 0, 11}, {-1, 0, 0} } },
	/* GAMMA0 */
	{DISP_MODULE_GAMMA0,
	 {{DISP_MODULE_OD, 0, 14}, {-1, 0, 0} } },
	/* GAMMA1 */
	{DISP_MODULE_GAMMA1,
	 {{DISP_MODULE_DITHER1, 0, 15}, {-1, 0, 0} } },
	/* OD */
	{DISP_MODULE_OD,
	 {{DISP_MODULE_DITHER0, 0, 16}, {-1, 0, 0} } },
};

int ddp_path_init(void)
{
	/* mout */
	mout_map[0].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
	mout_map[1].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_PQ_MOUT_EN;
	mout_map[2].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL1_MOUT_EN;
	mout_map[3].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL1_PQ_MOUT_EN;
	mout_map[4].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
	mout_map[5].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DITHER1_MOUT_EN;
	mout_map[6].reg = (unsigned long *)DISP_REG_CONFIG_DISP_UFOE_MOUT_EN;
	mout_map[7].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSC_MOUT_EN;
	mout_map[8].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSC_2ND_MOUT_EN;

	/* sel_out */
	sel_out_map[0].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_SOUT_SEL_IN;
	sel_out_map[1].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL1_INT_SOUT_SEL_IN;
	sel_out_map[2].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_2L_INT_SOUT_SEL_IN;
	sel_out_map[3].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL1_2L_INT_SOUT_SEL_IN;
	sel_out_map[4].reg = (unsigned long *)DISP_REG_CONFIG_DISP_PATH0_SOUT_SEL_IN;
	sel_out_map[5].reg = (unsigned long *)DISP_REG_CONFIG_DISP_PATH1_SOUT_SEL_IN;
	sel_out_map[6].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN;
	sel_out_map[7].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN;
	sel_out_map[8].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RDMA2_SOUT_SEL_IN;
	sel_out_map[9].reg = (unsigned long *)DISP_REG_CONFIG_DISP_CCORR0_SOUT_SEL_IN;
	sel_out_map[10].reg = (unsigned long *)DISP_REG_CONFIG_DISP_CCORR1_SOUT_SEL_IN;
	sel_out_map[11].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RSZ0_SOUT_SEL_IN;
	sel_out_map[12].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RSZ1_SOUT_SEL_IN;
	/* sel_in */
	sel_in_map[0].reg = (unsigned long *)DISP_REG_CONFIG_DISP_COLOR0_SEL_IN;
	sel_in_map[1].reg = (unsigned long *)DISP_REG_CONFIG_DISP_COLOR1_SEL_IN;
	sel_in_map[2].reg = (unsigned long *)DISP_REG_CONFIG_DISP_WDMA0_SEL_IN;
	sel_in_map[3].reg = (unsigned long *)DISP_REG_CONFIG_DISP_WDMA1_SEL_IN;
	sel_in_map[4].reg = (unsigned long *)DISP_REG_CONFIG_DISP_UFOE_SEL_IN;
	sel_in_map[5].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSC_SEL_IN;
	sel_in_map[6].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSI0_SEL_IN;
	sel_in_map[7].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSI1_SEL_IN;
	sel_in_map[8].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSI0_SEL_IN; /* DISDUAL for DSI0 */
	sel_in_map[9].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DSI1_SEL_IN; /* DISDUAL for DSI1 */
	sel_in_map[10].reg = (unsigned long *)DISP_REG_CONFIG_DISP_DPI0_SEL_IN;
	sel_in_map[11].reg = (unsigned long *)DISP_REG_CONFIG_DISP_PATH0_SEL_IN;
	sel_in_map[12].reg = (unsigned long *)DISP_REG_CONFIG_DISP_PATH1_SEL_IN;
	sel_in_map[13].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_SEL_IN;
	sel_in_map[14].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL1_2L_SEL_IN;
	sel_in_map[15].reg = (unsigned long *)DISP_REG_CONFIG_DISP_OVL0_2L_SEL_IN;
	sel_in_map[16].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RSZ0_SEL_IN;
	sel_in_map[17].reg = (unsigned long *)DISP_REG_CONFIG_DISP_RSZ1_SEL_IN;
	sel_in_map[18].reg = (unsigned long *)DISP_REG_CONFIG_DISP_AAL0_SEL_IN;
	sel_in_map[19].reg = (unsigned long *)DISP_REG_CONFIG_DISP_AAL1_SEL_IN;
	return 0;
}

static struct module_map_s module_mutex_map[DISP_MODULE_NUM] = {
	{DISP_MODULE_OVL0, 13, 0},
	{DISP_MODULE_OVL1, 14, 0},
	{DISP_MODULE_OVL0_2L, 15, 0},
	{DISP_MODULE_OVL1_2L, 16, 0},
	{DISP_MODULE_OVL0_VIRTUAL, -1, 0},
	{DISP_MODULE_OVL0_2L_VIRTUAL, -1, 0},
	{DISP_MODULE_OVL1_2L_VIRTUAL, -1, 0},
	{DISP_MODULE_RDMA0, 0, 0},
	{DISP_MODULE_RDMA1, 1, 0},
	{DISP_MODULE_RDMA2, 2, 0},
	{DISP_MODULE_WDMA0, 17, 0},
	{DISP_MODULE_WDMA1, 18, 0},
	{DISP_MODULE_COLOR0, 19, 0},
	{DISP_MODULE_COLOR1, 20, 0},
	{DISP_MODULE_CCORR0, 21, 0},
	{DISP_MODULE_CCORR1, 22, 0},
	{DISP_MODULE_AAL0, 23, 0},
	{DISP_MODULE_AAL1, 24, 0},
	{DISP_MODULE_GAMMA0, 25, 0},
	{DISP_MODULE_GAMMA1, 26, 0},
	{DISP_MODULE_OD, 27, 0},
	{DISP_MODULE_DITHER0, 28, 0},
	{DISP_MODULE_DITHER1, 29, 0},
	{DISP_PATH0, -1, 0},
	{DISP_PATH1, -1, 0},
	{DISP_MODULE_UFOE, 30, 0},
	{DISP_MODULE_DSC, 31, 0},
	{DISP_MODULE_DSC_2ND, 0, 1},
	{DISP_MODULE_SPLIT0, -1, 0},
	{DISP_MODULE_DPI, -1, 0},
	{DISP_MODULE_DSI0, 3, 1},
	{DISP_MODULE_DSI1, -1, 1},
	{DISP_MODULE_DSIDUAL, 3, 1},
	{DISP_MODULE_PWM0, 1, 1},
	{DISP_MODULE_PWM1, 2, 1},
	{DISP_MODULE_CONFIG, -1, 0},
	{DISP_MODULE_MUTEX, -1, 0},
	{DISP_MODULE_SMI_COMMON, -1, 0},
	{DISP_MODULE_SMI_LARB0, -1, 0},
	{DISP_MODULE_SMI_LARB1, -1, 0},
	{DISP_MODULE_MIPI0, -1, 0},
	{DISP_MODULE_MIPI1, -1, 0},
	{DISP_MODULE_RSZ0, 5, 1},
	{DISP_MODULE_RSZ1, 6, 1},
	{DISP_MODULE_UNKNOWN, -1, 0},
};

/* module can be connect if 1 */
static struct module_map_s module_can_connect[DISP_MODULE_NUM] = {
	{DISP_MODULE_OVL0, 1, 0},
	{DISP_MODULE_OVL1, 1, 0},
	{DISP_MODULE_OVL0_2L, 1, 0},
	{DISP_MODULE_OVL1_2L, 1, 0},
	{DISP_MODULE_OVL0_VIRTUAL, 1, 0},
	{DISP_MODULE_OVL0_2L_VIRTUAL, 1, 0},
	{DISP_MODULE_OVL1_2L_VIRTUAL, 1, 0},
	{DISP_MODULE_RDMA0, 1, 0},
	{DISP_MODULE_RDMA1, 1, 0},
	{DISP_MODULE_RDMA2, 1, 0},
	{DISP_MODULE_WDMA0, 1, 0},
	{DISP_MODULE_WDMA1, 1, 0},
	{DISP_MODULE_COLOR0, 1, 0},
	{DISP_MODULE_COLOR1, 1, 0},
	{DISP_MODULE_CCORR0, 1, 0},
	{DISP_MODULE_CCORR1, 1, 0},
	{DISP_MODULE_AAL0, 1, 0},
	{DISP_MODULE_AAL1, 1, 0},
	{DISP_MODULE_GAMMA0, 1, 0},
	{DISP_MODULE_GAMMA1, 1, 0},
	{DISP_MODULE_OD, 1, 0},
	{DISP_MODULE_DITHER0, 1, 0},
	{DISP_MODULE_DITHER1, 1, 0},
	{DISP_PATH0, 1, 0},
	{DISP_PATH1, 1, 0},
	{DISP_MODULE_UFOE, 1, 0},
	{DISP_MODULE_DSC, 1, 0},
	{DISP_MODULE_DSC_2ND, 1, 0},
	{DISP_MODULE_SPLIT0, 1, 0},
	{DISP_MODULE_DPI, 1, 0},
	{DISP_MODULE_DSI0, 1, 0},
	{DISP_MODULE_DSI1, 1, 0},
	{DISP_MODULE_DSIDUAL, 1, 0},
	{DISP_MODULE_PWM0, 0, 0},
	{DISP_MODULE_PWM1, 0, 0},
	{DISP_MODULE_CONFIG, 0, 0},
	{DISP_MODULE_MUTEX, 0, 0},
	{DISP_MODULE_SMI_COMMON, 0, 0},
	{DISP_MODULE_SMI_LARB0, 0, 0},
	{DISP_MODULE_SMI_LARB1, 0, 0},
	{DISP_MODULE_MIPI0, 0, 0},
	{DISP_MODULE_MIPI1, 0, 0},
	{DISP_MODULE_RSZ0, 1, 0},
	{DISP_MODULE_RSZ1, 1, 0},
	{DISP_MODULE_UNKNOWN, 0, 0},
};



char *ddp_get_scenario_name(enum DDP_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case DDP_SCENARIO_PRIMARY_DISP:
		return "primary_disp";
	case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
		return "primary_rdma0_color0_disp";
	case DDP_SCENARIO_PRIMARY_RDMA0_DISP:
		return "primary_rdma0_disp";
	case DDP_SCENARIO_PRIMARY_BYPASS_RDMA:
		return "primary_bypass_rdma";
	case DDP_SCENARIO_PRIMARY_OVL_MEMOUT:
		return "primary_ovl_memout";
	case DDP_SCENARIO_PRIMARY_DITHER_MEMOUT:
		return "primary_dither_memout";
	case DDP_SCENARIO_PRIMARY_UFOE_MEMOUT:
		return "primary_ufoe_memout";
	case DDP_SCENARIO_SUB_DISP:
		return "sub_disp";
	case DDP_SCENARIO_SUB_RDMA1_DISP:
		return "sub_rdma1_disp";
	case DDP_SCENARIO_SUB_OVL_MEMOUT:
		return "sub_ovl_memout";
	case DDP_SCENARIO_PRIMARY_ALL:
		return "primary_all";
	case DDP_SCENARIO_SUB_ALL:
		return "sub_all";
	case DDP_SCENARIO_DITHER_1TO2:
		return "dither_1to2";
	case DDP_SCENARIO_UFOE_1TO2:
		return "ufoe_1to2";
	default:
		DDPMSG("invalid scenario id=%d\n", scenario);
		return "unknown";
	}
}

int ddp_is_scenario_on_primary(enum DDP_SCENARIO_ENUM scenario)
{
	int on_primary = 0;

	switch (scenario) {
	case DDP_SCENARIO_PRIMARY_DISP:
	case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
	case DDP_SCENARIO_PRIMARY_RDMA0_DISP:
	case DDP_SCENARIO_PRIMARY_BYPASS_RDMA:
	case DDP_SCENARIO_PRIMARY_OVL_MEMOUT:
	case DDP_SCENARIO_PRIMARY_DITHER_MEMOUT:
	case DDP_SCENARIO_PRIMARY_UFOE_MEMOUT:
	case DDP_SCENARIO_PRIMARY_ALL:
	case DDP_SCENARIO_DITHER_1TO2:
	case DDP_SCENARIO_UFOE_1TO2:
		on_primary = 1;
		break;
	case DDP_SCENARIO_SUB_DISP:
	case DDP_SCENARIO_SUB_RDMA1_DISP:
	case DDP_SCENARIO_SUB_OVL_MEMOUT:
	case DDP_SCENARIO_SUB_ALL:
		on_primary = 0;
		break;
	default:
		DDPMSG("invalid scenario id=%d\n", scenario);
	}

	return on_primary;

}

char *ddp_get_mutex_sof_name(unsigned int regval)
{
	if (regval == SOF_VAL_MUTEX0_SOF_SINGLE_MODE)
		return "single";
	else if (regval == SOF_VAL_MUTEX0_SOF_FROM_DSI0)
		return "dsi0";
	else if (regval == SOF_VAL_MUTEX0_SOF_FROM_DPI)
		return "dpi";

	DDPDUMP("%s, unknown reg=%d\n", __func__, regval);
	return "unknown";
}

char *ddp_get_mode_name(enum DDP_MODE ddp_mode)
{
	switch (ddp_mode) {
	case DDP_VIDEO_MODE:
		return "vido_mode";
	case DDP_CMD_MODE:
		return "cmd_mode";
	default:
		DDPMSG("invalid ddp mode =%d\n", ddp_mode);
		return "unknown";
	}
}

static int ddp_get_module_num_l(int *module_list)
{
	unsigned int num = 0;

	while (*(module_list + num) != -1) {
		num++;

		if (num == DDP_ENING_NUM)
			break;
	}
	return num;
}

/* config mout/msel to creat a compelte path */
static void ddp_connect_path_l(int *module_list, void *handle)
{
	unsigned int i, j, k;
	int step = 0;
	unsigned int mout = 0;
	unsigned int reg_mout = 0;
	unsigned int mout_idx = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);

	DDPDBG("connect_path: %s to %s\n", ddp_get_module_name(module_list[0]),
	       ddp_get_module_name(module_list[module_num - 1]));
	/* connect mout */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] == mout_map[j].id) {
				/* find next module which can be connected */
				step = i + 1;
				while (module_can_connect[module_list[step]].bit == 0
				       && step < module_num) {
					step++;
				}
				ASSERT(step < module_num);
				mout = mout_map[j].reg_val;
				for (k = 0; k < BIT_NUM; k++) {
					if (mout_map[j].out_id_bit_map[k].m == -1)
						break;
					if (mout_map[j].out_id_bit_map[k].m == module_list[step]) {
						mout |= mout_map[j].out_id_bit_map[k].v;
						reg_mout |= mout;
						mout_idx = j;
						DDPDBG("connect mout %s to %s  bits 0x%x\n",
						       ddp_get_module_name(module_list[i]),
						       ddp_get_module_name(module_list[step]),
						       reg_mout);
						break;
					}
				}
				mout_map[j].reg_val = mout;
				mout = 0;
			}
		}
		if (reg_mout) {
			DISP_REG_SET(handle, mout_map[mout_idx].reg, reg_mout);
			reg_mout = 0;
			mout_idx = 0;
		}

	}
	/* connect out select */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_SEL_OUT_NUM; j++) {
			if (module_list[i] == sel_out_map[j].id) {
				step = i + 1;
				/* find next module which can be connected */
				while (module_can_connect[module_list[step]].bit == 0
				       && step < module_num) {
					step++;
				}
				ASSERT(step < module_num);
				for (k = 0; k < BIT_NUM; k++) {
					if (sel_out_map[j].id_bit_map[k] == -1)
						break;
					if (sel_out_map[j].id_bit_map[k] == module_list[step]) {
						DDPDBG("connect out_s %s to %s, value=%d\n",
						       ddp_get_module_name(module_list[i]),
						       ddp_get_module_name(module_list[step]), k);
						DISP_REG_SET(handle, sel_out_map[j].reg,
							     (uint16_t) k);
						break;
					}
				}
			}
		}
	}
	/* connect input select */
	for (i = 1; i < module_num; i++) {
		for (j = 0; j < DDP_SEL_IN_NUM; j++) {
			if (module_list[i] == sel_in_map[j].id) {
				int found = 0;

				step = i - 1;
				/* find next module which can be connected */
				while (module_can_connect[module_list[step]].bit == 0 && step > 0)
					step--;

				ASSERT(step >= 0);
				for (k = 0; k < BIT_NUM; k++) {
					if (sel_in_map[j].id_bit_map[k] == -1)
						break;
					if (sel_in_map[j].id_bit_map[k] == module_list[step]) {
						DDPDBG("connect in_s %s to %s, value=%d\n",
						       ddp_get_module_name(module_list[step]),
						       ddp_get_module_name(module_list[i]), k);
						DISP_REG_SET(handle, sel_in_map[j].reg,
							     (uint16_t) k);
						found = 1;
						break;
					}
				}
				if (!found)
					DDPERR("%s error: %s sel_in not set\n", __func__,
					       ddp_get_module_name(module_list[i]));
			}
		}
	}
}

static void ddp_check_path_l(int *module_list)
{
	unsigned int i, j, k;
	int step = 0;
	int valid = 0;
	unsigned int mout;
	unsigned int path_error = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);

	DDPDUMP("check_path: %s to %s\n", ddp_get_module_name(module_list[0])
		, ddp_get_module_name(module_list[module_num - 1]));
	/* check mout */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] == mout_map[j].id) {
				mout = 0;
				/* find next module which can be connected */
				step = i + 1;
				while (module_can_connect[module_list[step]].bit == 0
				       && step < module_num) {
					step++;
				}
				ASSERT(step < module_num);
				for (k = 0; k < BIT_NUM; k++) {
					if (mout_map[j].out_id_bit_map[k].m == -1)
						break;
					if (mout_map[j].out_id_bit_map[k].m == module_list[step]) {
						mout |= mout_map[j].out_id_bit_map[k].v;
						valid = 1;
						break;
					}
				}
				if (valid) {
					valid = 0;
					if ((DISP_REG_GET(mout_map[j].reg) & mout) == 0) {
						path_error += 1;
						DDPDUMP("error:%s mout, expect=0x%x, real=0x%x\n",
							ddp_get_module_name(module_list[i]),
							mout, DISP_REG_GET(mout_map[j].reg));
					} else if (DISP_REG_GET(mout_map[j].reg) != mout) {
						DDPDUMP
						    ("warning: %s mout expect=0x%x, real=0x%x\n",
						     ddp_get_module_name(module_list[i]), mout,
						     DISP_REG_GET(mout_map[j].reg));
					}
				}
				break;
			}
		}
	}
	/* check out select */
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_SEL_OUT_NUM; j++) {
			if (module_list[i] != sel_out_map[j].id)
				continue;
			/* find next module which can be connected */
			step = i + 1;
			while (module_can_connect[module_list[step]].bit == 0
			       && step < module_num) {
				step++;
			}
			ASSERT(step < module_num);
			for (k = 0; k < BIT_NUM; k++) {
				if (sel_out_map[j].id_bit_map[k] == -1)
					break;
				if (sel_out_map[j].id_bit_map[k] == module_list[step]) {
					if (DISP_REG_GET(sel_out_map[j].reg) != k) {
						path_error += 1;
						DDPDUMP
						    ("error:out_s %s not connect to %s, expect=0x%x, real=0x%x\n",
						     ddp_get_module_name(module_list[i]),
						     ddp_get_module_name(module_list[step]),
						     k, DISP_REG_GET(sel_out_map[j].reg));
					}
					break;
				}
			}
		}
	}
	/* check input select */
	for (i = 1; i < module_num; i++) {
		for (j = 0; j < DDP_SEL_IN_NUM; j++) {
			if (module_list[i] != sel_in_map[j].id)
				continue;
			/* find next module which can be connected */
			step = i - 1;
			while (module_can_connect[module_list[step]].bit == 0 && step > 0)
				step--;
			ASSERT(step >= 0);
			for (k = 0; k < BIT_NUM; k++) {
				if (sel_in_map[j].id_bit_map[k] == -1)
					break;
				if (sel_in_map[j].id_bit_map[k] == module_list[step]) {
					if (DISP_REG_GET(sel_in_map[j].reg) != k) {
						path_error += 1;
						DDPDUMP("error:in_s %s not conn to %s,expect0x%x,real0x%x\n",
						     ddp_get_module_name(module_list[step]),
						     ddp_get_module_name(module_list[i]), k,
						     DISP_REG_GET(sel_in_map[j].reg));
					}
					break;
				}
			}
		}
	}
	if (path_error == 0) {
		DDPDUMP("path: %s to %s is connected\n", ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	} else {
		DDPDUMP("path: %s to %s not connected!!!\n", ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	}
}

static void ddp_check_hw_path_l(int *module_list)
{
	unsigned int i, j, k;
	int next, prev;
	int module = 0;
	unsigned int hw_g, hw_b, hw_v;
	unsigned int hw_valid_error = 0;
	unsigned int hw_ready_error = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);
	unsigned int valid[4], ready[4];

	if (disp_helper_get_option(DISP_OPT_REG_PARSER_VALID_READY) == 0) {
		DDPDUMP("skip check_hw_path: %s to %s\n", ddp_get_module_name(module_list[0])
			, ddp_get_module_name(module_list[module_num - 1]));
		return;
	}

	valid[0] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_0);
	valid[1] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_1);
	valid[2] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_2);
	valid[3] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_VALID_3);
	ready[0] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_0);
	ready[1] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_1);
	ready[2] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_2);
	ready[3] = DISP_REG_GET(DISP_REG_CONFIG_DISP_DL_READY_3);

	DDPDUMP("valid0=0x%x, valid1=0x%x, valid2=0x%x, valid3=0x%x\n",
		valid[0], valid[1], valid[2], valid[3]);
	DDPDUMP("ready0=0x%x, ready1=0x%x, ready2=0x%x, ready3=0x%x\n",
		ready[0], ready[1], ready[2], ready[3]);
	DDPDUMP("check_hw_path: %s to %s\n", ddp_get_module_name(module_list[0])
		, ddp_get_module_name(module_list[module_num - 1]));

	/* check mout */
	for (i = 0; i < module_num; i++) {
		if (module_can_connect[module_list[i]].bit == 0)
			continue;

		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] != mout_hw_map[j].id)
				continue;

			for (k = 0; k < BIT_NUM; k++) {
				module = mout_hw_map[j].out_id_bit_map[k].m;
				if (module == -1)
					break;
				if (module_can_connect[module].bit == 0)
					continue;

				/* find next module which can be connected */
				next = i + 1;
				if (next >= module_num)
					next = i;
				while ((module_can_connect[module_list[next]].bit ==
					0) && (next < module_num)) {
					next++;
				}
				if (next >= module_num)
					next = i;

				if ((module_list[i] == module)
					|| (module_list[next] == module)) {
					hw_g = mout_hw_map[j].out_id_bit_map[k].g;
					hw_b = mout_hw_map[j].out_id_bit_map[k].b;
					hw_v = (1 << hw_b);
					if ((valid[hw_g] & hw_v) == 0) {
						hw_valid_error += 1;
						DDPDUMP
							("mout not_v:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 valid[hw_g], hw_v);
					}
					if ((ready[hw_g] & hw_v) == 0) {
						hw_ready_error += 1;
						DDPDUMP
							("mout not_r:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 ready[hw_g], hw_v);
					}
				}
			}
		}
	}
	/* check out select */
	for (i = 0; i < module_num; i++) {
		if (module_can_connect[module_list[i]].bit == 0)
			continue;

		for (j = 0; j < DDP_SEL_OUT_NUM; j++) {
			if (module_list[i] != sel_out_hw_map[j].id)
				continue;

			for (k = 0; k < BIT_NUM; k++) {
				module = sel_out_hw_map[j].out_id_bit_map[k].m;
				if (module == -1)
					break;
				if (module_can_connect[module].bit == 0)
					continue;

				/* find next module which can be connected */
				next = i + 1;
				if (next >= module_num)
					next = i;
				while ((module_can_connect[module_list[next]].bit ==
					0) && (next < module_num)) {
					next++;
				}
				if (next >= module_num)
					next = i;

				if ((module_list[i] == module)
					|| (module_list[next] == module)) {
					hw_g = sel_out_hw_map[j].out_id_bit_map[k].g;
					hw_b = sel_out_hw_map[j].out_id_bit_map[k].b;
					hw_v = (1 << hw_b);
					if ((valid[hw_g] & hw_v) == 0) {
						hw_valid_error += 1;
						DDPDUMP
							("sout not_v:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 valid[hw_g], hw_v);
					}
					if ((ready[hw_g] & hw_v) == 0) {
						hw_ready_error += 1;
						DDPDUMP
							("sout not_r:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 ready[hw_g], hw_v);
					}
				}
			}
		}
	}
	/* check input select */
	for (i = 0; i < module_num; i++) {
		if (module_can_connect[module_list[i]].bit == 0)
			continue;

		for (j = 0; j < DDP_SEL_IN_NUM; j++) {
			if (module_list[i] != sel_in_hw_map[j].id)
				continue;

			for (k = 0; k < BIT_NUM; k++) {
				module = sel_in_hw_map[j].out_id_bit_map[k].m;
				if (module == -1)
					break;
				if (module_can_connect[module].bit == 0)
					continue;

				/* find previous module which can be connected */
				prev = i - 1;
				if (prev < 0)
					prev = i;
				while ((module_can_connect[module_list[prev]].bit ==
					0) && (prev > 0)) {
					prev--;
				}
				if (prev < 0)
					prev = i;

				if ((module_list[i] == module)
					|| (module_list[prev] == module)) {
					hw_g = sel_in_hw_map[j].out_id_bit_map[k].g;
					hw_b = sel_in_hw_map[j].out_id_bit_map[k].b;
					hw_v = (1 << hw_b);
					if ((valid[hw_g] & hw_v) == 0) {
						hw_valid_error += 1;
						DDPDUMP
							("sin not_v:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 valid[hw_g], hw_v);
					}
					if ((ready[hw_g] & hw_v) == 0) {
						hw_ready_error += 1;
						DDPDUMP
							("sin not_r:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 ready[hw_g], hw_v);
					}
				}
			}
		}
	}
	/* check engine */
	for (i = 0; i < module_num; i++) {
		if (module_can_connect[module_list[i]].bit == 0)
			continue;

		for (j = 0; j < DDP_ENG_NUM; j++) {
			if (module_list[i] != eng_hw_map[j].id)
				continue;

			for (k = 0; k < BIT_NUM; k++) {
				module = eng_hw_map[j].out_id_bit_map[k].m;
				if (module == -1)
					break;
				if (module_can_connect[module].bit == 0)
					continue;

				/* find next module which can be connected */
				next = i + 1;
				if (next >= module_num)
					next = i;
				while ((module_can_connect[module_list[next]].bit ==
					0) && (next < module_num)) {
					next++;
				}
				if (next >= module_num)
					next = i;

				if ((module_list[i] == module)
					|| (module_list[next] == module)) {
					hw_g = eng_hw_map[j].out_id_bit_map[k].g;
					hw_b = eng_hw_map[j].out_id_bit_map[k].b;
					hw_v = (1 << hw_b);
					if ((valid[hw_g] & hw_v) == 0) {
						hw_valid_error += 1;
						DDPDUMP
							("eng not_v:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 valid[hw_g], hw_v);
					}
					if ((ready[hw_g] & hw_v) == 0) {
						hw_ready_error += 1;
						DDPDUMP
							("eng not_r:%s, 0x%x, 0x%x\n",
							 ddp_signal[hw_g][hw_b],
							 ready[hw_g], hw_v);
					}
				}
			}
		}
	}

	if (hw_valid_error == 0) {
		DDPDUMP("hw_v_path: %s to %s is connected\n", ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	} else {
		DDPDUMP("hw_v_path: %s to %s not connected!!!\n", ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	}
	if (hw_ready_error == 0) {
		DDPDUMP("hw_r_path: %s to %s is connected\n", ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	} else {
		DDPDUMP("hw_r_path: %s to %s not connected!!!\n", ddp_get_module_name(module_list[0]),
			ddp_get_module_name(module_list[module_num - 1]));
	}
}

static void ddp_disconnect_path_l(int *module_list, void *handle)
{
	unsigned int i, j, k;
	int step = 0;
	unsigned int mout = 0;
	unsigned int reg_mout = 0;
	unsigned int mout_idx = 0;
	unsigned int module_num = ddp_get_module_num_l(module_list);

	DDPDBG("disconnect_path: %s to %s\n", ddp_get_module_name(module_list[0]),
	       ddp_get_module_name(module_list[module_num - 1]));
	for (i = 0; i < module_num - 1; i++) {
		for (j = 0; j < DDP_MOUT_NUM; j++) {
			if (module_list[i] == mout_map[j].id) {
				/* find next module which can be connected */
				step = i + 1;
				while (module_can_connect[module_list[step]].bit == 0
				       && step < module_num) {
					step++;
				}
				ASSERT(step < module_num);
				for (k = 0; k < BIT_NUM; k++) {
					if (mout_map[j].out_id_bit_map[k].m == -1)
						break;
					if (mout_map[j].out_id_bit_map[k].m == module_list[step]) {
						mout |= mout_map[j].out_id_bit_map[k].v;
						reg_mout |= mout;
						mout_idx = j;
						DDPDBG("disconnect mout %s to %s\n",
						       ddp_get_module_name(module_list[i]),
						       ddp_get_module_name(module_list[step]));
						break;
					}
				}
				/* update mout_value */
				mout_map[j].reg_val &= ~mout;
				mout = 0;
			}
		}
		if (reg_mout) {
			DISP_REG_SET(handle, mout_map[mout_idx].reg, mout_map[mout_idx].reg_val);
			reg_mout = 0;
			mout_idx = 0;
		}
	}
}

static int ddp_get_mutex_src(enum DISP_MODULE_ENUM dest_module, enum DDP_MODE ddp_mode,
			     unsigned int *SOF_src, unsigned int *EOF_src)
{
	unsigned int src_from_dst_module = 0;

	/* CASE1: OVL -> WDMA */
	if (dest_module == DISP_MODULE_WDMA0 || dest_module == DISP_MODULE_WDMA1) {

		if (ddp_mode == DDP_VIDEO_MODE)
			DISP_LOG_W("%s: dst_mode=%s, but is video mode !!\n", __func__,
				   ddp_get_module_name(dest_module));
		if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
			if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
				/* full shadow mode: sof=eof=reserved for cmd mode to delay */
				/* signal sof until trigger path(mutex_en=1) */
				*SOF_src = SOF_VAL_MUTEX0_SOF_RESERVED;
				*EOF_src = SOF_VAL_MUTEX0_EOF_RESERVED;
			} else {
				/* force_commit, bypass_shadow should be sof=eof=single */
				*SOF_src = *EOF_src = SOF_VAL_MUTEX0_SOF_SINGLE_MODE;
			}
		}
		return 0;
	}

	/* CASE2: *** -> DSI | *** -> DPI */
	if (dest_module == DISP_MODULE_DSI0 || dest_module == DISP_MODULE_DSIDUAL) {
		src_from_dst_module = SOF_VAL_MUTEX0_SOF_FROM_DSI0;
	} else if (dest_module == DISP_MODULE_DPI) {
		src_from_dst_module = SOF_VAL_MUTEX0_SOF_FROM_DPI;
	} else {
		DDPERR("get mutex sof, invalid param dst module = %s(%d), dis mode %s\n",
		       ddp_get_module_name(dest_module), dest_module, ddp_get_mode_name(ddp_mode));
		ASSERT(0);
		return 0;
	}

	if (ddp_mode == DDP_CMD_MODE) {
		*SOF_src = SOF_VAL_MUTEX0_SOF_SINGLE_MODE;
		if (disp_helper_get_option(DISP_OPT_MUTEX_EOF_EN_FOR_CMD_MODE))
			*EOF_src = src_from_dst_module;
		else
			*EOF_src = SOF_VAL_MUTEX0_EOF_SINGLE_MODE;

		if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
			if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
				/* full shadow mode: sof=eof=reserved for cmd mode to delay */
				/* signal sof until trigger path(mutex_en=1) */
				*SOF_src = SOF_VAL_MUTEX0_SOF_RESERVED;
				*EOF_src = SOF_VAL_MUTEX0_EOF_RESERVED;
			}
		}
	} else {
		*SOF_src = *EOF_src = src_from_dst_module;
	}

	return 0;
}

/* id: mutex ID, 0~5 */
static int ddp_mutex_set_l(int mutex_id, int *module_list, enum DDP_MODE ddp_mode, void *handle)
{
	int i = 0;
	unsigned int value0 = 0;
	unsigned int value1 = 0;
	unsigned int sof_val;
	unsigned int sof_src, eof_src;
	int module_num = ddp_get_module_num_l(module_list);

	ddp_get_mutex_src(module_list[module_num - 1], ddp_mode, &sof_src, &eof_src);
	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}
	for (i = 0; i < module_num; i++) {
		if (module_mutex_map[module_list[i]].bit != -1) {
			DDPDBG("module %s added to mutex %d\n", ddp_get_module_name(module_list[i]),
			       mutex_id);
			if (module_mutex_map[module_list[i]].mod_num == 0) {
				value0 |= (1 << module_mutex_map[module_list[i]].bit);
			} else if (module_mutex_map[module_list[i]].mod_num == 1) {
				/* DISP_MODULE_DSIDUAL is special */
				if (module_mutex_map[module_list[i]].module == DISP_MODULE_DSIDUAL) {
					value1 |= (1 << module_mutex_map[DISP_MODULE_DSI0].bit);
					/* DISP MODULE enum must start from 0 */
					value1 |= (1 << module_mutex_map[DISP_MODULE_DSI1].bit);
				} else {
					value1 |= (1 << module_mutex_map[module_list[i]].bit);
				}
			}
		} else {
			DDPDBG("module %s not added to mutex %d\n",
			      ddp_get_module_name(module_list[i]), mutex_id);
		}
	}

	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_MOD0(mutex_id), value0);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_MOD1(mutex_id), value1);

	sof_val = REG_FLD_VAL(SOF_FLD_MUTEX0_SOF, sof_src);
	sof_val |= REG_FLD_VAL(SOF_FLD_MUTEX0_EOF, eof_src);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_SOF(mutex_id), sof_val);

	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER) &&
			disp_helper_get_option(DISP_OPT_SHADOW_MODE) != 0) {
		ddp_mutex_get(mutex_id, handle);
		ddp_mutex_release(mutex_id, handle);
	}

	DDPDBG("mutex %d value=0x%x, sof=%s, eof=%s\n", mutex_id,
	       value0, ddp_get_mutex_sof_name(sof_src), ddp_get_mutex_sof_name(eof_src));
	return 0;
}

static void ddp_check_mutex_l(int mutex_id, int *module_list, enum DDP_MODE ddp_mode)
{
	int i = 0;
	uint32_t real_value0 = 0;
	uint32_t real_value1 = 0;
	uint32_t expect_value0 = 0;
	uint32_t expect_value1 = 0;
	unsigned int real_sof, real_eof, val;
	unsigned int expect_sof, expect_eof;
	int module_num = ddp_get_module_num_l(module_list);

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPDUMP("error:check mutex fail:exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return;
	}
	real_value0 = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD0(mutex_id));
	real_value1 = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD1(mutex_id));
	for (i = 0; i < module_num; i++) {
		if (module_mutex_map[module_list[i]].bit != -1) {
			if (module_mutex_map[module_list[i]].mod_num == 0) {
				expect_value0 |= (1 << module_mutex_map[module_list[i]].bit);
			} else if (module_mutex_map[module_list[i]].mod_num == 1) {
				if (module_mutex_map[module_list[i]].module == DISP_MODULE_DSIDUAL) {
					expect_value1 |= (1 << module_mutex_map[DISP_MODULE_DSI0].bit);
					/* DISP MODULE enum must start from 0 */

					expect_value1 |= (1 << module_mutex_map[DISP_MODULE_DSI1].bit);
				} else {
					expect_value1 |= (1 << module_mutex_map[module_list[i]].bit);
				}
			}
		}
	}
	if (expect_value0 != real_value0)
		DDPDUMP("error:mutex %d error: expect0 0x%x, real0 0x%x\n", mutex_id, expect_value0,
			real_value0);

	if (expect_value1 != real_value1)
		DDPDUMP("error:mutex %d error: expect1 0x%x, real1 0x%x\n", mutex_id, expect_value1,
			real_value1);

	val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(mutex_id));
	real_sof = REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF, val);
	real_eof = REG_FLD_VAL_GET(SOF_FLD_MUTEX0_EOF, val);
	ddp_get_mutex_src(module_list[module_num - 1], ddp_mode, &expect_sof, &expect_eof);
	if (expect_sof != real_sof)
		DDPDUMP("error:mutex %d sof error: expect %s, real %s\n", mutex_id,
			ddp_get_mutex_sof_name(expect_sof), ddp_get_mutex_sof_name(real_sof));
	if (expect_eof != real_eof)
		DDPDUMP("error:mutex %d eof error: expect %s, real %s\n", mutex_id,
			ddp_get_mutex_sof_name(expect_eof), ddp_get_mutex_sof_name(real_eof));

}

static int ddp_mutex_enable_l(int mutex_idx, void *handle)
{
	DDPDBG("mutex %d enable\n", mutex_idx);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_EN(mutex_idx), 1);

			DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_GET(mutex_idx), 1);
			DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_GET(mutex_idx), 0);

	return 0;
}

int ddp_get_module_num(enum DDP_SCENARIO_ENUM scenario)
{
	return ddp_get_module_num_l(module_list_scenario[scenario]);
}

static void ddp_print_scenario(enum DDP_SCENARIO_ENUM scenario)
{
	int i = 0;
	char path[512] = { '\0' };
	int num = ddp_get_module_num(scenario);

	for (i = 0; i < num; i++)
		strncat(path, ddp_get_module_name(module_list_scenario[scenario][i]), sizeof(path));
	DDPMSG("scenario %s have modules: %s\n", ddp_get_scenario_name(scenario), path);
}

static int ddp_find_module_index(enum DDP_SCENARIO_ENUM ddp_scenario, enum DISP_MODULE_ENUM module)
{
	int i = 0;

	for (i = 0; i < DDP_ENING_NUM; i++) {
		if (module_list_scenario[ddp_scenario][i] == module)
			return i;

	}
	DDPDBG("find module: can not find module %s on scenario %s\n", ddp_get_module_name(module),
	       ddp_get_scenario_name(ddp_scenario));
	return -1;
}

/* set display interface when kernel init */
int ddp_set_dst_module(enum DDP_SCENARIO_ENUM scenario, enum DISP_MODULE_ENUM dst_module)
{
	int i = 0;

	DDPMSG("ddp_set_dst_module, scenario=%s, dst_module=%s\n",
	       ddp_get_scenario_name(scenario), ddp_get_module_name(dst_module));
	if (ddp_find_module_index(scenario, dst_module) > 0) {
		DDPDBG("%s is already on path\n", ddp_get_module_name(dst_module));
		return 0;
	}
	i = ddp_get_module_num_l(module_list_scenario[scenario]) - 1;
	ASSERT(i >= 0);
	if (dst_module == DISP_MODULE_DSIDUAL) {
		if (i < (DDP_ENING_NUM - 1)) {
			module_list_scenario[scenario][i++] = DISP_MODULE_SPLIT0;
		} else {
			DDPERR("set dst module over up bound\n");
			return -1;
		}
	} else {
		if (ddp_get_dst_module(scenario) == DISP_MODULE_DSIDUAL) {
			if (i >= 1) {
				module_list_scenario[scenario][i--] = -1;
			} else {
				DDPERR("set dst module over low bound\n");
				return -1;
			}
		}
	}
	module_list_scenario[scenario][i] = dst_module;
	if (scenario == DDP_SCENARIO_PRIMARY_ALL)
		ddp_set_dst_module(DDP_SCENARIO_PRIMARY_DISP, dst_module);
	else if (scenario == DDP_SCENARIO_SUB_ALL)
		ddp_set_dst_module(DDP_SCENARIO_SUB_RDMA1_DISP, dst_module);
	else if (scenario == DDP_SCENARIO_DITHER_1TO2)
		ddp_set_dst_module(DDP_SCENARIO_PRIMARY_DISP, dst_module);
	else if (scenario == DDP_SCENARIO_UFOE_1TO2)
		ddp_set_dst_module(DDP_SCENARIO_PRIMARY_DISP, dst_module);

	ddp_print_scenario(scenario);
	return 0;
}

enum DISP_MODULE_ENUM ddp_get_dst_module(enum DDP_SCENARIO_ENUM ddp_scenario)
{
	enum DISP_MODULE_ENUM module_name = DISP_MODULE_UNKNOWN;
	int module_num = ddp_get_module_num_l(module_list_scenario[ddp_scenario]) - 1;

	if (module_num >= 0)
		module_name = module_list_scenario[ddp_scenario][module_num];

	return module_name;
}

int *ddp_get_scenario_list(enum DDP_SCENARIO_ENUM ddp_scenario)
{
	return module_list_scenario[ddp_scenario];
}

int ddp_is_module_in_scenario(enum DDP_SCENARIO_ENUM ddp_scenario, enum DISP_MODULE_ENUM module)
{
	int i = 0;

	for (i = 0; i < DDP_ENING_NUM; i++) {
		if (module_list_scenario[ddp_scenario][i] == module)
			return 1;

	}
	return 0;
}

int ddp_insert_module(enum DDP_SCENARIO_ENUM ddp_scenario, enum DISP_MODULE_ENUM place,
		      enum DISP_MODULE_ENUM module)
{
	int i = DDP_ENING_NUM - 1;
	int idx = ddp_find_module_index(ddp_scenario, place);

	if (idx < 0) {
		DDPERR("error: ddp_insert_module , place=%s is not in scenario %s!\n",
		       ddp_get_module_name(place), ddp_get_scenario_name(ddp_scenario));
		return -1;
	}

	for (i = 0; i < DDP_ENING_NUM; i++) {
		if (module_list_scenario[ddp_scenario][i] == module) {
			DDPERR("error: ddp_insert_module , module=%s is already in scenario %s!\n",
			       ddp_get_module_name(module), ddp_get_scenario_name(ddp_scenario));
			return -1;
		}
	}

	/* should have empty room for insert */
	ASSERT(module_list_scenario[ddp_scenario][DDP_ENING_NUM - 1] == -1);

	for (i = DDP_ENING_NUM - 2; i >= idx; i--)
		module_list_scenario[ddp_scenario][i + 1] = module_list_scenario[ddp_scenario][i];
	module_list_scenario[ddp_scenario][idx] = module;

	{
		int *modules = ddp_get_scenario_list(ddp_scenario);
		int module_num = ddp_get_module_num(ddp_scenario);

		DDPMSG("after insert module, module list is:\n");
		for (i = 0; i < module_num; i++)
			DDPMSG("%s-", ddp_get_module_name(modules[i]));
	}

	return 0;
}

int ddp_remove_module(enum DDP_SCENARIO_ENUM ddp_scenario, enum DISP_MODULE_ENUM module)
{
	int i = 0;
	int idx = ddp_find_module_index(ddp_scenario, module);

	if (idx < 0) {
		DDPERR("ddp_remove_module, can not find module %s in scenario %s\n",
		       ddp_get_module_name(module), ddp_get_scenario_name(ddp_scenario));
		return -1;
	}

	for (i = idx; i < DDP_ENING_NUM - 1; i++)
		module_list_scenario[ddp_scenario][i] = module_list_scenario[ddp_scenario][i + 1];
	module_list_scenario[ddp_scenario][DDP_ENING_NUM - 1] = -1;

	{
		int *modules = ddp_get_scenario_list(ddp_scenario);
		int module_num = ddp_get_module_num(ddp_scenario);

		DDPMSG("after remove module, module list is:\n");
		for (i = 0; i < module_num; i++)
			DDPMSG("%s-", ddp_get_module_name(modules[i]));
	}
	return 0;
}

void ddp_connect_path(enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	DDPDBG("path connect on scenario %s\n", ddp_get_scenario_name(scenario));
	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP], handle);
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT], handle);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_SUB_DISP], handle);
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT], handle);
	} else if (scenario == DDP_SCENARIO_DITHER_1TO2) {
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP], handle);
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DITHER_MEMOUT], handle);
	} else if (scenario == DDP_SCENARIO_UFOE_1TO2) {
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP], handle);
		ddp_connect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_UFOE_MEMOUT], handle);
	} else {
		ddp_connect_path_l(module_list_scenario[scenario], handle);
	}

}

void ddp_disconnect_path(enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	DDPDBG("path disconnect on scenario %s\n", ddp_get_scenario_name(scenario));

	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP], handle);
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT],
				      handle);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_SUB_DISP], handle);
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT], handle);
	} else if (scenario == DDP_SCENARIO_DITHER_1TO2) {
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP], handle);
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DITHER_MEMOUT], handle);
	} else if (scenario == DDP_SCENARIO_UFOE_1TO2) {
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP], handle);
		ddp_disconnect_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_UFOE_MEMOUT], handle);
	} else {
		ddp_disconnect_path_l(module_list_scenario[scenario], handle);
	}

}

void ddp_check_path(enum DDP_SCENARIO_ENUM scenario)
{
	DDPDBG("path check path on scenario %s\n", ddp_get_scenario_name(scenario));

	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP]);
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT]);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_SUB_DISP]);
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT]);
	} else if (scenario == DDP_SCENARIO_DITHER_1TO2) {
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP]);
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DITHER_MEMOUT]);
	} else if (scenario == DDP_SCENARIO_UFOE_1TO2) {
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP]);
		ddp_check_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_UFOE_MEMOUT]);
	} else {
		ddp_check_path_l(module_list_scenario[scenario]);
	}

}

void ddp_check_hw_path(enum DDP_SCENARIO_ENUM scenario)
{
	DDPDBG("path check hw path on scenario %s\n", ddp_get_scenario_name(scenario));

	if (scenario == DDP_SCENARIO_PRIMARY_ALL) {
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP]);
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_OVL_MEMOUT]);
	} else if (scenario == DDP_SCENARIO_SUB_ALL) {
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_SUB_DISP]);
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_SUB_OVL_MEMOUT]);
	} else if (scenario == DDP_SCENARIO_DITHER_1TO2) {
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP]);
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DITHER_MEMOUT]);
	} else if (scenario == DDP_SCENARIO_UFOE_1TO2) {
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_DISP]);
		ddp_check_hw_path_l(module_list_scenario[DDP_SCENARIO_PRIMARY_UFOE_MEMOUT]);
	} else {
		ddp_check_hw_path_l(module_list_scenario[scenario]);
	}

}

void ddp_check_mutex(int mutex_id, enum DDP_SCENARIO_ENUM scenario, enum DDP_MODE mode)
{
	DDPDBG("check mutex %d on scenario %s\n", mutex_id, ddp_get_scenario_name(scenario));
	ddp_check_mutex_l(mutex_id, module_list_scenario[scenario], mode);
}

int ddp_mutex_set(int mutex_id, enum DDP_SCENARIO_ENUM scenario, enum DDP_MODE mode, void *handle)
{
	if (scenario < DDP_SCENARIO_MAX)
		return ddp_mutex_set_l(mutex_id, module_list_scenario[scenario], mode, handle);

	DDPERR("Invalid scenario %d when setting mutex\n", scenario);
	return -1;
}

int ddp_mutex_Interrupt_enable(int mutex_id, void *handle)
{

	DDPDBG("mutex %d interrupt enable\n", mutex_id);
	DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_INTEN, 0x1 << mutex_id, 0x1 << mutex_id);
	DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_INTEN, 1 << (mutex_id + DISP_MUTEX_TOTAL),
		      0x1 << (mutex_id + DISP_MUTEX_TOTAL));
	return 0;
}

int ddp_mutex_Interrupt_disable(int mutex_id, void *handle)
{
	DDPDBG("mutex %d interrupt disenable\n", mutex_id);
	DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_INTEN, 0, 0x1 << mutex_id);
	DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_INTEN, 0, 0x1 << (mutex_id + DISP_MUTEX_TOTAL));
	return 0;
}

int ddp_mutex_reset(int mutex_id, void *handle)
{
	DDPDBG("mutex %d reset\n", mutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_RST(mutex_id), 1);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_RST(mutex_id), 0);

	return 0;
}

int ddp_is_moudule_in_mutex(int mutex_id, enum DISP_MODULE_ENUM module)
{
	int ret = 0;
	uint32_t real_value = 0;

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPDUMP("error:check_moudule_in_mute fail:exceed mutex max (0 ~ %d)\n",
			DISP_MUTEX_DDP_LAST);
		return ret;
	}

	if (module_mutex_map[module].mod_num == 0)
		real_value = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD0(mutex_id));
	else if (module_mutex_map[module].mod_num == 1)
		real_value = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD1(mutex_id));


	if (1 == ((real_value >> module_mutex_map[module].bit) & 0x01))
		ret = 1;

	return ret;
}


int ddp_mutex_add_module(int mutex_id, enum DISP_MODULE_ENUM module, void *handle)
{
	int value = 0;

	if (module < DISP_MODULE_UNKNOWN) {
		if (module_mutex_map[module].bit != -1) {
			DDPDBG("module %s added to mutex %d\n", ddp_get_module_name(module),
			       mutex_id);
			value |= (1 << module_mutex_map[module].bit);
			if (module_mutex_map[module].mod_num == 0)
				DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_MOD0(mutex_id), value, value);
			else if (module_mutex_map[module].mod_num == 1)
				DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_MOD1(mutex_id), value, value);

		}
	}
	return 0;
}

int ddp_mutex_remove_module(int mutex_id, enum DISP_MODULE_ENUM module, void *handle)
{
	int value = 0;

	if (module < DISP_MODULE_UNKNOWN) {
		if (module_mutex_map[module].bit != -1) {
			DDPDBG("module %s added to mutex %d\n", ddp_get_module_name(module),
			       mutex_id);
			value |= (1 << module_mutex_map[module].bit);

			if (module_mutex_map[module].mod_num == 0)
				DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_MOD0(mutex_id), 0, value);
			else if (module_mutex_map[module].mod_num == 1)
				DISP_REG_MASK(handle, DISP_REG_CONFIG_MUTEX_MOD1(mutex_id), 0, value);

		}

	}
	return 0;
}

int ddp_mutex_clear(int mutex_id, void *handle)
{
	DDPDBG("mutex %d clear\n", mutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_MOD0(mutex_id), 0);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_MOD1(mutex_id), 0);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_SOF(mutex_id), 0);

	/*reset mutex */
	ddp_mutex_reset(mutex_id, handle);
	return 0;
}

int ddp_mutex_enable(int mutex_id, enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	return ddp_mutex_enable_l(mutex_id, handle);
}

int ddp_mutex_disable(int mutex_id, enum DDP_SCENARIO_ENUM scenario, void *handle)
{
	DDPDBG("mutex %d disable\n", mutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_EN(mutex_id), 0);
	return 0;
}

int ddp_mutex_get(int mutex_id, void *handle)
{
	DDPDBG("mutex %d get\n", mutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_GET(mutex_id), 1);
	/* polling internal mutex is taken by sw */
#if 1
	DISP_REG_CMDQ_POLLING(handle, DISP_REG_CONFIG_MUTEX_GET(mutex_id),
			REG_FLD_VAL(GET_FLD_INT_MUTEX0_EN, 1), REG_FLD_MASK(GET_FLD_INT_MUTEX0_EN));
#endif
	return 0;
}

int ddp_mutex_release(int mutex_id, void *handle)
{
	DDPDBG("mutex %d release\n", mutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_GET(mutex_id), 0);
	/* polling internal mutex is released */
#if 0
	DISP_REG_CMDQ_POLLING(handle, DISP_REG_CONFIG_MUTEX_GET(mutex_id),
			REG_FLD_VAL(GET_FLD_INT_MUTEX0_EN, 1), REG_FLD_MASK(GET_FLD_INT_MUTEX0_EN));
#endif
	return 0;
}

int ddp_mutex_set_sof_wait(int mutex_id, struct cmdqRecStruct *handle, int wait)
{
	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}

	DISP_REG_SET_FIELD(handle, SOF_FLD_MUTEX0_SOF_WAIT, DISP_REG_CONFIG_MUTEX_SOF(mutex_id), wait);
	return 0;
}


int ddp_check_engine_status(int mutexID)
{
	/* check engines' clock bit &  enable bit & status bit before unlock mutex */
	/* should not needed, in comdq do? */
	int result = 0;
	return result;
}

int ddp_path_top_clock_on(void)
{
#ifdef ENABLE_CLK_MGR
	static int need_enable;

	DDPMSG("ddp path top clock on\n");

	if (need_enable) {
		if (disp_helper_get_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK))
			;/*ddp_clk_prepare_enable(MM_VENCPLL);*/
		ddp_clk_prepare_enable(DISP_MTCMOS_CLK);
		/* SMI_COMMON has 8 clocks */
		ddp_clk_prepare_enable(DISP0_SMI_COMMON);
		ddp_clk_prepare_enable(DISP0_SMI_COMMON_2X);
		ddp_clk_prepare_enable(GALS_M0_2X);
		ddp_clk_prepare_enable(GALS_M1_2X);
		ddp_clk_prepare_enable(UPSZ0);
		ddp_clk_prepare_enable(UPSZ1);
		ddp_clk_prepare_enable(FIFO0);
		ddp_clk_prepare_enable(FIFO1);
		ddp_clk_prepare_enable(DISP0_SMI_LARB0);
		ddp_clk_prepare_enable(DISP0_SMI_LARB1);
	} else {
		need_enable = 1;
		if (disp_helper_get_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK))
			;/*ddp_clk_prepare_enable(MM_VENCPLL);*/
	}
	/* enable_clock(MT_CG_DISP0_MUTEX_32K   , "DDP_MUTEX"); */
	DDPMSG("ddp CG:%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif
	return 0;
}

int ddp_path_top_clock_off(void)
{
#ifdef ENABLE_CLK_MGR
	ddp_clk_disable_unprepare(DISP0_SMI_LARB1);
	ddp_clk_disable_unprepare(DISP0_SMI_LARB0);
	ddp_clk_disable_unprepare(FIFO1);
	ddp_clk_disable_unprepare(FIFO0);
	ddp_clk_disable_unprepare(UPSZ1);
	ddp_clk_disable_unprepare(UPSZ0);
	ddp_clk_disable_unprepare(GALS_M1_2X);
	ddp_clk_disable_unprepare(GALS_M0_2X);
	ddp_clk_disable_unprepare(DISP0_SMI_COMMON_2X);
	ddp_clk_disable_unprepare(DISP0_SMI_COMMON);
	ddp_clk_disable_unprepare(DISP_MTCMOS_CLK);
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK))
		;/*ddp_clk_disable_unprepare(MM_VENCPLL);*/

#endif
	return 0;
}

int ddp_insert_config_allow_rec(void *handle)
{
	int ret = 0;

	if (handle == NULL)
		ASSERT(0);

	if (primary_display_is_video_mode())
		ret = cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		ret = cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);

	return ret;
}

int ddp_insert_config_dirty_rec(void *handle)
{
	int ret = 0;

	if (handle == NULL)
		ASSERT(0);

	if (primary_display_is_video_mode()) /* TODO: modify this */
		;/* do nothing */
	else
		ret = cmdqRecSetEventToken(handle, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

	return ret;
}

int disp_get_dst_module(enum DDP_SCENARIO_ENUM scenario)
{
	return ddp_get_dst_module(scenario);
}

int ddp_convert_ovl_input_to_rdma(struct RDMA_CONFIG_STRUCT *rdma_cfg, struct OVL_CONFIG_STRUCT *ovl_cfg,
					int dst_w, int dst_h)
{
	unsigned int Bpp = ufmt_get_Bpp(ovl_cfg->fmt);
	unsigned int offset;

	rdma_cfg->dst_y = ovl_cfg->dst_y;
	rdma_cfg->dst_x = ovl_cfg->dst_x;
	rdma_cfg->dst_h = dst_h;
	rdma_cfg->dst_w = dst_w;
	rdma_cfg->inputFormat = ovl_cfg->fmt;
	offset = ovl_cfg->src_x * Bpp + ovl_cfg->src_y * ovl_cfg->src_pitch;
	rdma_cfg->address = ovl_cfg->addr + offset;
	rdma_cfg->pitch = ovl_cfg->src_pitch;
	rdma_cfg->width = ovl_cfg->dst_w;
	rdma_cfg->height = ovl_cfg->dst_h;
	rdma_cfg->security = ovl_cfg->security;
	rdma_cfg->yuv_range = ovl_cfg->yuv_range;
	return 0;
}


