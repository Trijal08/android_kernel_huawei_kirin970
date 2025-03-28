/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: provide function to find out mmap_sem's owner.
 * Author: Gong Chen <gongchen4@huawei.com>
 * Create: 2020-11-11
 */
#include <linux/hisi/huawei_check_mmap_sem.h>

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
};

static int is_waiting_mmap_sem(struct task_struct *task)
{
	struct rwsem_waiter *waiter = NULL;
	struct rw_semaphore *sem = &task->mm->mmap_sem;

	raw_spin_lock_irq(&sem->wait_lock);
	list_for_each_entry(waiter, &sem->wait_list, list) {
		if (task == waiter->task) {
			raw_spin_unlock_irq(&sem->wait_lock);
			return 1;
		}
	}
	raw_spin_unlock_irq(&sem->wait_lock);

	return 0;
}

void check_mmap_sem(pid_t pid)
{
	struct task_struct *task = NULL;
	struct task_struct *t = NULL;

	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (!task) {
		pr_err("%s : can't find task!\n", __func__);
		goto out;
	}

	task_lock(task);

	pr_err("hungtask:name=%s,PID=%d,tgid=%d,tgname=%s\n",
		task->comm, task->pid, task->tgid, task->group_leader->comm);
	sched_show_task(task);

	if (!task->mm) {
		pr_err("%s has no mm!\n", task->comm);
		goto unlock;
	}

	if (!is_waiting_mmap_sem(task)) {
		pr_err("%s is not waiting for mmap_sem!\n", task->comm);
		goto unlock;
	}

	pr_err("%s is waiting for mmap_sem!\n", task->comm);

	for_each_thread(task, t) {
		if ((t->state == TASK_RUNNING ||
			t->state == TASK_UNINTERRUPTIBLE) &&
			!is_waiting_mmap_sem(t)) {
			pr_err("hungtask:name=%s,PID=%d, may hold mmap_sem\n",
				t->comm, t->pid);
			sched_show_task(t);
		}
	}

unlock:
	task_unlock(task);

out:
	rcu_read_unlock();
}
