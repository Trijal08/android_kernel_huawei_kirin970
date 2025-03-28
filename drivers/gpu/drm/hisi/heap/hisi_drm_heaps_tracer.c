/*
 * hisi_drm_heaps_debugger.c
 *
 * Offer debug feature for hisi drm, if CONFIG_HISI_DEBUGFS is not open, the
 * feature also close.
 *
 * Copyright (c) 2019 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include "hisi_drm_heaps_tracer_inner.h"

#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/hisi/hisi_drm_heaps_tracer.h>

#include "hisi_drm_gem.h"
#include "hisi_gem_dmabuf.h"
#include "hisi_drm_heaps_defs.h"

struct hisi_gem_tracer_root {
	struct rb_root gems;

	spinlock_t rb_lock;
};

static struct hisi_gem_tracer_root *hisi_gem_tracer;

/**
 * dump nessary trace info into gem
 */
static void hisi_drm_heaps_dump_info(struct hisi_drm_heap_context *ctx)
{
	struct task_struct *task = NULL;
	struct hisi_drm_gem *hisi_gem = ctx->priv;

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);

	if (current->group_leader->flags & PF_KTHREAD) {
		task = NULL;
		hisi_gem->pid = 0;
	} else {
		task = current->group_leader;
		hisi_gem->pid = task_pid_nr(task);
	}

	task_unlock(current->group_leader);
	put_task_struct(current->group_leader);

	if (!task)
		strncpy(hisi_gem->task_comm, "invalid task", TASK_COMM_LEN);
	else
		get_task_comm(hisi_gem->task_comm, task);
}

/**
 * when gem is destroy, cancel trace
 */
void hisi_drm_heaps_remove_trace(struct hisi_drm_heap_context *ctx)
{
	struct hisi_drm_gem *hisi_gem = NULL;

	if (!ctx || !ctx->priv) {
		pr_heaps_err("invalid args for remove trace\n");
		return;
	}

	hisi_gem = ctx->priv;
	spin_lock(&hisi_gem_tracer->rb_lock);
	rb_erase(&hisi_gem->node, &hisi_gem_tracer->gems);
	spin_unlock(&hisi_gem_tracer->rb_lock);
}

/**
 * link gem into rb_tree, so we can trace it
 */
void hisi_drm_heaps_gem_trace(struct hisi_drm_heap_context *ctx)
{
	struct rb_node **p = &hisi_gem_tracer->gems.rb_node;
	struct rb_node *parent = NULL;
	struct hisi_drm_gem *buffer_gem = NULL;
	struct hisi_drm_gem *entry_gem = NULL;

	if (!ctx || !ctx->priv) {
		pr_heaps_err("invalid args for trace\n");
		return;
	}

	buffer_gem = ctx->priv;
	hisi_drm_heaps_dump_info(ctx);

	spin_lock(&hisi_gem_tracer->rb_lock);
	while (*p) {
		parent = *p;
		entry_gem = rb_entry(parent, struct hisi_drm_gem, node);

		if (buffer_gem < entry_gem) {
			p = &(*p)->rb_left;
		} else if (buffer_gem > entry_gem) {
			p = &(*p)->rb_right;
		} else {
			pr_heaps_err("hisi gem is already exist");
			BUG();
		}
	}

	rb_link_node(&buffer_gem->node, parent, p);
	rb_insert_color(&buffer_gem->node, &hisi_gem_tracer->gems);
	spin_unlock(&hisi_gem_tracer->rb_lock);
}

static inline int hisi_drm_heaps_init_debugfs(void)
{
	return 0;
}

/**
 * show gem dump info which file is dma_buf_file and dma_buf is drm's dma_buf
 * @data task_struct
 * @f task's open file
 * @fd file's fd
 */
static int hisi_drm_debug_process_cb(const void *data, struct file *f,
				     unsigned int fd)
{
	const struct task_struct *tsk = data;
	struct dma_buf *dma_buf = NULL;
	struct hisi_drm_gem *hisi_gem = NULL;

	if (!is_dma_buf_file(f))
		return 0;

	dma_buf = file_to_dma_buf(f);
	if (!dma_buf || !is_hisi_drm_dmabuf(dma_buf))
		return 0;

	hisi_gem = to_hisi_gem(dma_buf->priv);
	if (!hisi_gem)
		return 0;

	pr_heaps_err("%16s %16u %16u %#16lx %16u %16s %16u\n", tsk->comm,
		     tsk->pid, fd, dma_buf->size, hisi_gem->pid,
		     hisi_gem->task_comm, hisi_gem->heap_id);
	return 0;
}

/**
 * when alloc limit the time(maybe 500ms), show all task info which contains DRM
 * dma_buf
 */
void hisi_drm_heaps_process_show(void)
{
	struct task_struct *tsk = NULL;

	pr_heaps_err("Process HISI DRM Heap info:\n");
	pr_heaps_err("%16s %16s %16s %16s %16s %16s %16s\n", "Process name",
		     "Process ID", "FD", "Size(0x)", "PID", "Task Name", "Heap ID");
	rcu_read_lock();
	for_each_process (tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		task_lock(tsk);
		iterate_fd(tsk->files, 0, hisi_drm_debug_process_cb,
			   (void *)tsk);
		task_unlock(tsk);
	}
	rcu_read_unlock();
}

void hisi_drm_gem_info_show(struct hisi_drm_gem *hisi_gem)
{
	if (unlikely(!hisi_gem)) {
		pr_heaps_err("invalid gem input\n");
		return;
	}

	pr_heaps_err("%16s %16s %16s %16s %16s\n", "Process name", "Process ID",
		     "Size(0x)", "Kmap_cnt", "Heap Id");
	pr_heaps_err("%16s %16u %#16lx %16u %16u\n", hisi_gem->task_comm,
		     hisi_gem->pid, hisi_gem->size, hisi_gem->kmap_cnt,
		     hisi_gem->heap_id);
}

int hisi_drm_heaps_tracer_init(void)
{
	int ret = 0;

	hisi_gem_tracer = kzalloc(sizeof(*hisi_gem_tracer), GFP_KERNEL);
	if (!hisi_gem_tracer)
		return -ENOMEM;

	hisi_gem_tracer->gems = RB_ROOT;

	spin_lock_init(&hisi_gem_tracer->rb_lock);

	ret = hisi_drm_heaps_init_debugfs();
	if (ret)
		return ret;

	return ret;
}
