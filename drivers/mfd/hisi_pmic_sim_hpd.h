/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 *
 * pmic_sim_hpd.h
 *
 * pmic sim hpd irq process

 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _HISI_PMIC_SIM_HPD_H_
#define _HISI_PMIC_SIM_HPD_H_

#include <linux/bitops.h>

#define LDO_DISABLE 0x00
#define LDO_ONOFF_MASK 0x01

#define SIM_ENABLE 0x04
#define SIM_ONOFF_MASK 0x05
/* main pmic sim hpd process interface */
static inline void pmic_sim_hpd_proc(void)
{
}

/* sub pmic sim hpd process interface */
static inline void sub_pmic_sim_hpd_proc(void)
{
}
#endif
