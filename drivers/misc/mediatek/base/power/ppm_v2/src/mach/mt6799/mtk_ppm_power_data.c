/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"


void ppm_power_data_init(void)
{
	ppm_cobra_init();

	ppm_platform_init();

#ifdef PPM_SSPM_SUPPORT
	ppm_ipi_init(0, 0);
#endif

	ppm_info("power data init done!\n");
}


