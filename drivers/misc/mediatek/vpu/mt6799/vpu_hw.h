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

#ifndef _MT6798_VPU_HW_H_
#define _MT6798_VPU_HW_H_

#include "vpu_reg.h"
#include "vpu_drv.h"

#define VPU_MAX_NUM_CODE_SEGMENTS       (20)
#define VPU_MAX_NUM_ALGOS               (20)

/* MVA */
#define VPU_MVA_RESET_VECTOR            (0x50000000)
#define VPU_MVA_MAIN_PROGRAM            (0x60000000)

/* Size */
#define VPU_SIZE_RESET_VECTOR           (0x00080000)
#define VPU_SIZE_MAIN_PROGRAM           (0x00A00000)
#define VPU_SIZE_RESERVED_INSTRUCT      (0x00A00000)
#define VPU_SIZE_ALGO_AREA              (0x00A00000)
#define VPU_SIZE_IMAGE_HEADERS          (0x00080000)
#define VPU_NUMS_IMAGE_HEADER           (3)

/* Offset */
#define VPU_OFFSET_RESET_VECTOR         (0x00000000)
#define VPU_OFFSET_MAIN_PROGRAM         (0x00080000)
#define VPU_OFFSET_ALGO_AREA            (0x01480000)
#define VPU_OFFSET_IMAGE_HEADERS        (0x01E80000)

/* Sum of all parts */
#define VPU_SIZE_BINARY_CODE            (0x01F00000)


struct vpu_code_segment {
	uint32_t offset;        /* offset from this partition */
	uint32_t dst_addr;      /* the DDR position is IPU can realize. */
	uint32_t length;        /* total size for this segment */
	uint32_t file_size;     /* file size to copy */
};

struct vpu_algo_info {
	uint32_t offset;
	uint32_t length;
	vpu_name_t name;
};


/*
 * The VPU program is stored in EMMC Partitions, and the little kernel will
 * load it to DDR. There are three partitions for different purpose, and little
 * kernel will merge them to contiguous physical memory. The buffer layout in
 * DDR is as follows:
 *
 * Using the layout, VPU driver could map these binary data
 * to the specific mva for VPU booting.
 *
 *  [offset]                              [mapping mva]
 *  0x00000000  +-----------------------+  0x50000000
 *              |  Reset vector of VPU  |
 *              |  code        [512KB]  |
 *  0x00080000  +-----------------------+  0x60000000
 *              |  Main Program         |
 *              |              [10MB]   |
 *  0x00A80000  +-----------------------+  0x60A00000
 *              |  Reserved for algo    |
 *              |  instruction [10MB]   |
 *  0x01480000  +-----------------------+  0x61400000
 *              |  Algo area            |
 *              |              [10MB]   |
 *  0x01E80000  +-----------------------+  no mva
 *              |  Merged image header  |
 *              |              [512KB]  |
 *              +-----------------------+
 *
 * The last part of buffer, named "merged image header", will put a array of
 * struct vpu_image_header, whose size is 3. All VPU driver needs to know is algo's
 * offset for algo loading.
 *
 */
struct vpu_image_header {
	uint32_t version;
	uint32_t build_date;
	uint32_t header_desc[8];
	uint32_t header_size;
	uint32_t image_size;
	uint32_t code_segment_count;
	struct vpu_code_segment code_segments[VPU_MAX_NUM_CODE_SEGMENTS];
	uint32_t algo_info_count;
	struct vpu_algo_info algo_infos[VPU_MAX_NUM_ALGOS];
	uint32_t reserved[32];
};


/*
 * VPU driver uses the spare register to exchange data with VPU program(VPU code).
 * The below tables show the usage of spare register:
 *
 * Command: GET_ALGO_INFO
 *  +-----------------+----------------------------------------------+-----------+
 *  | Field           | Description                                  | Filled By |
 *  +-----------------+----------------------------------------------+-----------+
 *  |FLD_XTENSA_INFO1 | 0x82(GET_ALGO_INFO)                          | Driver    |
 *  |FLD_XTENSA_INFO5 | num of ports                                 | VPU Code  |
 *  |FLD_XTENSA_INFO6 | pointer to the array of struct port          | Driver    |
 *  |FLD_XTENSA_INFO7 | [info] pointer to property buffer            | Driver    |
 *  |FLD_XTENSA_INFO8 | [info] size of property buffer(1024)         | Driver    |
 *  |FLD_XTENSA_INFO9 | [info] num of property description           | VPU Code  |
 *  |FLD_XTENSA_INFO10| [info] pointer to the array of struct desc   | Driver    |
 *  |FLD_XTENSA_INFO11| [sett] num of property description           | VPU Code  |
 *  |FLD_XTENSA_INFO12| [sett] pointer to the array of struct desc   | Driver    |
 *  +-----------------+----------------------------------------------+-----------+
 *
 * Command: DO_LOADER
 *  +-----------------+---------------------------------------+-----------+
 *  | Field           | Description                           | Filled By |
 *  +-----------------+---------------------------------------+-----------+
 *  |FLD_XTENSA_INFO1 | 0x01(DO_LOADER)                       | Driver    |
 *  |FLD_XTENSA_INFO12| pointer to the algo's start-address   | Driver    |
 *  |FLD_XTENSA_INFO13| size of the algo                      | Driver    |
 *  |FLD_XTENSA_INFO14| function entry point (optional)       | Driver    |
 *  |FLD_XTENSA_INFO15| VPU frequency (KHz)                   | Driver    |
 *  |FLD_XTENSA_INFO16| VPU IF frequency (KHz)                | Driver    |
 *  +-----------------+---------------------------------------+-----------+
 *
 * Command: DO_D2D
 *  +-----------------+---------------------------------------+-----------+
 *  | Field           | Description                           | Filled By |
 *  +-----------------+---------------------------------------+-----------+
 *  |FLD_XTENSA_INFO1 | 0x22(DO_D2D)                          | Driver    |
 *  |FLD_XTENSA_INFO12| num of buffers                        | Driver    |
 *  |FLD_XTENSA_INFO13| pointer to the array of struct buffer | Driver    |
 *  |FLD_XTENSA_INFO14| pointer to setting buffer             | Driver    |
 *  |FLD_XTENSA_INFO15| size of setting buffer                | Driver    |
 *  +-----------------+---------------------------------------+-----------+
 *
 * Command: GET_SWVER
 *  +-----------------+---------------------------------------+-----------+
 *  | Field           | Description                           | Filled By |
 *  +-----------------+---------------------------------------+-----------+
 *  |FLD_XTENSA_INFO1 | 0x40(GET_SWVER)                       | Driver    |
 *  |FLD_XTENSA_INFO20| Software version                      | VPU Code  |
 *  +-----------------+---------------------------------------+-----------+
 *
 *  Command: SET_DEBUG
 *  +-----------------+---------------------------------------+-----------+
 *  | Field           | Description                           | Filled By |
 *  +-----------------+---------------------------------------+-----------+
 *  |FLD_XTENSA_INFO1 | 0x40(SET_DEG)                         | Driver    |
 *  |FLD_XTENSA_INFO21| mva of log buffer                     | Driver    |
 *  |FLD_XTENSA_INFO22| length of log buffer                  | Driver    |
 *  +-----------------+---------------------------------------+-----------+
 */


/**
 * vpu_hw_boot_sequence - do booting sequence
 */
int vpu_hw_boot_sequence(void);

/**
 * vpu_set_debug - set log buffer and size to VPU
 */
int vpu_hw_set_debug(void);

/**
 * vpu_hw_enable_jtag - start dsp debug via jtag
 */
int vpu_hw_enable_jtag(bool enabled);

/**
 * vpu_hw_enque_request - do DRAM-to-DRAM processing, and it will block until done.
 * @req:        the pointer to request
 */
int vpu_hw_enque_request(struct vpu_request *req);

/**
 * Working buffer's offset
 *
 *  [offset]
 *  0x00000000  +-----------------------+
 *              |  Command Buffer       |
 *              |              [8KB]    |
 *  0x00002000  +-----------------------+
 *              |  Log Buffer           |
 *              |              [8KB]    |
 *              +-----------------------+
 *
 * the first 16 bytes of log buffer:
 *   @tail_addr: the mva of log end, which always points to '\0'
 *
 *   +-----------+----------------------+
 *   |   0 ~ 3   |      4 ~ 15          |
 *   +-----------+----------------------+
 *   |{tail_addr}|    {reserved}        |
 *   +-----------+----------------------+
 */
#define VPU_OFFSET_COMMAND           (0x00000000)
#define VPU_OFFSET_LOG               (0x00002000)
#define VPU_SIZE_LOG_BUF             (0x00002000)
#define VPU_SIZE_LOG_HEADER          (0x00000010)
#define VPU_SIZE_WORK_BUF            (0x00004000)

#endif
