/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2016-2019. All rights reserved.
 * Description: mas block unistore interface for hmfs
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[BLK-IO]" fmt

#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/hisi/powerkey_event.h>
#include <trace/events/block.h>
#include <linux/types.h>
#include <trace/iotrace.h>
#include "blk.h"
#include "dsm_block.h"

static int mas_blk_get_device_unistore_enabled(
	struct block_device *bdev, struct blkdev_cmd *cmd)
{
	int err;
	uint8_t enabled = 0;
	struct request_queue *q = bdev_get_queue(bdev);

	if (blk_queue_query_unistore_enable(q))
		enabled = 1;

	if (!cmd->cust_argp)
		return -EFAULT;

	err = copy_to_user(cmd->cust_argp, &enabled, sizeof(uint8_t));
	if (unlikely(err))
		pr_err("%s, copy_to_user failed, err = %d\n", __func__, err);

	return err;
}

int mas_blk_cust_ioctl(struct block_device *bdev, struct blkdev_cmd __user *arg)
{
	struct blkdev_cmd cmd;

	if (!arg)
		return -EFAULT;

	if (copy_from_user(&cmd, arg, sizeof(struct blkdev_cmd))) {
		pr_err("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	pr_err("%s! cmd = 0x%lx\n", __func__, cmd.cmd);
	switch (cmd.cmd) {
	case CUST_BLKDEV_GET_UNISTORE_ENABLE:
		return mas_blk_get_device_unistore_enabled(bdev, &cmd);
	default:
		break;
	}
	return 0;
}
