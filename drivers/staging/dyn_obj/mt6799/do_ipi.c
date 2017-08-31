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

#include <linux/delay.h>
#include "do_ipi.h"
#include "do_impl.h"

/********************* IPI functions *******************/
/*
 * do_ipi_send:
 * make DO message and try to send IPI until timeout or error
 * @param
 * type: DO msg type
 * buf: DO msg payload data buffer
 * bufsize: size of payload buffer
 * @ret: 1 = success
 *	   0 = timeout
 *	  -1 = IPI error
 */
int do_ipi_send(u32 type, void *buf, unsigned int bufsize, scp_core_id id)
{
	ipi_status status;
	struct do_msg *header;
	int retry_count = 0;
	char ipibuf[IPI_BUFF_SIZE];

	if (bufsize > IPI_BUFF_SIZE - 1) {
		pr_err("ERR: payload size exceeds ipi buffer size\n");
		return -1;
	}

	header = (struct do_msg *)&ipibuf[0];
	header->scp_num = id;
	header->type = (u32)type;
	header->len = (u32)bufsize;
	if (bufsize != 0)
		memcpy((void *)&header->payload, buf, bufsize);

	while (1) {
		status = scp_ipi_send(IPI_DO_AP_MSG, ipibuf, bufsize + sizeof(struct do_msg) - 1, 1, id);
		if (status == BUSY) {
			msleep(IPI_WAIT_INTERVAL_MS);
			retry_count++;
			if (retry_count > IPI_MAX_RETRY_COUNT) {
				pr_err("ERR: ipi retry timeout\n");
				return 0;
			}
			/* else retry */
		} else if (status == ERROR) {
			pr_err("ERR: ipi error\n");
			return -1;
		}
		/* success */
		break;
	}

	return 1;
}

int do_ipi_send_dram_addr(u32 addr, scp_core_id scp)
{
	pr_debug("send dram addr to scp %d: 0x%x\n", scp, addr);
	return do_ipi_send(DO_MSG_DRAM_ADDR, &addr, sizeof(u32), scp);
}

int do_ipi_send_do_name(char *name, scp_core_id scp)
{
	return do_ipi_send(DO_MSG_LOAD_MODULE, name, strlen(name) + 1 /* include '\0' */, scp);
}

void do_ipi_handler(int id, void *data, unsigned int len)
{
	struct do_msg *header;

	header = (struct do_msg *)data;

	switch (header->type) {
	case DO_MSG_GET_INFO:
		do_send_dram_start_addr();
		break;
	case DO_MSG_UPDATE_CURRENT_DO:
		scp_update_current_do(&header->payload, header->len, header->scp_num);
		break;
	case DO_MSG_INFO:
		mt_do_init_infos((struct do_info *)&header->payload, header->len, header->scp_num);
		break;
	default:
		pr_debug("ipi_handler: unknown msg\n");
		break;
	}
}

