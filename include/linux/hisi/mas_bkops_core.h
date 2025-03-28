/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * Description: bkops core framework
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

#ifndef MAS_BKOPS_CORE_H
#define MAS_BKOPS_CORE_H

#include <linux/workqueue.h>
#include <linux/blkdev.h>

enum bkops_operation {
	BKOPS_STOP = 0,
	BKOPS_START,
};

#define BKOPS_START_STATUS 0
#define BKOPS_QUERY_NEEDED BIT(1)
#define BKOPS_CHK_ACCU_WRITE BIT(2)
#define BKOPS_CHK_ACCU_DISCARD BIT(3)
#define BKOPS_CHK_TIME_INTERVAL BIT(4)
#define BKOPS_ASYNC_WORK_STARTED 5

enum bkops_dev_type {
	BKOPS_DEV_NONE = 0,
	BKOPS_DEV_MMC,
	BKOPS_DEV_UFS_1861,
	BKOPS_DEV_UFS_HYNIX,
	BKOPS_DEV_TYPE_MAX,
};

struct mas_bkops {
	enum bkops_dev_type dev_type;
	struct bkops_ops *bkops_ops;
	void *bkops_data;
	struct request_queue *q;
	struct delayed_work bkops_idle_work;
	unsigned long bkops_flag;
	unsigned long bkops_idle_delay_ms;
	struct blk_busyidle_event_node busyidle_event_node;

	u32 bkops_status; /* bkops status of last query */
	unsigned int en_bkops_retry;
	long bkops_check_interval; /* in seconds */
	long last_bkops_query_time; /* in seconds */

	unsigned long bkops_check_discard_len; /* in bytes */
	unsigned long last_discard_len; /* in bytes */

	unsigned long bkops_check_write_len; /* in bytes */
	unsigned long last_write_len; /* in bytes */
};

typedef int (bkops_start_stop_fn)(void *bkops_data, int start);
typedef int (bkops_status_query_fn)(void *bkops_data, u32 *status);
struct bkops_ops {
	bkops_start_stop_fn *bkops_start_stop;
	bkops_status_query_fn *bkops_status_query;
};

#define BKOPS_DEF_IDLE_DELAY	1000 /* in ms */
#define BKOPS_DEF_CHECK_INTERVAL	(60 * 60) /* in seconds */
#define BKOPS_DEF_DISCARD_LEN	(512 * 1024 * 1024) /* in bytes */
#define BKOPS_DEF_WRITE_LEN		(512 * 1024 * 1024) /* in bytes */

struct mas_bkops *mas_bkops_alloc(void);
void mas_bkops_set_status_str(
	struct mas_bkops *bkops, u32 bkops_stat_max,
	const char **bkops_stat_str);
int mas_bkops_enable(
	struct request_queue *q, struct mas_bkops *bkops, struct dentry *pdir);
int mas_bkops_add_debugfs(
	struct mas_bkops *bkops, const struct dentry *bkops_root);
void mas_bkops_remove_debugfs(struct mas_bkops *bkops);
#endif /* MAS_BKOPS_CORE_H */
