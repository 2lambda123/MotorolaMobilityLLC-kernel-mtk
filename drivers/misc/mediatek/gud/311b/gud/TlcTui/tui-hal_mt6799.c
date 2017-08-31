/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <t-base-tui.h>

#include <mach/mtk_clkmgr.h>

#include "tui_ioctl.h"
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"
#include "tui-hal_mt.h"

#define TUI_MEMPOOL_SIZE 0

/* Extrac memory size required for TUI driver */
#define TUI_EXTRA_MEM_SIZE (0x200000)

struct tui_mempool {
	void *va;
	unsigned long pa;
	size_t size;
};

static int g_tbuff_alloc;

/**
 * hal_tui_init() - integrator specific initialization for kernel module
 *
 * This function is called when the kernel module is initialized, either at
 * boot time, if the module is built statically in the kernel, or when the
 * kernel is dynamically loaded if the module is built as a dynamic kernel
 * module. This function may be used by the integrator, for instance, to get a
 * memory pool that will be used to allocate the secure framebuffer and work
 * buffer for TUI sessions.
 *
 * Return: must return 0 on success, or non-zero on error. If the function
 * returns an error, the module initialization will fail.
 */
uint32_t hal_tui_init(void)
{
	/* Allocate memory pool for the framebuffer
	 */
	return TUI_DCI_OK;
}

/**
 * hal_tui_exit() - integrator specific exit code for kernel module
 *
 * This function is called when the kernel module exit. It is called when the
 * kernel module is unloaded, for a dynamic kernel module, and never called for
 * a module built into the kernel. It can be used to free any resources
 * allocated by hal_tui_init().
 */
void hal_tui_exit(void)
{
	/* delete memory pool if any */
}

/**
 * hal_tui_alloc() - allocator for secure framebuffer and working buffer
 * @allocbuffer:    input parameter that the allocator fills with the physical
 *                  addresses of the allocated buffers
 * @allocsize:      size of the buffer to allocate.  All the buffer are of the
 *                  same size
 * @number:         Number to allocate.
 *
 * This function is called when the module receives a CMD_TUI_SW_OPEN_SESSION
 * message from the secure driver.  The function must allocate 'number'
 * buffer(s) of physically contiguous memory, where the length of each buffer
 * is at least 'allocsize' bytes.  The physical address of each buffer must be
 * stored in the array of structure 'allocbuffer' which is provided as
 * arguments.
 *
 * Physical address of the first buffer must be put in allocate[0].pa , the
 * second one on allocbuffer[1].pa, and so on.  The function must return 0 on
 * success, non-zero on error.  For integrations where the framebuffer is not
 * allocated by the Normal World, this function should do nothing and return
 * success (zero).
 * If the working buffer allocation is different from framebuffers, ensure that
 * the physical address of the working buffer is at index 0 of the allocbuffer
 * table (allocbuffer[0].pa).
 */
uint32_t hal_tui_alloc(
	struct tui_alloc_buffer_t allocbuffer[MAX_DCI_BUFFER_NUMBER],
	size_t allocsize, uint32_t number)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;
	phys_addr_t pa = 0;
	unsigned long size = allocsize;

	if (!allocbuffer) {
		pr_debug("%s(%d): allocbuffer is null\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	pr_debug("%s(%d): Requested size=0x%zx x %u chunks\n",
		 __func__, __LINE__, allocsize, number);

	if ((size_t)allocsize == 0) {
		pr_debug("%s(%d): Nothing to allocate\n", __func__, __LINE__);
		return TUI_DCI_OK;
	}

	if (number != 2) {
		pr_err("%s(%d): Unexpected number of buffers requested\n",
			 __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	ret = tui_region_offline(&pa, &size);

	if (ret == 0) {
		g_tbuff_alloc = 1;
		allocbuffer[0].pa = (uint64_t) pa;
		allocbuffer[1].pa = (uint64_t) (pa + allocsize);
		pr_debug("request_size=%lu, alloc_size=%lu, extra=%d\n",
			allocsize, size, TUI_EXTRA_MEM_SIZE);
		pr_debug("%s(%d): buf_s %llx, buf_e %llx\n", __func__, __LINE__,
			allocbuffer[0].pa, allocbuffer[1].pa);
	} else {
		pr_err("%s(%d): tui_region_offline failed!\n",
			 __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return TUI_DCI_OK;
}

/**
 * hal_tui_free() - free memory allocated by hal_tui_alloc()
 *
 * This function is called at the end of the TUI session, when the TUI module
 * receives the CMD_TUI_SW_CLOSE_SESSION message. The function should free the
 * buffers allocated by hal_tui_alloc(...).
 */
void hal_tui_free(void)
{
	pr_info("[TUI-HAL] hal_tui_free()\n");
	if (g_tbuff_alloc) {
		tui_region_online();
		g_tbuff_alloc = 0;
	}
}

/**
 * hal_tui_deactivate() - deactivate Normal World display and input
 *
 * This function should stop the Normal World display and, if necessary, Normal
 * World input. It is called when a TUI session is opening, before the Secure
 * World takes control of display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
uint32_t hal_tui_deactivate(void)
{
	int ret = TUI_DCI_OK, tmp;
	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	/*
	 * Stop NWd display here.  After this function returns, SWd will take
	 * control of the display and input.  Therefore the NWd should no longer
	 * access it
	 * This can be done by calling the fb_blank(FB_BLANK_POWERDOWN) function
	 * on the appropriate framebuffer device
	 */

	tpd_enter_tui();
#if 0
	enable_clock(MT_CG_PERI_I2C0, "i2c");
	enable_clock(MT_CG_PERI_I2C1, "i2c");
	enable_clock(MT_CG_PERI_I2C2, "i2c");
	enable_clock(MT_CG_PERI_I2C3, "i2c");
	enable_clock(MT_CG_PERI_APDMA, "i2c");
#endif
	i2c_tui_enable_clock();

	/*gt1x_power_reset();*/

	tmp = display_enter_tui();
	if (tmp) {
		pr_err("[TUI-HAL] %s() failed because display\n", __func__);
		ret = TUI_DCI_ERR_OUT_OF_DISPLAY;
	}


	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|
			   TRUSTEDUI_MODE_INPUT_SECURED);

	pr_info("[TUI-HAL] %s()\n", __func__);

	return ret;
}

/**
 * hal_tui_activate() - restore Normal World display and input after a TUI
 * session
 *
 * This function should enable Normal World display and, if necessary, Normal
 * World input. It is called after a TUI session, after the Secure World has
 * released the display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
uint32_t hal_tui_activate(void)
{
	pr_info("[TUI-HAL] hal_tui_activate()\n");
	/* Protect NWd */
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|
			     TRUSTEDUI_MODE_INPUT_SECURED);
	/*
	 * Restart NWd display here.  TUI session has ended, and therefore the
	 * SWd will no longer use display and input.
	 * This can be done by calling the fb_blank(FB_BLANK_UNBLANK) function
	 * on the appropriate framebuffer device
	 */
	/* Clear linux TUI flag */

	tpd_exit_tui();
#if 0
	disable_clock(MT_CG_PERI_I2C0, "i2c");
	disable_clock(MT_CG_PERI_I2C1, "i2c");
	disable_clock(MT_CG_PERI_I2C2, "i2c");
	disable_clock(MT_CG_PERI_I2C3, "i2c");
	disable_clock(MT_CG_PERI_APDMA, "i2c");
#endif
	i2c_tui_disable_clock();
	display_exit_tui();

	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_process_cmd(struct tui_hal_cmd_t *cmd,
			     struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	pr_info("hal_tui_post_start\n");
}

int __weak tui_region_offline(phys_addr_t *pa, unsigned long *size)
{
	return -1;
}

int __weak tui_region_online(void)
{
	return 0;
}

int __weak tpd_reregister_from_tui(void)
{
	return 0;
}

int __weak tpd_enter_tui(void)
{
	return 0;
}

int __weak tpd_exit_tui(void)
{
	return 0;
}

int __weak display_enter_tui(void)
{
	return 0;
}

int __weak display_exit_tui(void)
{
	return 0;
}

int __weak i2c_tui_enable_clock(void)
{
	return 0;
}

int __weak i2c_tui_disable_clock(void)
{
	return 0;
}
