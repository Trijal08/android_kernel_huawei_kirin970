/* Copyright (c) Hisilicon Technologies Co., Ltd. 2012-2019. All rights reserved.
 * FileName: kernel/drivers/mtd/hisi_nve.c
 * Description: complement NV partition(or called block) read and write
 * in kernel.
 * Author: hisilicon
 * Create: 2012-12-22
 * Revision history:2019-06-20 NVE CSEC
 */
#define pr_fmt(fmt) "[NVE]" fmt

#include "hisi_nve.h"
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/compat.h>
#include <linux/syscalls.h>
#include <linux/hisi/partition_ap_kernel.h>
#include <linux/version.h>

#ifdef CONFIG_CRC_SUPPORT
#include "hisi_nve_crc.h"
#endif

#if CONFIG_HISI_NVE_WHITELIST
#include "hisi_nve_whitelist.h"
#endif

static struct semaphore nv_sem;
static struct class *nve_class;
static unsigned int dev_major_num;
static struct nve_ramdisk_struct *nve_data_struct;
static int hisi_nv_setup_ok;
static char *nve_block_device_name;
#if CONFIG_HISI_NVE_WHITELIST
static int nve_whitelist_en = 1;
#endif
/* The number of valid nv items */
static unsigned int valid_nv_item_num;
static int nve_later_setup(void);
struct mutex nve_set_mutex;

/*
 * Function name:update_header_valid_nvme_items.
 * Discription:update the actual valid item in ramdisk
 * Parameters:
 * @ nve_ramdisk:the ramdisk stores the total nv partition
 */
static int update_header_valid_nvme_items(
				struct nv_partittion_struct *nve_ramdisk)
{
	unsigned int i;
	struct nv_items_struct nve_item = { 0 };
	unsigned int valid_items;

	if (!nve_ramdisk) {
		pr_err("[%s]line:%d nve_ramdisk is NULL.\n",
			__func__, __LINE__);
		return NVE_ERROR;
	}

	if (nve_ramdisk->header.valid_items > NV_ITEMS_MAX_NUMBER) {
		pr_err("[%s]line:%d valid_items is biger than 1023\n",
			__func__, __LINE__);
		return NVE_ERROR;
	}

	for (i = 0; i < nve_ramdisk->header.valid_items; i++) {
		/* find nve item's nv_number and check */
		nve_item = nve_ramdisk->nv_items[i];
		if (i != nve_item.nv_number)
			break;
	}
	valid_items = i;
	/* update ram valid_items.
	 * when the driver is just started, data is not loaded into memory,
	 * valid_nv_item_num is 0
	 */
	if (valid_nv_item_num == 0) {
		nve_ramdisk->header.valid_items = valid_items;
		return 0;
	}
	/* After the data is loaded, the read and write operations go here.
	 * The following unequalities indicate that the data read from
	 * the device is corrupted.
	 */
	if (valid_nv_item_num != valid_items) {
		pr_err("[%s]error, valid_items(%u) isnot equal to valid_nv_item_num(%u),device read data is broken.\n",
			__func__, valid_items, valid_nv_item_num);
		return NVE_ERROR;
	}
	return 0;
}

#ifdef CONFIG_CRC_SUPPORT
/*
 * Function name:check_crc_for_valid_items.
 * Discription:check the crc for nv items
 * Parameters:
 * @ nv_item_start:the start nv number
 * @ check_items:the number of check items
 * @ nve_ramdisk:the ramdisk stores the total nv partition
 * return value:
 *   0:check success;
 *   others:check failed
 */
static int check_crc_for_valid_items(unsigned int nv_item_start,
				unsigned int check_items,
				struct nv_partittion_struct *nve_ramdisk)
{
	int i;
	uint8_t crc_data[NVE_NV_CRC_HEADER_SIZE + NVE_NV_DATA_SIZE] = { 0 }; /*lint !e302*/
	struct nv_items_struct *nv_item = NULL;
	uint32_t temp_crc;
	int nv_number;

	/* nv_item_start means the start nv number, range is 0-1022 */
	if (nv_item_start >= NV_ITEMS_MAX_NUMBER) {
		pr_err("invalid nv_item_start in check crc fuction, nv_item_start = %u\n",
		       nv_item_start);
		return -EINVAL;
	}

	/* check_items means the number of chek nv items, range is 1-1023 */
	if (check_items > NV_ITEMS_MAX_NUMBER || check_items <= 0) {
		pr_err("invalid check_items in check crc fuction, check_items = %u\n",
		       check_items);
		return -EINVAL;
	}

	if (!nve_ramdisk) {
		pr_err("[%s]line:%d nve_ramdisk is NULL.\n", __func__,
		       __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < (int)check_items; i++) {
		nv_number = nv_item_start + i;
		if (nv_number >= NV_ITEMS_MAX_NUMBER) {
			pr_err("invalid nv number in check crc fuction, nv_item_start = %u, check_items = %u\n",
			       nv_item_start, check_items);
			return -EINVAL;
		}
		nv_item = &nve_ramdisk->nv_items[nv_number];
		memcpy(crc_data, nv_item, NVE_NV_CRC_HEADER_SIZE);
		memcpy(crc_data + NVE_NV_CRC_HEADER_SIZE, nv_item->nv_data,
		       (size_t)NVE_NV_DATA_SIZE);
		temp_crc = ~crc32c_nve(CRC32C_REV_SEED, crc_data,
				       sizeof(crc_data));
		if (nv_item->crc != temp_crc) {
			pr_err("crc check failed, item {%u}, old_crc_value = 0x%x, new_crc_value = 0x%x\n",
			       nv_number, nv_item->crc, temp_crc);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Function name:caculate_crc_for_valid_items.
 * Discription:caculate crc for nv item and update
 * Parameters:
 * @ nv_items:the pointer of nv item
 */
static void caculate_crc_for_valid_items(struct nv_items_struct *nv_items)
{
	uint8_t crc_data[NVE_NV_CRC_HEADER_SIZE + NVE_NV_DATA_SIZE] = { 0 }; /*lint !e302*/

	if (!nv_items) {
		pr_err("[%s]line:%d nv_items is NULL.\n", __func__,
		       __LINE__);
		return;
	}

	memcpy(crc_data, nv_items, NVE_NV_CRC_HEADER_SIZE);
	memcpy(crc_data + NVE_NV_CRC_HEADER_SIZE, nv_items->nv_data,
	       (size_t)NVE_NV_DATA_SIZE);
	nv_items->crc =
		~crc32c_nve(CRC32C_REV_SEED, crc_data, sizeof(crc_data));
}
#endif

/*
 * Function name:nve_increment.
 * Discription:complement NV block' increment automatically, when current block
 * has been writeen, block index will pointer to next block, and if current
 * block
 * is maximum block count ,then block index will be assigned "1", ensuring all
 * of
 * NV block will be used and written circularly.
 */
static void nve_increment(struct nve_ramdisk_struct *nvep)
{
	if (!nvep) {
		pr_err("[%s]line:%d nvep is NULL.\n", __func__, __LINE__);
		return;
	}

	if (nvep->nve_current_id >= nvep->nve_partition_count - 1)
		nvep->nve_current_id = 1;
	else
		nvep->nve_current_id++;
}

/*
 * Function name:nve_decrement.
 * Discription:complement NV block' decrement automatically, when current block
 * has been writeen, block index will pointer to next block, if write failed,
 * we will recover the index
 */
static void nve_decrement(struct nve_ramdisk_struct *nvep)
{
	if (!nvep) {
		pr_err("[%s]line:%d nvep is NULL.\n", __func__, __LINE__);
		return;
	}

	/* we only use 1-7 partition, if id is 1,
	 * next decrement id is 7, after init
	 * nvep->nve_partition_count is 8
	 */
	if (nvep->nve_current_id <= 1)
		nvep->nve_current_id = nvep->nve_partition_count - 1;
	else
		nvep->nve_current_id--;
}

/*
 * Function name:nve_read.
 * Discription:read NV partition to buf.
 * Parameters:
 *          @ from:emmc start block number that will be read.
 *          @ len:total bytes that will be read from emmc.
 *          @ buf:buffer used to store bytes that is read from emmc.
 */
static int nve_read(loff_t from, size_t len, u_char *buf)
{
	int ret;
	int fd = -1;

	/*lint -e501*/
	mm_segment_t oldfs = get_fs();

	set_fs(get_ds());
	/*lint +e501*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	fd = ksys_open(nve_block_device_name, O_RDONLY, 0);
#else
	fd = sys_open(nve_block_device_name, O_RDONLY, 0);
#endif
	if (fd < 0) {
		pr_err("[%s]open nv block device failed, and fd = %d!\n",
		       __func__, fd);
		ret = -ENODEV;
		goto err;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ret = ksys_lseek((unsigned int)fd, from, SEEK_SET);
#else
	ret = sys_lseek((unsigned int)fd, from, SEEK_SET);
#endif
	if (ret < 0) {
		pr_err("[%s] Fatal seek error, read flash from = 0x%llx, len = 0x%zx, ret = %d.\n",
		       __func__, from, len, ret);
		ret = -EIO;
		goto out;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ret = ksys_read((unsigned int)fd, (char *)buf, len);
#else
	ret = sys_read((unsigned int)fd, (char *)buf, len);
#endif
	if (ret < 0) {
		pr_err("[%s] Fatal read error, read flash from = 0x%llx, len = 0x%zx, ret = %d.\n",
		       __func__, from, len, ret);
		ret = -EIO;
		goto out;
	} else {
		ret = 0;
	}

out:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ksys_close((unsigned int)fd);
#else
	sys_close((unsigned int)fd);
#endif
err:
	set_fs(oldfs);
	return ret;
}

/*
 * Function name:nve_write.
 * Discription:write NV partition.
 * Parameters:
 *          @ from:emmc start block number that will be written.
 *          @ len:total bytes that will be written from emmc.
 *          @ buf:given buffer whose bytes will be written to emmc.
 */
static int nve_write(loff_t from, size_t len, u_char *buf)
{
	int ret;
	int fd = -1;

	/*lint -e501*/
	mm_segment_t oldfs = get_fs();

	set_fs(get_ds());
	/*lint +e501*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	fd = ksys_open(nve_block_device_name, O_RDWR, 0);
#else
	fd = sys_open(nve_block_device_name, O_RDWR, 0);
#endif
	if (fd < 0) {
		pr_err("[%s]open nv block device failed, and fd = %x!\n",
		       __func__, fd);
		ret = -ENODEV;
		goto err;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ret = ksys_lseek((unsigned int)fd, from, SEEK_SET);
#else
	ret = sys_lseek((unsigned int)fd, from, SEEK_SET);
#endif
	if (ret < 0) {
		pr_err("[%s] Fatal seek error, read flash from = 0x%llx, len = 0x%zx, ret = 0x%x.\n",
		       __func__, from, len, ret);
		ret = -EIO;
		goto out;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ret = ksys_write((unsigned int)fd, (char *)buf, len);
#else
	ret = sys_write((unsigned int)fd, (char *)buf, len);
#endif
	if (ret < 0) {
		pr_err("[%s] Fatal write error, read flash from = 0x%llx, len = 0x%zx, ret = 0x%x.\n",
		       __func__, from, len, ret);
		ret = -EIO;
		goto out;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ksys_sync();
	ret = 0;
#else
	ret = sys_fsync((unsigned int)fd);
	if (ret < 0) {
		pr_err("[%s] Fatal sync error, read flash from = 0x%llx, len = 0x%zx, ret = 0x%x.\n",
		       __func__, from, len, ret);
		ret = -EIO;
		goto out;
	} else {
		ret = 0;
	}
#endif

out:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ksys_close((unsigned int)fd);
#else
	sys_close((unsigned int)fd);
#endif
err:
	set_fs(oldfs);
	return ret;
}

/*
 * Function name:nve_check_partition.
 * Discription:check current NV partition is valid partition or not by means of
 * comparing current partition's name to NVE_HEADER_NAME.
 * Parameters:
 *          @ ramdisk:struct nve_ramdisk_struct pointer.
 *          @ index  :indicates current NV partion that will be checked.
 * return value:
 *          @ 0 - current parition is valid.
 *          @ others - current parition is invalid.
 */
static int nve_check_partition(
			struct nv_partittion_struct *ramdisk, uint32_t index)
{
	int ret;
	struct nve_partition_header_struct *nve_partition_header = NULL;

	if ((!ramdisk) || (index >= NVE_PARTITION_COUNT)) {
		pr_err("[%s]Input error in line %d!\n", __func__,
		       __LINE__);
		return -EINVAL;
	}

	nve_partition_header = &ramdisk->header;

	ret = nve_read((loff_t)index * NVE_PARTITION_SIZE,
		       (size_t)NVE_PARTITION_SIZE, (u_char *)ramdisk);
	if (ret) {
		pr_err("[%s]nve_read error in line %d!\n",
			__func__, __LINE__);
		return ret;
	}

	/* update for valid nvme items */
	ret = update_header_valid_nvme_items(ramdisk);
	if (ret) {
		pr_err("[%s]line:%d  valid_nv_items error!\n",
			__func__, __LINE__);
		return ret;
	}

	/* compare partition_name with const name,
	 * if return 0,then current partition is valid
	 */
	ret = strncmp(NVE_HEADER_NAME,
		      nve_partition_header->nve_partition_name,
		      sizeof(NVE_HEADER_NAME));
	if (ret)
		return ret;

#ifdef CONFIG_CRC_SUPPORT
	if (nve_partition_header->nve_crc_support ==
	    NVE_CRC_SUPPORT_VERSION) {
		/* check the crc for valid nvme items */
		ret = check_crc_for_valid_items(
			0, nve_partition_header->valid_items, ramdisk);
		if (ret) {
			pr_err("index = %u, valid_items = %u, version = %u, crc_support = %u\n",
			       index, nve_partition_header->valid_items,
			       nve_partition_header->nve_version,
			       nve_partition_header->nve_crc_support);
			return ret;
		}
	}
#endif

	return 0;
}

/*
 * Function name:nve_find_valid_partition.
 * Discription:find valid NV partition in terms of checking every
 * partition circularly. when two or more NV paritions are both valid,
 * nve_age will be used to indicates which one is really valid, i.e. the
 * partition whose age is the biggest is valid partition.
 */
static void nve_find_valid_partition(struct nve_ramdisk_struct *nvep)
{
	uint32_t i;
	uint32_t age_temp = 0;
	int partition_invalid;
	struct nve_partition_header_struct *nve_partition_header = NULL;

	if (!nvep) {
		pr_err("[%s]line:%d nvep is NULL.\n", __func__, __LINE__);
		return;
	}

	nve_partition_header = &nvep->nve_store_ramdisk->header;
	nvep->nve_current_id = NVE_INVALID_NVM;
	/* Nv partition is 1-7 */
	for (i = 1; i < nvep->nve_partition_count; i++) {
		partition_invalid =
			nve_check_partition(nvep->nve_store_ramdisk, i);
		if (partition_invalid)
			continue;
		if (nve_partition_header->nve_age > age_temp) {
			nvep->nve_current_id = i;
			age_temp = nve_partition_header->nve_age;
		}
	}

	pr_info("[%s]current_id = %u valid_items = %u, version = %u, crc_support = %u\n",
		__func__, nvep->nve_current_id,
		nve_partition_header->valid_items,
		nve_partition_header->nve_version,
		nve_partition_header->nve_crc_support);
}

static int write_ramdisk_to_device(unsigned int id,
				   struct nv_partittion_struct *ramdisk)
{
	struct nve_partition_header_struct *nve_partition_header = NULL;
	int ret;
	loff_t start_addr;
	unsigned int nve_update_age;

	if (id >= NVE_PARTITION_COUNT) {
		pr_err("[%s]invalid id in line %d!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!ramdisk) {
		pr_err("[%s]line:%d ramdisk is NULL.\n", __func__,
		       __LINE__);
		return -EINVAL;
	}

	nve_partition_header = &ramdisk->header;
	nve_update_age = nve_partition_header->nve_age + 1;
	/* write to next partition a invalid age */
	nve_partition_header->nve_age = NVE_PARTITION_INVALID_AGE;
	/* write the old partition head */
	start_addr = (((loff_t)id + 1) * NVE_PARTITION_SIZE) - NVE_BLOCK_SIZE;
	ret = nve_write(start_addr, (size_t)NVE_BLOCK_SIZE,
			((unsigned char *)ramdisk + NVE_PARTITION_SIZE - NVE_BLOCK_SIZE));
	if (ret) {
		pr_err("[%s]write old nv partition head failed in line %d!\n",
		       __func__, __LINE__);
		/* recover the age */
		nve_partition_header->nve_age = nve_update_age - 1;
		return ret;
	}

	/* write the partition data */
	start_addr = (loff_t)id * NVE_PARTITION_SIZE;
	ret = nve_write(start_addr, (size_t)NVE_PARTITION_SIZE - NVE_BLOCK_SIZE,
			(unsigned char *)ramdisk);
	if (ret) {
		pr_err("[%s]write nv partition data failed in line %d!\n",
		       __func__, __LINE__);
		/* recover the age */
		nve_partition_header->nve_age = nve_update_age - 1;
		return ret;
	}

	/* after writing partition to device, read the partition again to check,
	 * if check not pass, return error and not update header age
	 */
	ret = nve_check_partition(ramdisk, id);
	if (ret) {
		pr_err("[%s]after writing partition to device, read the partition again to check failed!\n",
		       __func__);
		/* recover the age */
		nve_partition_header->nve_age = nve_update_age - 1;
		return ret;
	}

	/* update the partition head age */
	nve_partition_header->nve_age = nve_update_age;
	/* write the latest partition head */
	start_addr = (((loff_t)id + 1) * NVE_PARTITION_SIZE) - NVE_BLOCK_SIZE;
	ret = nve_write(start_addr, (size_t)NVE_BLOCK_SIZE,
			((unsigned char *)ramdisk + NVE_PARTITION_SIZE - NVE_BLOCK_SIZE));
	if (ret) {
		pr_err("[%s]write nv latest partition head failed in line %d!\n",
		       __func__, __LINE__);
		return ret;
	}

	return ret;
}

/*
 * Function name:nve_open_ex.
 * Discription:open NV device.
 * return value:
 *          @ 0 - success.
 *          @ -1- failure.
 */
static int read_nve_to_ramdisk(void)
{
	int ret = 0;

	/* the driver is not initialized successfully, return error */
	if (!nve_data_struct) {
		ret = -ENOMEM;
		pr_err("[%s]:nve_data_struct has not been alloced.\n",
		       __func__);
		goto out;
	}

	/* Total avaliable NV partition size is 4M,but we only use 1M */
	nve_data_struct->nve_partition_count = NVE_PARTITION_COUNT;

	nve_find_valid_partition(nve_data_struct);
	if (nve_data_struct->nve_current_id == NVE_INVALID_NVM) {
		pr_err("[%s]: no valid NVM.\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	/* read the current NV partiton and store into Ramdisk */
	if (nve_read((unsigned long)nve_data_struct->nve_current_id *
			     NVE_PARTITION_SIZE,
		     (size_t)NVE_PARTITION_SIZE,
		     (unsigned char *)nve_data_struct->nve_current_ramdisk)) {
		pr_err("[%s]:nve_read error!\n", __func__);
		return -EIO;
	}

	ret = update_header_valid_nvme_items(nve_data_struct->nve_current_ramdisk);
	if (ret) {
		pr_err("[%s]line:%d valid_nv_items error!\n",
				__func__, __LINE__);
		return ret;
	}

	/* Drive initialization ends, record valid_nv_item_num */
	valid_nv_item_num =
		nve_data_struct->nve_current_ramdisk->header.valid_items;
	pr_err("[%s]line:%d init valid_nv_item_num = %u\n",
				__func__, __LINE__, valid_nv_item_num);
out:
	return ret;
}

#ifdef CONFIG_CRC_SUPPORT
static int nve_check_crc_and_recover(unsigned int nv_item_start,
		unsigned int check_items, struct nve_ramdisk_struct *nvep)
{
	int ret;

	if (!nvep) {
		pr_err("[%s]line:%d nvep is NULL.\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* crc check for ram to one item */
	ret = check_crc_for_valid_items(nv_item_start, check_items,
					nvep->nve_current_ramdisk);
	/* retry to read to ramdisk */
	if (ret) {
		pr_err("[NVE], one item crc check failed, %d!\n", ret);
		/* this place can optmize */
		nve_find_valid_partition(nvep);
		if (nvep->nve_current_id == NVE_INVALID_NVM) {
			pr_err("[NVE]can't find the valid partition in 1-7!\n");
			return -EIO;
		}

		/* read the current NV partiton and store to Ramdisk */
		if (nve_read((unsigned long)nvep->nve_current_id *
				     NVE_PARTITION_SIZE,
			     NVE_PARTITION_SIZE,
			     (unsigned char *)
				     nvep->nve_current_ramdisk)) {
			pr_err("[NVE]nve_read error!\n");
			return -EIO;
		}

		ret = update_header_valid_nvme_items(
			nvep->nve_current_ramdisk);
		if (ret) {
			pr_err("[%s]line:%d  valid_nv_items error!\n",
				__func__, __LINE__);
			return ret;
		}
	}

	return 0;
}
#endif

#if CONFIG_HISI_NVE_WHITELIST
static bool nve_number_in_whitelist(uint32_t nv_number)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(nv_num_whitelist); i++) {
		if (nv_number == nv_num_whitelist[i])
			return true;
	}

	return false;
}

static bool nve_process_in_whitelist(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(nv_process_whitelist); i++) {
		if (!strncmp(current->comm, nv_process_whitelist[i],
			     strlen(nv_process_whitelist[i]) + 1)) {
			return true;
		}
	}

	return false;
}

static int hisi_nve_whitelist_check(struct hisi_nve_info_user *user_info)
{
	/* input parameter invalid, return */
	if (!user_info) {
		pr_err("[%s] input parameter is NULL.\n", __func__);
		return -EINVAL;
	}

	if (nve_whitelist_en && (user_info->nv_operation != NV_READ)) {
		if (nve_number_in_whitelist(user_info->nv_number) &&
		    (!nve_process_in_whitelist() || !get_userlock())) {
			pr_err("%s nv_number: %d process %s is not in whitelist, or phone was unlocked. Forbid write to NVE!\n",
			       __func__, user_info->nv_number, current->comm);

			return -EPERM;
		}
	}

	return 0;
}
#endif /* CONFIG_HISI_NVE_WHITELIST  */

static int hisi_nve_direct_access_for_ramdisk(
		struct hisi_nve_info_user *user_info)
{
	int ret = 0;
	struct nv_items_struct *nv_item = NULL;
	struct nv_items_struct nv_item_backup = { 0 };
	struct nve_partition_header_struct *nve_partition_header = NULL;

#if CONFIG_HISI_NVE_WHITELIST
	if (hisi_nve_whitelist_check(user_info)) {
		pr_err("[%s] hisi_nve_whitelist_check Failed!\n", __func__);
		return -EINVAL;
	}
#else
	/* input parameter invalid, return */
	if (!user_info) {
		pr_err("[%s] input parameter is NULL.\n", __func__);
		return -EINVAL;
	}
#endif /* CONFIG_HISI_NVE_WHITELIST */

	if (!nve_data_struct->nve_current_ramdisk) {
		pr_err("[%s] nve_data_struct->nve_current_ramdisk is NULL.\n",
			__func__);
		return -EINVAL;
	}

	/* get nve partition header to check nv number */
	nve_partition_header = &nve_data_struct->nve_current_ramdisk->header;
	if (user_info->nv_number >= nve_partition_header->valid_items) {
		pr_err("[%s] The input NV items[%d] is out of range,not exist.\n",
		       __func__, user_info->nv_number);
		ret = -EINVAL;
		goto out;
	}

	if (user_info->nv_number >= NV_ITEMS_MAX_NUMBER) {
		pr_err("[%s] nv_number is too big\n", __func__);
		return -EINVAL;
	}

	/* check nv valid size, it is same to old */
	nv_item = &nve_data_struct->nve_current_ramdisk
			   ->nv_items[user_info->nv_number];
	if (user_info->valid_size > nv_item->valid_size ||
	    user_info->valid_size == 0) {
		pr_err("[%s]info:Input valid_size = %d, original = %d!\n",
			__func__, user_info->valid_size, nv_item->valid_size);
		user_info->valid_size = nv_item->valid_size;
	}

	if (user_info->valid_size > NVE_NV_DATA_SIZE) {
		pr_err("[%s] user info valid size is error, %d.\n",
		       __func__, user_info->valid_size);
		ret = -EINVAL;
		goto out;
	}

	if (user_info->nv_operation == NV_READ) {
#ifdef CONFIG_CRC_SUPPORT
		if (nve_partition_header->nve_crc_support ==
		    NVE_CRC_SUPPORT_VERSION) {
			ret = nve_check_crc_and_recover(
				user_info->nv_number, 1, nve_data_struct);
			if (ret) {
				pr_err("[NVE]nve_check_crc_and_recover failed, ret = %d\n",
				       ret);
				return ret;
			}
		}
#endif
		/* read nv from ramdisk */
		memcpy(user_info->nv_data, nv_item->nv_data,
			(size_t)user_info->valid_size);
	} else {
#ifdef CONFIG_CRC_SUPPORT
		if (nve_partition_header->nve_crc_support ==
		    NVE_CRC_SUPPORT_VERSION) {
			ret = nve_check_crc_and_recover(
				0, nve_partition_header->valid_items,
				nve_data_struct);
			if (ret) {
				pr_err("[NVE]nve_check_crc_and_recover failed, ret = %d\n",
				       ret);
				return ret;
			}
		}
#endif

		/* backup the original item,
		 * if write to device failed, we will recover the item
		 */
		memcpy(&nv_item_backup, nv_item,
			sizeof(struct nv_items_struct));
		/* write nv to ram */
		memset(nv_item->nv_data, 0x0, (unsigned long)NVE_NV_DATA_SIZE);
		memcpy(nv_item->nv_data, user_info->nv_data,
			(unsigned long)user_info->valid_size); /*[false alarm]:valid_size <= NVE_NV_DATA_SIZE */
#ifdef CONFIG_CRC_SUPPORT
		if (nve_partition_header->nve_crc_support ==
		    NVE_CRC_SUPPORT_VERSION)
			/* caculate crc to update */
			caculate_crc_for_valid_items(nv_item);
#endif
		/* update the current id */
		nve_increment(nve_data_struct);
		/* write the total ramdisk to device */
		ret = write_ramdisk_to_device(
			nve_data_struct->nve_current_id,
			nve_data_struct->nve_current_ramdisk);
		if (ret) {
			pr_err("[%s]write to device failed in line %d, and nv_number = %d!\n",
			       __func__, __LINE__, user_info->nv_number);
			/* if write to device failed, we will recover
			 * something recover the item
			 */
			memcpy(nv_item, &nv_item_backup,
				sizeof(struct nv_items_struct));
			/* recover the current id */
			nve_decrement(nve_data_struct);
			goto out;
		}
	}
out:
	return ret;
}

/*
 * Function name:hisi_nve_direct_access.
 * Discription:read or write NV items interfaces that will be called by other
 * functions.
 * Parameters:
 *          @ user_info:struct hisi_nve_info_user pointer.
 * return value:
 *          @      0 - success.
 *          @ error - failure.
 * errors:
 *          @  ENOMEM - Out of memory
 *          @  ENODEV - No NVE device
 *          @  EBUSY  - Device or resource busy
 *          @  EFAULT - Bad address
 *          @  EINVAL - Invalid argument
 *          @  ENOTTY - Unknow command
 */
int hisi_nve_direct_access(struct hisi_nve_info_user *user_info)
{
	int ret;

	if (!nve_data_struct) {
		pr_err("[%s] NVE struct not alloc.\n", __func__);
		return -ENOMEM;
	}
	/* the interface check the nv init */
	if (!hisi_nv_setup_ok) {
		ret = nve_later_setup();
		if (ret) {
			pr_err("[%s] NVE init is not done, please wait.\n",
			       __func__);
			return -ENODEV;
		}
	}

	/* ensure only one process can visit NV at the same time in
	 * kernel
	 */
	if (down_interruptible(&nv_sem))
		return -EBUSY;

	ret = hisi_nve_direct_access_for_ramdisk(user_info);
	if (ret)
		pr_err("[%s]access for nve according ramdisk failed in line %d!\n",
		       __func__, __LINE__);

	/* release the semaphore */
	up(&nv_sem);
	return ret;
}
/* export hisi_nve_direct_access,so we can use it in other procedures */
EXPORT_SYMBOL(hisi_nve_direct_access);

/*
 * Function name:nve_open.
 * Discription:open NV device in terms of calling nve_open_ex().
 * return value:
 *          @      0 - success.
 *          @ error - failure.
 * errors:
 *          @  ENOMEM - Out of memory
 *          @  ENODEV - No NVE device
 */
static int nve_open(struct inode *inode, struct file *file)
{
	if (!nve_data_struct) {
		pr_err("[%s] NVE struct not alloc.\n", __func__);
		return -ENOMEM;
	}

	if (hisi_nv_setup_ok)
		return 0;
	else
		return -ENODEV;
}

static int nve_close(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * Function name:nve_ioctl.
 * Discription:complement read or write NV by terms of sending command-words.
 * return value:
 *          @      0 - on success.
 *          @ -error - on error.
 * errors:
 *          @  ENOMEM - Out of memory
 *          @  ENODEV - No NVE device
 *          @  EBUSY  - Device or resource busy
 *          @  EFAULT - Bad address
 *          @  EINVAL - Invalid argument
 *          @  ENOTTY - Unknow command
 */
static long nve_ioctl(struct file *file, u_int cmd, u_long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	u_int size = 0;
	struct hisi_nve_info_user info = { 0 };

	/* make sure arg isnot NULL */
	if (!argp) {
		pr_err("[%s] The input arg is null.\n", __func__);
		return -ENOMEM;
	}

	/* make sure nve is init */
	if (!nve_data_struct) {
		pr_err("[%s] NVE struct not alloc.\n", __func__);
		return -ENOMEM;
	}

	/* the interface check the nv init */
	if (!hisi_nv_setup_ok) {
		pr_err("[%s] NVE init is not done, please wait.\n",
		       __func__);
		return -ENODEV;
	}

	/* ensure only one process can visit NV device
	 * at the same time in API
	 */
	if (down_interruptible(&nv_sem))
		return -EBUSY;

	size = ((cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT);

	if (cmd & IOC_IN) {
		if (!access_ok(VERIFY_READ, arg, size)) {
			pr_err("[%s]access_in error!\n", __func__);
			up(&nv_sem);
			return -EFAULT;
		}
	}

	if (cmd & IOC_OUT) {
		if (!access_ok(VERIFY_WRITE, arg, size)) {
			pr_err("[%s]access_out error!\n", __func__);
			up(&nv_sem);
			return -EFAULT;
		}
	}

	switch (cmd) {
	case NVEACCESSDATA:
		if (copy_from_user(&info, argp,
				   sizeof(struct hisi_nve_info_user))) {
			up(&nv_sem);
			return -EFAULT;
		}

		ret = hisi_nve_direct_access_for_ramdisk(&info);
		if (ret) {
			pr_err("[%s]nve access failed in line %d!\n",
			       __func__, __LINE__);
			goto out;
		}

		if (info.nv_operation == NV_READ) {
			if (copy_to_user(argp, &info,
					 sizeof(struct hisi_nve_info_user))) {
				up(&nv_sem);
				return -EFAULT;
			}
		}
		break;
	default:
		pr_err("[%s] Unknow command!\n", __func__);
		ret = -ENOTTY;
		break;
	}
out:
	up(&nv_sem);
	return (long)ret;
}

#ifdef CONFIG_COMPAT
static long nve_compat_ioctl(struct file *file, u_int cmd, u_long arg)
{
	return nve_ioctl(file, cmd,
			 (unsigned long)compat_ptr((unsigned int)arg));
}
#endif

static const struct file_operations nve_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = nve_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nve_compat_ioctl,
#endif
	.open = nve_open,
	.release = nve_close,
};

static int __init init_nve(void)
{
	int error = 0;

	/* semaphore initial */
	sema_init(&nv_sem, 1);

	/* alloc nve_ramdisk_struct */
	nve_data_struct = kzalloc(sizeof(struct nve_ramdisk_struct),
		GFP_KERNEL);
	if (!nve_data_struct) {
		pr_err("[%s] line %d failed kzalloc.\n",
		       __func__, __LINE__);
		return -NVE_ERROR_INIT;
	}

	/* alloc ramdisk */
	nve_data_struct->nve_current_ramdisk =
		(struct nv_partittion_struct *)kzalloc(
			(size_t)NVE_PARTITION_SIZE, GFP_KERNEL);
	if (!nve_data_struct->nve_current_ramdisk) {
		pr_err("[%s]failed to allocate current ramdisk buffer in line %d.\n",
		       __func__, __LINE__);
		error = -NVE_ERROR_INIT;
		goto failed_free_driver_data;
	}

	nve_data_struct->nve_update_ramdisk =
		(struct nv_partittion_struct *)kzalloc(
			(size_t)NVE_PARTITION_SIZE, GFP_KERNEL);
	if (!nve_data_struct->nve_update_ramdisk) {
		pr_err("[%s]failed to allocate update ramdisk buffer in line %d.\n",
		       __func__, __LINE__);
		error = -NVE_ERROR_INIT;
		goto failed_free_current_ramdisk;
	}

	nve_data_struct->nve_store_ramdisk =
		(struct nv_partittion_struct *)kzalloc(
			(size_t)NVE_PARTITION_SIZE, GFP_KERNEL);
	if (!nve_data_struct->nve_store_ramdisk) {
		pr_err("[%s]failed to allocate store ramdisk buffer in line %d.\n",
		       __func__, __LINE__);
		error = -NVE_ERROR_INIT;
		goto failed_free_update_ramdisk;
	}

	/* register a device in kernel, return the number of major device */
	nve_data_struct->nve_major_number = register_chrdev(0,
			"nve", &nve_fops);
	if (nve_data_struct->nve_major_number < 0) {
		pr_err("[NVE]Can't allocate major number for Non-Volatile memory Extension device.\n");
		error = -NVE_ERROR_INIT;
		goto failed_free_store_ramdisk;
	}

	/* register a class, make sure that mdev can create device node in
	 * "/dev"
	 */
	nve_class = class_create(THIS_MODULE, "nve");
	if (IS_ERR(nve_class)) {
		pr_err("[NVE]Error creating nve class.\n");
		unregister_chrdev(
		(unsigned int)nve_data_struct->nve_major_number, "nve");
		error = -NVE_ERROR_INIT;
		goto failed_free_store_ramdisk;
	}

	mutex_init(&nve_set_mutex);

	return 0;

failed_free_store_ramdisk:
	kfree(nve_data_struct->nve_store_ramdisk);
	nve_data_struct->nve_store_ramdisk = NULL;
failed_free_update_ramdisk:
	kfree(nve_data_struct->nve_update_ramdisk);
	nve_data_struct->nve_update_ramdisk = NULL;
failed_free_current_ramdisk:
	kfree(nve_data_struct->nve_current_ramdisk);
	nve_data_struct->nve_current_ramdisk = NULL;
failed_free_driver_data:
	kfree(nve_data_struct);
	nve_data_struct = NULL;
	return error;
}

static void __exit cleanup_nve(void)
{
	device_destroy(nve_class, MKDEV(dev_major_num, 0));
	class_destroy(nve_class);
	if (nve_data_struct) {
		unregister_chrdev(
		(unsigned int)nve_data_struct->nve_major_number, "nve");
		kfree(nve_data_struct->nve_store_ramdisk);
		nve_data_struct->nve_store_ramdisk = NULL;
		kfree(nve_data_struct->nve_update_ramdisk);
		nve_data_struct->nve_update_ramdisk = NULL;
		kfree(nve_data_struct->nve_current_ramdisk);
		nve_data_struct->nve_current_ramdisk = NULL;
		kfree(nve_data_struct);
		nve_data_struct = NULL;
		kfree(nve_block_device_name);
		nve_block_device_name = NULL;
	}
}

static int nve_setup(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	struct device *nve_dev = NULL;

	mutex_lock(&nve_set_mutex);

	if (hisi_nv_setup_ok == 1) {
		pr_err("[%s]has been done.\n", __func__);
		mutex_unlock(&nve_set_mutex);
		return 0;
	}

	pr_info("[%s]nve setup begin.\n", __func__);
	/* get param by cmdline */
	if (!val) {
		mutex_unlock(&nve_set_mutex);
		return -EINVAL;
	}

	nve_block_device_name = kzalloc(strlen(val) + 1, GFP_KERNEL);
	if (!nve_block_device_name) {
		pr_err("[%s] line:%d failed to kzalloc.\n",
		       __func__, __LINE__);
		mutex_unlock(&nve_set_mutex);
		return -ENOMEM;
	}

	memcpy(nve_block_device_name, val, strlen(val) + 1);
	pr_info("[%s] device name = %s\n", __func__,
		nve_block_device_name);

	/* read nve partition to ramdisk */
	ret = read_nve_to_ramdisk();
	if (ret < 0) {
		pr_err("[%s] read nve to ramdisk failed!\n", __func__);
		goto out;
	}

	dev_major_num = (unsigned int)(nve_data_struct->nve_major_number);
	/* create a device node for application */
	nve_dev = device_create(nve_class, NULL, MKDEV(dev_major_num, 0), NULL,
				"nve0");
	if (IS_ERR(nve_dev)) {
		pr_err("[%s]failed to create nve device in line %d.\n",
		       __func__, __LINE__);
		ret = NVE_ERROR;
		goto out;
	}

	hisi_nv_setup_ok = 1;
	mutex_unlock(&nve_set_mutex);
	pr_info("[%s]nve setup end.\n", __func__);

	return ret;

out:
	kfree(nve_block_device_name);
	nve_block_device_name = NULL;
	mutex_unlock(&nve_set_mutex);

	return ret;
}

/*
 * Function name:nve_later_setup.
 * Discription: when initialized is 0, try to find path by call flash_find_ptn_s
 * if found, then try to read data from device
 * return value:
 *          @ 0 - success.
 *          @ -1- failure.
 */
static int nve_later_setup(void)
{
	char nve_path[NV_DEVICE_MAX_PATH_LEN + 1] = { 0 };
	int ret;

	ret = flash_find_ptn_s("nvme", nve_path, NV_DEVICE_MAX_PATH_LEN);
	if (ret != 0) {
		pr_err("[%s] find_ptn_s fail\n", __func__);
		return ret;
	}
	pr_err("[%s] nve_path is : %s.\n", __func__, nve_path);

	ret = nve_setup((const char *)nve_path, NULL);
	return ret;
}

module_param_call(nve, nve_setup, NULL, NULL, 0660);

module_init(init_nve);
module_exit(cleanup_nve);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("hisi-nve");
MODULE_DESCRIPTION("Direct character-device access to NVE devices");
