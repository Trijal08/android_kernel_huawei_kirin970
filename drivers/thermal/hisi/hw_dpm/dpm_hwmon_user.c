/*
 * dpm_hwmon_user.c
 *
 * dpm interface for user
 *
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
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

#include <linux/hisi/dpm_hwmon_user.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <securec.h>

#ifdef CONFIG_DPM_HWMON_V1
#include "dpm_hwmon_v1.h"
#elif CONFIG_DPM_HWMON_V2
#include <linux/suspend.h>
#include "dpm_hwmon_v2.h"
#endif

#define DPM_SWITCH_MASK		0x80000000
#define DPM_PERI_ENABLE_MASK		0x1
#define DPM_GPU_ENABLE_MASK		0x2
#define DPM_NPU_ENABLE_MASK		0x4

#define DPM_SAMPLE_INTERVAL		1000

static struct delayed_work g_dpm_hwmon_work;
bool g_dpm_report_enabled = true;

static struct dpm_hwmon_ops *search_dpm_module(int dpm_id)
{
	struct dpm_hwmon_ops *pos = NULL;
	struct dpm_hwmon_ops *tmp = NULL;

	mutex_lock(&g_dpm_hwmon_ops_list_lock);
	list_for_each_entry(tmp, &g_dpm_hwmon_ops_list, ops_list) {
		if (tmp->dpm_module_id == dpm_id) {
			pos = tmp;
			break;
		}
	}
	mutex_unlock(&g_dpm_hwmon_ops_list_lock);
	return pos;
}

unsigned int dpm_peri_num(void)
{
	struct dpm_hwmon_ops *pos = NULL;
	unsigned int count  = 0;

	list_for_each_entry(pos, &g_dpm_hwmon_ops_list, ops_list)
		if (pos->dpm_type == DPM_PERI_MODULE)
			count++;

	return count;
}

unsigned long long get_dpm_chdmod_power(int dpm_id)
{
	struct dpm_hwmon_ops *pos = NULL;
	unsigned long long dpm_power;

	pos = search_dpm_module(dpm_id);
	dpm_power = get_dpm_power(pos);
	return dpm_power;
}

#ifdef CONFIG_DPM_HWMON_V2
static int dpm_hwmon_pm_callback(struct notifier_block *nb,
				 unsigned long action, void *ptr)
{
	if (nb == NULL || ptr == NULL)
		pr_info("dpm_hwmon only for sc!\n");

	switch (action) {
	case PM_SUSPEND_PREPARE:
		pr_info("dpm_hwmon suspend\n");
		break;

	case PM_POST_SUSPEND:
		pr_info("dpm_hwmon resume +\n");
		if (g_dpm_peri_pcr_vote > 0)
			dpm_enable_peri_pcr();
		pr_info("dpm_hwmon resume -\n");
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

struct notifier_block dpm_hwmon_pm_notif_block = {
	.notifier_call = dpm_hwmon_pm_callback,
};
#endif

bool check_dpm_enabled(int dpm_type)
{
	struct dpm_hwmon_ops *pos = NULL;

	list_for_each_entry(pos, &g_dpm_hwmon_ops_list, ops_list) {
		if (pos->dpm_type == dpm_type)
			return get_dpm_enabled(pos);
	}
	return false;
}

void dpm_parse_switch_cmd(unsigned int cmd)
{
	struct dpm_hwmon_ops *pos = NULL;
	bool dpm_switch = ((cmd & DPM_SWITCH_MASK) != 0);
	bool peri_enabled = ((cmd & DPM_PERI_ENABLE_MASK) != 0);
	bool gpu_enabled = ((cmd & DPM_GPU_ENABLE_MASK) != 0);
	bool npu_enabled = ((cmd & DPM_NPU_ENABLE_MASK) != 0);

	list_for_each_entry(pos, &g_dpm_hwmon_ops_list, ops_list) {
		switch (pos->dpm_type) {
		case DPM_PERI_MODULE:
			if (peri_enabled) {
#ifdef CONFIG_DPM_HWMON_V2
				peri_pcr_vote(dpm_switch, DPM_PERI_ENABLE_MASK);
#endif
				dpm_enable_module(pos, dpm_switch);
			}
			break;
		case DPM_GPU_MODULE:
			if (gpu_enabled) {
#ifdef CONFIG_DPM_HWMON_V2
				peri_pcr_vote(dpm_switch, DPM_GPU_ENABLE_MASK);
#endif
				dpm_enable_module(pos, dpm_switch);
			}
			break;
		case DPM_NPU_MODULE:
			if (npu_enabled)
				dpm_enable_module(pos, dpm_switch);
			break;
		default:
			break;
		}
	}
}

int get_dpm_peri_data(struct dubai_transmit_t *peri_data, unsigned int peri_num)
{
	int ret;
	unsigned int count = 0;
	struct dpm_hwmon_ops *pos = NULL;
	struct ip_energy *dpm_ip = NULL;

	if (peri_data == NULL) {
		pr_err("%s peri data is null\n", __func__);
		return -EFAULT;
	}
	if (peri_num <= 0 || peri_num > DPM_MODULE_NUM) {
		pr_err("%s:%u, invalid peri_num!", __func__, peri_num);
		return -EFAULT;
	}

	list_for_each_entry(pos, &g_dpm_hwmon_ops_list, ops_list) {
		if (pos->dpm_type == DPM_PERI_MODULE && count < peri_num) {
			dpm_ip = (struct ip_energy *)(peri_data->data) + count;
			ret = strcpy_s(dpm_ip->name, DPM_NAME_SIZE,
				       dpm_module_table[pos->dpm_module_id]);
			if (ret != EOK) {
				pr_err("%s strcpy_s fail, ret is %d\n",
				       __func__, ret);
				return -EFAULT;
			}
			dpm_ip->energy = get_dpm_power(pos);
			count++;
		}
	}

	if (count != peri_num) {
		pr_err("peri num is not consistent %u vs %u!\n", count, peri_num);
		return -EFAULT;
	}

	peri_data->length = count * sizeof(struct ip_energy);
	return 0;
}

static void dpm_hwmon_sample_func(struct work_struct *work)
{
	struct dpm_hwmon_ops *pos = NULL;

	if (!g_dpm_report_enabled)
		goto restart_work;
	list_for_each_entry(pos, &g_dpm_hwmon_ops_list, ops_list)
		dpm_sample(pos);

restart_work:
	queue_delayed_work(system_freezable_power_efficient_wq,
			   &g_dpm_hwmon_work,
			   msecs_to_jiffies(DPM_SAMPLE_INTERVAL));
}

void update_dpm_power(int dpm_id)
{
	struct dpm_hwmon_ops *pos = NULL;

	pos = search_dpm_module(dpm_id);
	if (pos != NULL)
		(void)dpm_sample(pos);
}

static int __init dpm_hwmon_init(void)
{
	g_dpm_report_enabled = true;
#ifdef CONFIG_DPM_HWMON_V2
	if (dpm_ioremap() < 0)
		return -EFAULT;
#endif
	/* dpm workqueue initialize */
	INIT_DEFERRABLE_WORK(&g_dpm_hwmon_work, dpm_hwmon_sample_func);
	queue_delayed_work(system_freezable_power_efficient_wq,
			   &g_dpm_hwmon_work,
			   msecs_to_jiffies(DPM_SAMPLE_INTERVAL));
#ifdef CONFIG_DPM_HWMON_V2
	register_pm_notifier(&dpm_hwmon_pm_notif_block);
#endif
	return 0;
}

static void __exit dpm_hwmon_exit(void)
{
	cancel_delayed_work(&g_dpm_hwmon_work);

#ifdef CONFIG_DPM_HWMON_V2
	dpm_iounmap();
#endif
}

module_init(dpm_hwmon_init);
module_exit(dpm_hwmon_exit);
