/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#include <linux/regulator/consumer.h>
#include "mtk_cpufreq_platform.h"
#include "../../mtk_cpufreq_hybrid.h"
/* #include "mach/mt_freqhopping.h" */

static struct regulator *regulator_proc1;
static struct regulator *regulator_proc2;
static struct regulator *regulator_sram1;
static struct regulator *regulator_sram2;

#if defined(CONFIG_OF)
static unsigned long mcucfg_base;
static unsigned long topckgen_base;
static unsigned long mcumixed_base;

#define MCUCFG_NODE			"mediatek,mcucfg" /* 0x10200000 */
#define TOPCKGEN_NODE		"mediatek,topckgen" /* 0x10210000 */
#define MCUMIXED_NODE		"mediatek,mcumixed" /* 0x10212000 */

#undef MCUCFG_BASE
#undef TOPCKGEN_BASE
#undef MCUMIXED_BASE

#define MCUCFG__BASE		(mcucfg_base)
#define TOPCKGEN_BASE		(topckgen_base)
#define MCUMIXED_BASE		(mcumixed_base)

#else				/* #if defined (CONFIG_OF) */
#undef MCUCFG__BASE
#undef TOPCKGEN_BASE
#undef MCUMIXED_BASE

#define MCUCFG__BASE        0xF0200000
#define TOPCKGEN_BASE       0xF0210000
#define MCUMIXED_BASE       0xF0212000
#endif				/* #if defined (CONFIG_OF) */

/* LL */
#define ARMPLL1_CON0	(MCUMIXED_BASE + 0x310)
#define ARMPLL1_CON1	(MCUMIXED_BASE + 0x314)
#define ARMPLL1_CON2	(MCUMIXED_BASE + 0x318)

/* L */
#define ARMPLL2_CON0	(MCUMIXED_BASE + 0x320)
#define ARMPLL2_CON1	(MCUMIXED_BASE + 0x324)
#define ARMPLL2_CON2	(MCUMIXED_BASE + 0x328)

/* B */
#define ARMPLL3_CON0	(MCUMIXED_BASE + 0x350)
#define ARMPLL3_CON1	(MCUMIXED_BASE + 0x354)
#define ARMPLL3_CON2	(MCUMIXED_BASE + 0x358)

/* CCI */
#define CCIPLL_CON0		(MCUMIXED_BASE + 0x348)
#define CCIPLL_CON1		(MCUMIXED_BASE + 0x34C)
#define CCIPLL_CON2		(MCUMIXED_BASE + 0x350)

/* ARMPLL DIV */
/* [10:9] MUXSEL */
/* [21:17] CLK_DIV */
#define TOP_CKDIV_LL	(MCUCFG__BASE + 0x7A0)
#define TOP_CKDIV_L		(MCUCFG__BASE + 0x7A4)
#define TOP_CKDIV_B		(MCUCFG__BASE + 0x7A8)
#define TOP_CKDIV_BUS	(MCUCFG__BASE + 0x7C0)

/* TOPCKGEN Register */
#undef CLK_MISC_CFG_0
#define CLK_MISC_CFG_0  (TOPCKGEN_BASE + 0x100)

struct mt_cpu_dvfs cpu_dvfs[] = {
	[MT_CPU_DVFS_LL] = {
				.name = __stringify(MT_CPU_DVFS_LL),
				.cpu_id = MT_CPU_DVFS_LL,
				.idx_normal_max_opp = -1,
				.idx_opp_ppm_base = -1,
				.idx_opp_ppm_limit = -1,
				.Vproc_buck_id = BUCK_ISL91302a_VPROC1,
				.Vsram_buck_id = BUCK_MT6335_VSRAM1,
				.Pll_id = PLL_LL_CLUSTER,
				},

	[MT_CPU_DVFS_L] = {
				.name = __stringify(MT_CPU_DVFS_L),
				.cpu_id = MT_CPU_DVFS_L,
				.idx_normal_max_opp = -1,
				.idx_opp_ppm_base = -1,
				.idx_opp_ppm_limit = -1,
				.Vproc_buck_id = BUCK_ISL91302a_VPROC2,
				.Vsram_buck_id = BUCK_MT6335_VSRAM2,
				.Pll_id = PLL_L_CLUSTER,
				},

	[MT_CPU_DVFS_B] = {
				.name = __stringify(MT_CPU_DVFS_B),
				.cpu_id = MT_CPU_DVFS_B,
				.idx_normal_max_opp = -1,
				.idx_opp_ppm_base = -1,
				.idx_opp_ppm_limit = -1,
				.Vproc_buck_id = BUCK_ISL91302a_VPROC1,
				.Vsram_buck_id = BUCK_MT6335_VSRAM1,
				.Pll_id = PLL_B_CLUSTER,
				},

	[MT_CPU_DVFS_CCI] = {
				.name = __stringify(MT_CPU_DVFS_CCI),
				.cpu_id = MT_CPU_DVFS_CCI,
				.idx_normal_max_opp = -1,
				.idx_opp_ppm_base = -1,
				.idx_opp_ppm_limit = -1,
				.Vproc_buck_id = BUCK_ISL91302a_VPROC1,
				.Vsram_buck_id = BUCK_MT6335_VSRAM1,
				.Pll_id = PLL_CCI_CLUSTER,
				},
};

#ifdef CONFIG_HYBRID_CPU_DVFS
unsigned int get_cur_volt_mcu(struct buck_ctrl_t *buck_p)
{
	unsigned int volt = 0;

	volt = cpuhvfs_get_volt((int)buck_p->buck_id);

	return volt;
}

int set_cur_volt_mcu(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	cpufreq_err("No way to set volt in secure mode\n");

	return 0;
}

unsigned int get_cur_freq_mcu(struct pll_ctrl_t *pll_p)
{
	unsigned int freq = 0;

	freq = cpuhvfs_get_freq((int)pll_p->pll_id);

	return freq;
}
#endif

static struct buck_ctrl_ops buck_ops_isl91302_vproc1 = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_volt = get_cur_volt_mcu,
	.set_cur_volt = set_cur_volt_mcu,
#else
	.get_cur_volt = get_cur_volt_proc_cpu1,
	.set_cur_volt = set_cur_volt_proc_cpu1,
#endif
	.transfer2pmicval = ISL191302_transfer2pmicval,
	.transfer2volt = ISL191302_transfer2volt,
	.settletime = ISL191302_settletime,
};

static struct buck_ctrl_ops buck_ops_isl91302_vproc2 = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_volt = get_cur_volt_mcu,
	.set_cur_volt = set_cur_volt_mcu,
#else
	.get_cur_volt = get_cur_volt_proc_cpu2,
	.set_cur_volt = set_cur_volt_proc_cpu2,
#endif
	.transfer2pmicval = ISL191302_transfer2pmicval,
	.transfer2volt = ISL191302_transfer2volt,
	.settletime = ISL191302_settletime,
};

static struct buck_ctrl_ops buck_ops_mt6335_vsram1 = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_volt = get_cur_volt_mcu,
	.set_cur_volt = set_cur_volt_mcu,
#else
	.get_cur_volt = get_cur_volt_sram_cpu1,
	.set_cur_volt = set_cur_volt_sram_cpu1,
#endif
	.transfer2pmicval = MT6335_transfer2pmicval,
	.transfer2volt = MT6335_transfer2volt,
	.settletime = MT6335_settletime,
};

static struct buck_ctrl_ops buck_ops_mt6335_vsram2 = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_volt = get_cur_volt_mcu,
	.set_cur_volt = set_cur_volt_mcu,
#else
	.get_cur_volt = get_cur_volt_sram_cpu2,
	.set_cur_volt = set_cur_volt_sram_cpu2,
#endif
	.transfer2pmicval = MT6335_transfer2pmicval,
	.transfer2volt = MT6335_transfer2volt,
	.settletime = MT6335_settletime,
};

struct buck_ctrl_t buck_ctrl[] = {
	[BUCK_ISL91302a_VPROC1] = {
				.name = __stringify(BUCK_ISL91302a_VPROC1),
				.buck_id = BUCK_ISL91302a_VPROC1,
				.buck_ops = &buck_ops_isl91302_vproc1,
				},

	[BUCK_ISL91302a_VPROC2] = {
				.name = __stringify(BUCK_ISL91302a_VPROC2),
				.buck_id = BUCK_ISL91302a_VPROC2,
				.buck_ops = &buck_ops_isl91302_vproc2,
				},
	[BUCK_MT6335_VSRAM1] = {
				.name = __stringify(BUCK_MT6335_VSRAM1),
				.buck_id = BUCK_MT6335_VSRAM1,
				.buck_ops = &buck_ops_mt6335_vsram1,
				},
	[BUCK_MT6335_VSRAM2] = {
				.name = __stringify(BUCK_MT6335_VSRAM2),
				.buck_id = BUCK_MT6335_VSRAM2,
				.buck_ops = &buck_ops_mt6335_vsram2,
				},
};

int set_cur_volt_proc_cpu1(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	regulator_set_voltage(regulator_proc1, volt * 10, MAX_VPROC_VOLT);
	return 0;
}

int set_cur_volt_proc_cpu2(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	regulator_set_voltage(regulator_proc2, volt * 10, MAX_VPROC_VOLT);
	return 0;
}

unsigned int get_cur_volt_proc_cpu1(struct buck_ctrl_t *buck_p)
{
	unsigned int rdata = 0;

	rdata = regulator_get_voltage(regulator_proc1);

	return rdata;
}

unsigned int get_cur_volt_proc_cpu2(struct buck_ctrl_t *buck_p)
{
	unsigned int rdata = 0;

	rdata = regulator_get_voltage(regulator_proc2);

	return rdata;
}

unsigned int ISL191302_transfer2pmicval(unsigned int volt)
{
	return (volt * 1024 / 1231000);
}

unsigned int ISL191302_transfer2volt(unsigned int val)
{
	return ((1231000 * val) / 1024);
}

unsigned int ISL191302_settletime(unsigned int old_mv, unsigned int new_mv)
{
	if (new_mv > old_mv)
		return (((((new_mv) - (old_mv)) + 1250 - 1) / 1250) + PMIC_CMD_DELAY_TIME);
	else if (old_mv > new_mv)
		return (((((old_mv) - (new_mv)) + 1250 - 1) / 1250) + PMIC_CMD_DELAY_TIME);
	else
		return 0;
}

int set_cur_volt_sram_cpu1(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	regulator_set_voltage(regulator_sram1, volt * 10, MAX_VSRAM_VOLT);

	return 0;
}

unsigned int get_cur_volt_sram_cpu1(struct buck_ctrl_t *buck_p)
{
	unsigned int rdata = 0;

	rdata = regulator_get_voltage(regulator_sram1);

	return rdata;
}

int set_cur_volt_sram_cpu2(struct buck_ctrl_t *buck_p, unsigned int volt)
{
	regulator_set_voltage(regulator_sram2, volt * 10, MAX_VSRAM_VOLT);

	return 0;
}

unsigned int get_cur_volt_sram_cpu2(struct buck_ctrl_t *buck_p)
{
	unsigned int rdata = 0;

	rdata = regulator_get_voltage(regulator_sram2);

	return rdata;
}

unsigned int MT6335_transfer2pmicval(unsigned int volt)
{
	return (((((volt) - 40000) + 625 - 1) / 625) & 0x7F);
}

unsigned int MT6335_transfer2volt(unsigned int val)
{
	return (40000 + ((val) & 0x7F) * 625);
}

unsigned int MT6335_settletime(unsigned int old_volt, unsigned int new_volt)
{
	if (new_volt > old_volt)
		return (((((new_volt) - (old_volt)) + 1250 - 1) / 1250) + PMIC_CMD_DELAY_TIME);
	else
		return (((((old_volt) - (new_volt)) * 2)  / 625) + PMIC_CMD_DELAY_TIME);
}

int __attribute__((weak)) sync_dcm_set_mp0_freq(unsigned int mhz)
{
	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp1_freq(unsigned int mhz)
{
	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp2_freq(unsigned int mhz)
{
	return 0;
}

int __attribute__((weak)) sync_dcm_set_cci_freq(unsigned int mhz)
{
	return 0;
}

/* pll ctrl configs */
static struct pll_ctrl_ops pll_ops_ll = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_freq = get_cur_freq_mcu,
#else
	.get_cur_freq = get_cur_phy_freq,
#endif
	.set_armpll_dds = adjust_armpll_dds,
	.set_armpll_posdiv = adjust_posdiv,
	.set_armpll_clkdiv = adjust_clkdiv,
	.set_freq_hopping = adjust_freq_hopping,
	.clksrc_switch = _cpu_clock_switch,
	.get_clksrc = _get_cpu_clock_switch,
	.set_sync_dcm = sync_dcm_set_mp0_freq,
};

static struct pll_ctrl_ops pll_ops_l = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_freq = get_cur_freq_mcu,
#else
	.get_cur_freq = get_cur_phy_freq,
#endif
	.set_armpll_dds = adjust_armpll_dds,
	.set_armpll_posdiv = adjust_posdiv,
	.set_armpll_clkdiv = adjust_clkdiv,
	.set_freq_hopping = adjust_freq_hopping,
	.clksrc_switch = _cpu_clock_switch,
	.get_clksrc = _get_cpu_clock_switch,
	.set_sync_dcm = sync_dcm_set_mp1_freq,
};

static struct pll_ctrl_ops pll_ops_cci = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_freq = get_cur_freq_mcu,
#else
	.get_cur_freq = get_cur_phy_freq,
#endif
	.set_armpll_dds = adjust_armpll_dds,
	.set_armpll_posdiv = adjust_posdiv,
	.set_armpll_clkdiv = adjust_clkdiv,
	.set_freq_hopping = adjust_freq_hopping,
	.clksrc_switch = _cpu_clock_switch,
	.get_clksrc = _get_cpu_clock_switch,
	.set_sync_dcm = sync_dcm_set_cci_freq,
};

static struct pll_ctrl_ops pll_ops_b = {
#ifdef CONFIG_HYBRID_CPU_DVFS
	.get_cur_freq = get_cur_freq_mcu,
#else
	.get_cur_freq = get_cur_phy_freq,
#endif
	.set_armpll_dds = adjust_armpll_dds,
	.set_armpll_posdiv = adjust_posdiv,
	.set_armpll_clkdiv = adjust_clkdiv,
	.set_freq_hopping = adjust_freq_hopping,
	.clksrc_switch = _cpu_clock_switch,
	.get_clksrc = _get_cpu_clock_switch,
	.set_sync_dcm = sync_dcm_set_mp2_freq,
};

struct pll_ctrl_t pll_ctrl[] = {
	[PLL_LL_CLUSTER] = {
				.name = __stringify(PLL_LL_CLUSTER),
				.pll_id = PLL_LL_CLUSTER,
				/* Fix me .hopping_id = FH_PLL0, */ /*ARMPLL1*/
				.armpll_div_l = 17,
				.armpll_div_h = 21,
				.pll_muxsel_l = 9,
				.pll_muxsel_h = 10,
				.pll_ops = &pll_ops_ll,
				},

	[PLL_L_CLUSTER] = {
				.name = __stringify(PLL_L_CLUSTER),
				.pll_id = PLL_L_CLUSTER,
				/* Fix me .hopping_id = FH_PLL1, */ /*ARMPLL2*/
				.armpll_div_l = 17,
				.armpll_div_h = 21,
				.pll_muxsel_l = 9,
				.pll_muxsel_h = 10,
				.pll_ops = &pll_ops_l,
				},
	[PLL_CCI_CLUSTER] = {
				.name = __stringify(PLL_CCI_CLUSTER),
				.pll_id = PLL_CCI_CLUSTER,
				/* Fix me .hopping_id = FH_PLL3, */ /*CCIPLL*/
				.armpll_div_l = 17,
				.armpll_div_h = 21,
				.pll_muxsel_l = 9,
				.pll_muxsel_h = 10,
				.pll_ops = &pll_ops_cci,
				},
	[PLL_B_CLUSTER] = {
				.name = __stringify(PLL_B_CLUSTER),
				.pll_id = PLL_B_CLUSTER,
				/* Fix me .hopping_id = FH_PLL2, */ /*ARMPLL3*/
				.armpll_div_l = 17,
				.armpll_div_h = 21,
				.pll_muxsel_l = 9,
				.pll_muxsel_h = 10,
				.pll_ops = &pll_ops_b,
				},
};

/* PLL Part */
void prepare_pll_addr(enum mt_cpu_dvfs_pll_id pll_id)
{
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(pll_id);

	pll_p->armpll_addr = ((pll_id == PLL_LL_CLUSTER) ? (unsigned int *)ARMPLL1_CON1 :
			  (pll_id == PLL_L_CLUSTER) ? (unsigned int *)ARMPLL2_CON1 :
			  (pll_id == PLL_B_CLUSTER) ? (unsigned int *)ARMPLL3_CON1 : (unsigned int *)CCIPLL_CON1);

	pll_p->armpll_div_addr = ((pll_id == PLL_LL_CLUSTER) ? (unsigned int *)TOP_CKDIV_LL :
			  (pll_id == PLL_L_CLUSTER) ? (unsigned int *)TOP_CKDIV_L :
			  (pll_id == PLL_B_CLUSTER) ? (unsigned int *)TOP_CKDIV_B : (unsigned int *)TOP_CKDIV_BUS);
}

#define POS_SETTLE_TIME (2)
#define PLL_SETTLE_TIME	(20)
unsigned int _cpu_dds_calc(unsigned int khz)
{
	unsigned int dds = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	dds = ((khz / 1000) << 14) / 26;
	/* dds = ((khz / 1000) << 18) / 26; */

	FUNC_EXIT(FUNC_LV_HELP);

	return dds;
}

/* 0.85V */
#define MAIN_PLL_VOLT_IDX 1
void adjust_armpll_dds(struct pll_ctrl_t *pll_p, unsigned int vco, unsigned int pos_div)
{
	unsigned int dds;
	int shift;
	unsigned int reg;

#if 0
	int i;
	struct mt_cpu_dvfs *p;
	struct buck_ctrl_t *vproc_p = NULL;
	unsigned int cur_volt = 0;
	int restore_volt = 0;

	for_each_cpu_dvfs(i, p) {
		if (p->Pll_id == pll_p->pll_id) {
			vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
			break;
		}
	}

	if (vproc_p)
		cur_volt = vproc_p->buck_ops->get_cur_volt(vproc_p);

	if (cur_volt < cpu_dvfs_get_volt_by_idx(p, MAIN_PLL_VOLT_IDX)) {
		restore_volt = 1;
		set_cur_volt_wrapper(p, cpu_dvfs_get_volt_by_idx(p, MAIN_PLL_VOLT_IDX));
	}

	pll_p->pll_ops->clksrc_switch(pll_p, TOP_CKMUXSEL_MAINPLL);
#endif
	shift = (pos_div == 1) ? 0 :
		(pos_div == 2) ? 1 :
		(pos_div == 4) ? 2 : 0;

	dds = _cpu_dds_calc(vco);
	/* dds = _GET_BITS_VAL_(20:0, _cpu_dds_calc(vco)); */
	reg = cpufreq_read(pll_p->armpll_addr);

	dds = (((reg & ~(_BITMASK_(30:28))) | (shift << 28)) & ~(_BITMASK_(21:0))) | dds;
	cpufreq_write(pll_p->armpll_addr, dds | _BIT_(31)); /* CHG */

#if 0
	udelay(PLL_SETTLE_TIME);
	pll_p->pll_ops->clksrc_switch(pll_p, TOP_CKMUXSEL_ARMPLL);

	if (restore_volt > 0)
		set_cur_volt_wrapper(p, cur_volt);
#endif
}

void adjust_posdiv(struct pll_ctrl_t *pll_p, unsigned int pos_div)
{
	unsigned int dds;
	int shift;
	unsigned int reg;

#if 0
	int i;
	struct mt_cpu_dvfs *p;
	struct buck_ctrl_t *vproc_p = NULL;
	unsigned int cur_volt = 0;
	int restore_volt = 0;

	for_each_cpu_dvfs(i, p) {
		if (p->Pll_id == pll_p->pll_id) {
			vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
			break;
		}
	}

	if (vproc_p)
		cur_volt = vproc_p->buck_ops->get_cur_volt(vproc_p);

	if (cur_volt < cpu_dvfs_get_volt_by_idx(p, MAIN_PLL_VOLT_IDX)) {
		restore_volt = 1;
		set_cur_volt_wrapper(p, cpu_dvfs_get_volt_by_idx(p, MAIN_PLL_VOLT_IDX));
	}

	pll_p->pll_ops->clksrc_switch(pll_p, TOP_CKMUXSEL_MAINPLL);
#endif

	shift = (pos_div == 1) ? 0 :
		(pos_div == 2) ? 1 :
		(pos_div == 4) ? 2 : 0;

	reg = cpufreq_read(pll_p->armpll_addr);
	dds = (reg & ~(_BITMASK_(30:28))) | (shift << 28);
	/* dbg_print("DVFS: Set POSDIV CON1: 0x%x as 0x%x\n", pll_p->armpll_addr, dds | _BIT_(31)); */
	cpufreq_write(pll_p->armpll_addr, dds | _BIT_(31)); /* CHG */

#if 0
	udelay(POS_SETTLE_TIME);
	pll_p->pll_ops->clksrc_switch(pll_p, TOP_CKMUXSEL_ARMPLL);

	if (restore_volt > 0)
		set_cur_volt_wrapper(p, cur_volt);
#endif
}

void adjust_clkdiv(struct pll_ctrl_t *pll_p, unsigned int clk_div)
{
	unsigned int sel = 0;

	sel = (clk_div == 1) ? 8 :
		(clk_div == 2) ? 10 :
		(clk_div == 4) ? 11 : 8;

	cpufreq_write_mask(pll_p->armpll_div_addr, pll_p->armpll_div_h : pll_p->armpll_div_l, sel);
	udelay(POS_SETTLE_TIME);
}

void adjust_freq_hopping(struct pll_ctrl_t *pll_p, unsigned int dds)
{
}

/* Frequency API */
static unsigned int _cpu_freq_calc(unsigned int con1, unsigned int ckdiv1)
{
	unsigned int freq = 0;
	unsigned int posdiv = 0;

	posdiv = _GET_BITS_VAL_(30:28, con1);

	con1 &= _BITMASK_(21:0);
	/* freq = ((con1 * 26) >> 18) * 1000; */
	freq = ((con1 * 26) >> 14) * 1000;

	FUNC_ENTER(FUNC_LV_HELP);

	switch (posdiv) {
	case 0:
		break;
	case 1:
		freq = freq / 2;
		break;
	case 2:
		freq = freq / 4;
		break;
	default:
		break;
	};

	switch (ckdiv1) {
	case 10:
		freq = freq * 2 / 4;
		break;

	case 11:
		freq = freq * 1 / 4;
		break;

	default:
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return freq;
}

unsigned int get_cur_phy_freq(struct pll_ctrl_t *pll_p)
{
	unsigned int con1;
	unsigned int ckdiv1;
	unsigned int cur_khz;

	FUNC_ENTER(FUNC_LV_LOCAL);

	con1 = cpufreq_read(pll_p->armpll_addr);
	ckdiv1 = cpufreq_read(pll_p->armpll_div_addr);
	ckdiv1 = _GET_BITS_VAL_(pll_p->armpll_div_h:pll_p->armpll_div_l, ckdiv1);
	cur_khz = _cpu_freq_calc(con1, ckdiv1);

	cpufreq_ver("@%s: cur_khz = %d, con1[0x%p] = 0x%x, ckdiv1_val = 0x%x\n",
		__func__, cur_khz, pll_p->armpll_addr, con1, ckdiv1);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return cur_khz;
}

void _cpu_clock_switch(struct pll_ctrl_t *pll_p, enum top_ckmuxsel sel)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (sel) {
	case TOP_CKMUXSEL_CLKSQ:
	case TOP_CKMUXSEL_ARMPLL:
		cpufreq_write_mask(CLK_MISC_CFG_0, 5 : 4, 0x0);
		break;
	case TOP_CKMUXSEL_MAINPLL:
	case TOP_CKMUXSEL_UNIVPLL:
		cpufreq_write_mask(CLK_MISC_CFG_0, 5 : 4, 0x3);
		break;
	default:
		break;
	};

	cpufreq_write_mask(pll_p->armpll_div_addr,
		pll_p->pll_muxsel_h:pll_p->pll_muxsel_l, sel);

	FUNC_EXIT(FUNC_LV_HELP);
}

enum top_ckmuxsel _get_cpu_clock_switch(struct pll_ctrl_t *pll_p)
{
	unsigned int val;
	unsigned int mask;

	FUNC_ENTER(FUNC_LV_HELP);

	val = cpufreq_read(pll_p->armpll_div_addr);
	mask = _BITMASK_(pll_p->pll_muxsel_h:pll_p->pll_muxsel_l);
	val &= mask;
	val = val >> pll_p->pll_muxsel_l;

	FUNC_EXIT(FUNC_LV_HELP);

	return val;
}

/* Always put action cpu at last */
struct hp_action_tbl cpu_dvfs_hp_action[] = {
#if 1
	{
		.action = CPU_DOWN_PREPARE | CPU_TASKS_FROZEN,
		.cluster = MT_CPU_DVFS_LL,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_LL].action_id = FREQ_LOW,
	},

	{
		.action = CPU_DOWN_PREPARE | CPU_TASKS_FROZEN,
		.cluster = MT_CPU_DVFS_L,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_L].action_id = FREQ_LOW,
	},

	{
		.action = CPU_DOWN_PREPARE | CPU_TASKS_FROZEN,
		.cluster = MT_CPU_DVFS_B,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_B].action_id = FREQ_LOW,
	},

	{
		.action = CPU_DOWN_PREPARE,
		.cluster = MT_CPU_DVFS_LL,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_LL].action_id = FREQ_LOW,
	},

	{
		.action = CPU_DOWN_PREPARE,
		.cluster = MT_CPU_DVFS_L,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_L].action_id = FREQ_LOW,
	},

	{
		.action = CPU_DOWN_PREPARE,
		.cluster = MT_CPU_DVFS_B,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_B].action_id = FREQ_LOW,
	},
#else
	{
		.action = CPU_DOWN_PREPARE,
		.cluster = MT_CPU_DVFS_B,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_B].action_id = FREQ_USR_REQ,
		.hp_action_cfg[MT_CPU_DVFS_B].freq_idx = FREQ_LOW,
		.hp_action_cfg[MT_CPU_DVFS_LL].action_id = FREQ_DEPEND_VOLT,
		.hp_action_cfg[MT_CPU_DVFS_L].action_id = FREQ_DEPEND_VOLT,
	},

	{
		.action = CPU_ONLINE,
		.cluster = MT_CPU_DVFS_LL,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_LL].action_id = FREQ_DEPEND_VOLT,
	},

	{
		.action = CPU_ONLINE,
		.cluster = MT_CPU_DVFS_L,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_L].action_id = FREQ_DEPEND_VOLT,
	},

	{
		.action = CPU_ONLINE,
		.cluster = MT_CPU_DVFS_B,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_B].action_id = FREQ_HIGH,
	},

	{
		.action = CPU_DOWN_FAILED,
		.cluster = MT_CPU_DVFS_LL,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_LL].action_id = FREQ_DEPEND_VOLT,
	},

	{
		.action = CPU_DOWN_FAILED,
		.cluster = MT_CPU_DVFS_L,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_L].action_id = FREQ_DEPEND_VOLT,
	},

	{
		.action = CPU_DOWN_FAILED,
		.cluster = MT_CPU_DVFS_B,
		.trigged_core = 1,
		.hp_action_cfg[MT_CPU_DVFS_B].action_id = FREQ_HIGH,
	},
#endif
};

int mt_cpufreq_regulator_map(struct platform_device *pdev)
{
	int ret = 0;

	regulator_proc1 = regulator_get_exclusive(&pdev->dev, "ext_buck_proc1");
	if (!regulator_proc1) {
		cpufreq_err("%s No ext_buck_proc1\n", __func__);
		return -EINVAL;
	}

	regulator_proc2 = regulator_get_exclusive(&pdev->dev, "ext_buck_proc2");
	if (!regulator_proc2) {
		cpufreq_err("%s No ext_buck_proc1\n", __func__);
		return -EINVAL;
	}

	regulator_sram1 = regulator_get_exclusive(&pdev->dev, "vsram_dvfs1");
	if (!regulator_sram1) {
		cpufreq_err("%s No vsram_dvfs1\n", __func__);
		return -EINVAL;
	}

	regulator_sram2 = regulator_get_exclusive(&pdev->dev, "vsram_dvfs2");
	if (!regulator_sram2) {
		cpufreq_err("%s No vsram_dvfs2\n", __func__);
		return -EINVAL;
	}

	ret = regulator_enable(regulator_proc1);
	if (ret < 0)
		pr_err("regulator_proc1 enable fail\n");

	ret = regulator_enable(regulator_proc2);
	if (ret < 0)
		pr_err("regulator_proc2 enable fail\n");

	ret = regulator_enable(regulator_sram1);
	if (ret < 0)
		pr_err("regulator_sram1 enable fail\n");

	ret = regulator_enable(regulator_sram2);
	if (ret < 0)
		pr_err("regulator_sram2 enable fail\n");

	return ret;
}

#ifdef CONFIG_OF
int mt_cpufreq_dts_map(void)
{
	struct device_node *node;

	/* topckgen */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_NODE);
	if (!node)
		cpufreq_err("error: cannot find node " TOPCKGEN_NODE);

	topckgen_base = (unsigned long)of_iomap(node, 0);
	if (!topckgen_base)
		cpufreq_err("error: cannot iomap " TOPCKGEN_NODE);

	/* infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node)
		cpufreq_err("error: cannot find node " MCUCFG_NODE);

	mcucfg_base = (unsigned long)of_iomap(node, 0);
	if (!mcucfg_base)
		cpufreq_err("error: cannot iomap " MCUCFG_NODE);

	/* apmixed */
	node = of_find_compatible_node(NULL, NULL, MCUMIXED_NODE);
	if (!node)
		cpufreq_err("error: cannot find node " MCUMIXED_NODE);

	mcumixed_base = (unsigned long)of_iomap(node, 0);
	if (!mcumixed_base)
		cpufreq_err("error: cannot iomap " MCUMIXED_NODE);

	return 0;
}
#else
int mt_cpufreq_dts_map(void)
{
	return 0;
}
#endif

#ifdef __KERNEL__
unsigned int _mt_cpufreq_get_cpu_level(void)
{
	unsigned int lv = 0;
	unsigned int func_code_0 = _GET_BITS_VAL_(27:24, get_devinfo_with_index(FUNC_CODE_EFUSE_INDEX));
	unsigned int func_code_1 = _GET_BITS_VAL_(3:0, get_devinfo_with_index(FUNC_CODE_EFUSE_INDEX));
	return CPU_LEVEL_0;
	cpufreq_ver("from efuse: function code 0 = 0x%x, function code 1 = 0x%x\n", func_code_0,
		     func_code_1);

	if (func_code_1 == 0)
		lv = CPU_LEVEL_0;
	else if (func_code_1 == 1)
		lv = CPU_LEVEL_1;
	else /* if (func_code_1 == 2) */
		lv = CPU_LEVEL_2;

	/* get CPU clock-frequency from DT */
#ifdef CONFIG_OF
	{
		/* struct device_node *node = of_find_node_by_type(NULL, "cpu"); */
		struct device_node *node = of_find_compatible_node(NULL, "cpu", "arm,cortex-a72");
		unsigned int cpu_speed = 0;

		if (!of_property_read_u32(node, "clock-frequency", &cpu_speed))
			cpu_speed = cpu_speed / 1000 / 1000;	/* MHz */
		else {
			cpufreq_err
			    ("@%s: missing clock-frequency property, use default CPU level\n",
			     __func__);
			return CPU_LEVEL_0;
		}

		cpufreq_ver("CPU clock-frequency from DT = %d MHz\n", cpu_speed);

		if (cpu_speed == 1989) /* M */
			return CPU_LEVEL_2;

	}
#endif

	return lv;
}
#else
unsigned int _mt_cpufreq_get_cpu_level(void)
{
	return CPU_LEVEL_0;
}
#endif
