/*
 * Copyright (c) 2014 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <drm/drmP.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"

struct mtk_drm_debugfs_table {
	char name[8];
	unsigned int offset[2];
	unsigned int length[2];
	unsigned int reg_base;
};

/* ------------------------------------------------------------------------- */
/* External variable declarations */
/* ------------------------------------------------------------------------- */
void __iomem *gdrm_disp1_base[7];
void __iomem *gdrm_disp2_base[7];
struct mtk_drm_debugfs_table gdrm_disp1_reg_range[7] = {
	{ "OVL0 ", {0, 0xf40}, {0x260, 0x80} },
	{ "COLOR0 ", {0x400, 0xc00}, {0x400, 0x100} },
	{ "AAL0 ", {0, 0}, {0x100, 0} },
	{ "OD0 ", {0, 0}, {0x100, 0} },
	{ "RDMA0 ", {0, 0}, {0x100, 0} },
	{ "CONFIG ", {0, 0}, {0x120, 0} },
	{ "MUTEX ", {0, 0}, {0x100, 0} }
};

struct mtk_drm_debugfs_table gdrm_disp2_reg_range[7] = {
	{ "OVL1 ", {0, 0xf40}, {0x260, 0x80} },
	{ "COLOR1 ", {0x400, 0xc00}, {0x100, 0x100} },
	{ "AAL1 ", {0, 0}, {0x100, 0} },
	{ "OD1 ", {0, 0}, {0x100, 0} },
	{ "RDMA1 ", {0, 0}, {0x100, 0} },
	{ "CONFIG ", {0, 0}, {0x120, 0} },
	{ "MUTEX ", {0, 0}, {0x100, 0} }
};
static bool dbgfs_alpha;

void mtk_read_reg(unsigned long addr)
{
	unsigned long reg_va = 0;

	reg_va = (unsigned long)ioremap_nocache(addr, sizeof(unsigned long));
	pr_info("r:0x%8lx = 0x%08x\n", addr, readl((void *)reg_va));
	iounmap((void *)reg_va);
}

void mtk_write_reg(unsigned long addr, unsigned long val)
{
	unsigned long reg_va = 0;

	reg_va = (unsigned long)ioremap_nocache(addr, sizeof(unsigned long));
	writel(val, (void *)reg_va);
	iounmap((void *)reg_va);
}

/* ------------------------------------------------------------------------- */
/* Debug Options */
/* ------------------------------------------------------------------------- */
static char STR_HELP[] =
	"\n"
	"USAGE\n"
	"        echo [ACTION]... > mtkdrm\n"
	"\n"
	"ACTION\n"
	"\n"
	"        dump:\n"
	"             dump all hw registers\n"
	"\n"
	"        regw:addr=val\n"
	"             write hw register\n"
	"\n"
	"        regr:addr\n"
	"             read hw register\n";

/* ------------------------------------------------------------------------- */
/* Command Processor */
/* ------------------------------------------------------------------------- */
static void process_dbg_opt(const char *opt)
{
	if (strncmp(opt, "regw:", 5) == 0) {
		char *p = (char *)opt + 5;
		char *np;
		unsigned long addr, val;
		int i;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr))
			goto error;

		if (!p)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val))
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr > gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base + 0x1000) {
				writel(val, gdrm_disp1_base[i] + addr -
					gdrm_disp1_reg_range[i].reg_base);
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr > gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base + 0x1000) {
				writel(val, gdrm_disp2_base[i] + addr -
					gdrm_disp2_reg_range[i].reg_base);
				break;
			}
		}

	} else if (strncmp(opt, "regr:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned long addr;
		int i;

		if (kstrtoul(p, 16, &addr))
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
			    0x1000) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp1_reg_range[i].name, addr,
					 readl(gdrm_disp1_base[i] + addr -
				gdrm_disp1_reg_range[i].reg_base));
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base + 0x1000) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp2_reg_range[i].name, addr,
					 readl(gdrm_disp2_base[i] + addr -
				gdrm_disp2_reg_range[i].reg_base));
				break;
			}
		}

	} else if (strncmp(opt, "autoregr:", 9) == 0) {
		DRM_INFO("Set the register addr for Auto-test\n");
	} else if (strncmp(opt, "dump:", 5) == 0) {
		int i, j;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (!gdrm_disp1_base[i])
				continue;
			for (j = gdrm_disp1_reg_range[i].offset[0];
			     j < gdrm_disp1_reg_range[i].offset[0] +
			     gdrm_disp1_reg_range[i].length[0]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_disp1_reg_range[i].name,
					gdrm_disp1_reg_range[i].reg_base + j,
					readl(gdrm_disp1_base[i] + j),
					readl(gdrm_disp1_base[i] + j + 0x4),
					readl(gdrm_disp1_base[i] + j + 0x8),
					readl(gdrm_disp1_base[i] + j + 0xc));

			for (j = gdrm_disp1_reg_range[i].offset[1];
			     j < gdrm_disp1_reg_range[i].offset[1] +
			     gdrm_disp1_reg_range[i].length[1]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_disp1_reg_range[i].name,
					gdrm_disp1_reg_range[i].reg_base + j,
					readl(gdrm_disp1_base[i] + j),
					readl(gdrm_disp1_base[i] + j + 0x4),
					readl(gdrm_disp1_base[i] + j + 0x8),
					readl(gdrm_disp1_base[i] + j + 0xc));
		}
		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (!gdrm_disp2_base[i])
				continue;
			for (j = gdrm_disp2_reg_range[i].offset[0];
			     j < gdrm_disp2_reg_range[i].offset[0] +
			     gdrm_disp2_reg_range[i].length[0]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_disp2_reg_range[i].name,
					gdrm_disp2_reg_range[i].reg_base + j,
					readl(gdrm_disp2_base[i] + j),
					readl(gdrm_disp2_base[i] + j + 0x4),
					readl(gdrm_disp2_base[i] + j + 0x8),
					readl(gdrm_disp2_base[i] + j + 0xc));

			for (j = gdrm_disp2_reg_range[i].offset[1];
			     j < gdrm_disp2_reg_range[i].offset[1] +
			     gdrm_disp2_reg_range[i].length[1]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_disp2_reg_range[i].name,
					gdrm_disp2_reg_range[i].reg_base + j,
					readl(gdrm_disp2_base[i] + j),
					readl(gdrm_disp2_base[i] + j + 0x4),
					readl(gdrm_disp2_base[i] + j + 0x8),
					readl(gdrm_disp2_base[i] + j + 0xc));
		}

	} else if (strncmp(opt, "hdmi:", 5) == 0) {
	} else if (strncmp(opt, "alpha", 5) == 0) {
		if (dbgfs_alpha) {
			DRM_INFO("set src alpha to src alpha\n");
			dbgfs_alpha = false;
		} else {
			DRM_INFO("set src alpha to ONE\n");
			dbgfs_alpha = true;
		}
	} else if (strncmp(opt, "r:", 2) == 0) {
		char *p = (char *)opt + 2;
		unsigned long addr;

		if (kstrtoul(p, 16, &addr))
			goto error;

		mtk_read_reg(addr);
	} else if (strncmp(opt, "w:", 2) == 0) {
		char *p = (char *)opt + 2;
		char *np;
		unsigned long addr, val;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr))
			goto error;

		if (!p)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val))
			goto error;

		mtk_write_reg(addr, val);
	} else {
		goto error;
	}

	return;
 error:
	DRM_ERROR("Parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* ------------------------------------------------------------------------- */
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char dis_cmd_buf[512];
static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	if (strncmp(dis_cmd_buf, "regr:", 5) == 0) {
		char read_buf[512] = {0};
		char *p = (char *)dis_cmd_buf + 5;
		unsigned long addr;
		int i;

		if (kstrtoul(p, 16, &addr))
			return 0;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base + 0x1000) {
				sprintf(read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp1_reg_range[i].name, addr,
					readl(gdrm_disp1_base[i] + addr -
					gdrm_disp1_reg_range[i].reg_base));
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base + 0x1000) {
				sprintf(read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp2_reg_range[i].name, addr,
					readl(gdrm_disp2_base[i] + addr -
					gdrm_disp2_reg_range[i].reg_base));
				break;
			}
		}

		return simple_read_from_buffer(ubuf, count, ppos, read_buf,
						strlen(read_buf));
	} else if (strncmp(dis_cmd_buf, "autoregr:", 9) == 0) {
		char read_buf[512] = {0};
		char read_buf2[512] = {0};
		char *p = (char *)dis_cmd_buf + 9;
		unsigned long addr;
		unsigned long addr2;
		int i;

		if (kstrtoul(p, 16, &addr))
			return 0;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base + 0x1000) {
				sprintf(read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp1_reg_range[i].name, addr,
					readl(gdrm_disp1_base[i] + addr -
					gdrm_disp1_reg_range[i].reg_base));
				break;
			}
		}
		addr2 = addr + 0x1000;
		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr2 >= gdrm_disp2_reg_range[i].reg_base &&
			    addr2 < gdrm_disp2_reg_range[i].reg_base + 0x1000) {
				sprintf(read_buf2,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp2_reg_range[i].name, addr2,
					readl(gdrm_disp2_base[i] + addr2 -
					gdrm_disp2_reg_range[i].reg_base));
				break;
			}
		}
		strcat(read_buf, read_buf2);
		return simple_read_from_buffer(ubuf, count, ppos, read_buf,
						strlen(read_buf));
	} else {
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
						strlen(STR_HELP));
	}
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(dis_cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&dis_cmd_buf, ubuf, count))
		return -EFAULT;

	dis_cmd_buf[count] = 0;

	process_dbg_cmd(dis_cmd_buf);

	return ret;
}

struct dentry *mtkdrm_dbgfs;
static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

bool force_alpha(void)
{
	return dbgfs_alpha;
}

void mtk_drm_debugfs_init(struct drm_device *dev,
			  struct mtk_drm_private *priv)
{
	void __iomem *mutex_regs;
	unsigned int mutex_phys;
	struct device_node *np;
	struct resource res;
	int i;
	int comp_id;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	mtkdrm_dbgfs = debugfs_create_file("mtkdrm", 0644, NULL, (void *)0,
					   &debug_fops);

	for (i = 0; (comp_id = priv->data->main_path[i]) !=
		     DDP_COMPONENT_DPI0 && comp_id !=
		     DDP_COMPONENT_PWM0; i++) {
		np = priv->comp_node[comp_id];
		gdrm_disp1_base[i] = priv->ddp_comp[comp_id]->regs;
		of_address_to_resource(np, 0, &res);
		gdrm_disp1_reg_range[i].reg_base = res.start;
	}

	gdrm_disp1_base[i] = priv->config_regs;
	gdrm_disp1_reg_range[i++].reg_base = 0x14000000;
	mutex_regs = of_iomap(priv->mutex_node, 0);
	of_address_to_resource(priv->mutex_node, 0, &res);
	mutex_phys = res.start;
	gdrm_disp1_base[i] = mutex_regs;
	gdrm_disp1_reg_range[i++].reg_base = mutex_phys;

	for (i = 0; (comp_id = priv->data->ext_path[i]) !=
		     DDP_COMPONENT_DPI1 && comp_id !=
		     DDP_COMPONENT_PWM1; i++) {
		np = priv->comp_node[comp_id];
		gdrm_disp2_base[i] = of_iomap(np, 0);
		of_address_to_resource(np, 0, &res);
		gdrm_disp2_reg_range[i].reg_base = res.start;
	}
	gdrm_disp2_base[i] = priv->config_regs;
	gdrm_disp2_reg_range[i++].reg_base = 0x14000000;
	gdrm_disp2_base[i] = mutex_regs;
	gdrm_disp2_reg_range[i].reg_base = mutex_phys;

	DRM_DEBUG_DRIVER("%s..done\n", __func__);
}

void mtk_drm_debugfs_deinit(void)
{
	debugfs_remove(mtkdrm_dbgfs);
}
