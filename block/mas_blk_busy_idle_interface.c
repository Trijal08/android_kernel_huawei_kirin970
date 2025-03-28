/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * Description: mas block busy idle interface
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

#include "mas_blk_busy_idle_interface.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
void __cfi_mas_blk_busyidle_handler_latency_check_timer_expire(
	struct timer_list *timer)
{
	mas_blk_busyidle_handler_latency_check_timer_expire(timer);
}
#else
void __cfi_mas_blk_busyidle_handler_latency_check_timer_expire(
	unsigned long data)
{
	mas_blk_busyidle_handler_latency_check_timer_expire(data);
}
#endif

int __cfi_mas_blk_busyidle_notify_handler(
	struct notifier_block *nb, unsigned long val, void *v)
{
	return mas_blk_busyidle_notify_handler(nb, val, v);
}

void __cfi_mas_blk_idle_notify_work(struct work_struct *work)
{
	mas_blk_idle_notify_work(work);
}

void __cfi_mas_blk_busyidle_end_rq(struct request *rq, blk_status_t error)
{
	mas_blk_busyidle_end_rq(rq, error);
}

