/*
 * client_hash_auth.c
 *
 * function for CA code hash auth
 *
 * Copyright (c) 2012-2020 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "client_hash_auth.h"
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/rwsem.h>

#include <linux/mm.h>
#include <linux/dcache.h>
#include <linux/mm_types.h>
#include <linux/highmem.h>
#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/sched/mm.h>

#include "tc_ns_log.h"
#include "auth_base_impl.h"

#ifdef CONFIG_ANDROID_HIDL

/* hash for : /vendor/bin/hw/vendor.huawei.hardware.libteec@3.0-service1000 */
static unsigned char g_hidl_path_hash[SHA256_DIGEST_LENTH] = {
	0x0a, 0xce, 0x6c, 0x1a,
	0x6a, 0x54, 0x0f, 0xbd,
	0xe3, 0xc8, 0xf8, 0x5e,
	0xe1, 0xf3, 0x0a, 0x3a,
	0x43, 0x6a, 0x45, 0x1c,
	0x26, 0x7e, 0xa7, 0x08,
	0xfa, 0x01, 0x3b, 0x25,
	0x38, 0x51, 0x9e, 0xcb,
};

static unsigned char g_hidl_calc_hash[SHA256_DIGEST_LENTH];
static bool g_hidl_hash_calced = false;
DEFINE_MUTEX(g_hidl_calc_lock);

static int check_hidl_code_hash()
{
	unsigned char digest[SHA256_DIGEST_LENTH] = {0};

	if (!g_hidl_hash_calced)
		return CHECK_ACCESS_SUCC;

	if (calc_task_hash(digest, (uint32_t)SHA256_DIGEST_LENTH,
		current)) {
		tloge("calc task hash failed\n");
		return CHECK_ACCESS_FAIL;
	}

	if (memcmp(digest, g_hidl_calc_hash, SHA256_DIGEST_LENTH)) {
		tloge("compare libteec hidl hash error\n");
		return CHECK_ACCESS_FAIL;
	}

	return CHECK_ACCESS_SUCC;
}

static int calc_hidl_process_hash(void)
{
	mutex_lock(&g_hidl_calc_lock);

	if (g_hidl_hash_calced) {
		mutex_unlock(&g_hidl_calc_lock);
		return CHECK_ACCESS_SUCC;
	}

	if (memset_s((void *)g_hidl_calc_hash,
		sizeof(g_hidl_calc_hash), 0x00,
		sizeof(g_hidl_calc_hash))) {
		tloge("memset failed\n");
		mutex_unlock(&g_hidl_calc_lock);
		return CHECK_ACCESS_FAIL;
	}

	g_hidl_hash_calced = !calc_task_hash(g_hidl_calc_hash,
		(uint32_t)SHA256_DIGEST_LENTH, current);
	if (!g_hidl_hash_calced) {
		tloge("calc libteec hidl hash failed\n");
		mutex_unlock(&g_hidl_calc_lock);
		return CHECK_ACCESS_FAIL;
	}

	mutex_unlock(&g_hidl_calc_lock);
	return CHECK_ACCESS_SUCC;
}


static void dump_hash_auth(unsigned char *hash_buf)
{

	tlogd("{0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, ",
		*(hash_buf + 0), *(hash_buf + 1), *(hash_buf + 2), *(hash_buf + 3),
		*(hash_buf + 4), *(hash_buf + 5), *(hash_buf + 6), *(hash_buf + 7));
	tlogd("0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, ",
		*(hash_buf + 8), *(hash_buf + 9), *(hash_buf + 10), *(hash_buf + 11),
		*(hash_buf + 12), *(hash_buf + 13), *(hash_buf + 14),
		*(hash_buf + 15));
	tlogd("0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X,  ",
		*(hash_buf + 16), *(hash_buf + 17), *(hash_buf + 18),
		*(hash_buf + 19), *(hash_buf + 20), *(hash_buf + 21),
		*(hash_buf + 22), *(hash_buf + 23));
	tlogd("0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X} ",
		*(hash_buf + 24), *(hash_buf + 25), *(hash_buf + 26),
		*(hash_buf + 27), *(hash_buf + 28), *(hash_buf + 29),
		*(hash_buf + 30), *(hash_buf + 31));
}

static void spoof_hash_auth(unsigned char *hash_buf, char *pkg_name)
{
	unsigned char gatekeeper_hash[32] = {0x13, 0xF2, 0x99, 0xDE, 0x59, 0x3A, 0x96, 0x2F,
					   0xA9, 0x00, 0x42, 0x97, 0x88, 0xB0, 0x62, 0xDE,
					   0xB7, 0x12, 0xDC, 0xD1, 0x7A, 0x4A, 0x2D, 0xA6,
					   0xB4, 0xB2, 0x5A, 0x0C, 0x36, 0x7D, 0x74, 0x97};

	unsigned char fingerprint_hash[32] = {0x2F, 0x46, 0xE4, 0x4C, 0x57, 0xB7, 0x38, 0xE4,
					    0xFE, 0x5E, 0xC6, 0x3B, 0x05, 0x49, 0x50, 0x47,
					    0x9F, 0xED, 0xF6, 0x63, 0x15, 0x5D, 0xC9, 0xA4,
					    0x1C, 0xC2, 0x55, 0x17, 0xD3, 0x1A, 0xA5, 0x04};

	unsigned char keymaster_hash[32] = {0xDE, 0xE1, 0x2D, 0x2F, 0xA7, 0xCA, 0xC4, 0x25,
					    0x92, 0x38, 0x25, 0x7E, 0xDA, 0xBD, 0x8D, 0x2A,
					    0x7D, 0x9D, 0x49, 0x52, 0x7A, 0x64, 0x82, 0xE6,
					    0x67, 0xA6, 0x99, 0x0E, 0x79, 0x0E, 0x40, 0x5D};
	
	unsigned char graphics_allocator_hash[32] = {0x7B, 0x77, 0x87, 0xF4, 0xE6, 0x69, 0xB0, 0xED,
					    0xDB, 0x7D, 0xD1, 0x94, 0x50, 0x8A, 0xAE, 0x7C,
					    0xB0, 0xC3, 0x89, 0xAE, 0xC7, 0x72, 0x16, 0x88,
					    0xE7, 0xFF, 0x67, 0x2F, 0x86, 0xE9, 0x05, 0x93};
	
	unsigned char graphics_composer_hash[32] = {0x04, 0x6B, 0x09, 0x58, 0x9E, 0x3D, 0xED, 0x51,
					    0x61, 0xE0, 0x0A, 0xF5, 0xAF, 0x47, 0x00, 0xC7,
					    0x3F, 0x52, 0x91, 0x5E, 0xCA, 0x8C, 0xDB, 0xE0,
					    0xF9, 0xB4, 0x3C, 0x92, 0xDD, 0x66, 0x4C, 0x6A};
	
	unsigned char omx_hash[32] = {0xC6, 0xB0, 0x28, 0x8D, 0x85, 0xBE, 0x61, 0x7A,
					    0x27, 0xC1, 0x0D, 0xD3, 0x44, 0x9A, 0xAE, 0x0A,
					    0x33, 0x79, 0xB3, 0xA9, 0xAD, 0x8B, 0x4D, 0xBB,
					    0x8F, 0x0B, 0x4E, 0x21, 0x62, 0x98, 0x15, 0x79};
	
	/* Hardcode hash - same of the native_packages */
	if (!strncmp(pkg_name, "/vendor/bin/hw/vendor.huawei.hardware.biometrics.fingerprint@2.2-service", 72)){
		tlogd("Spoof now %s process\n",pkg_name);
		memcpy(hash_buf, fingerprint_hash, MAX_SHA_256_SZ);
	}
	
	if (!strncmp(pkg_name, "/vendor/bin/hw/android.hardware.gatekeeper@1.0-service", 54)) {
		tlogd("Spoof now %s process\n",pkg_name);
		memcpy(hash_buf, gatekeeper_hash, MAX_SHA_256_SZ);
	}
	
	if (!strncmp(pkg_name, "/vendor/bin/hw/android.hardware.keymaster@3.0-service", 53)) {
		tlogd("Spoof now %s process\n",pkg_name);
		memcpy(hash_buf, keymaster_hash, MAX_SHA_256_SZ);
	}
	
	if (!strncmp(pkg_name, "/vendor/bin/hw/android.hardware.graphics.allocator@2.0-service", 62)) {
		tlogd("Spoof now %s process\n",pkg_name);
		memcpy(hash_buf, graphics_allocator_hash, MAX_SHA_256_SZ);
	}
	
	if (!strncmp(pkg_name, "/vendor/bin/hw/android.hardware.graphics.composer@2.2-service", 61)) {
		tlogd("Spoof now %s process\n",pkg_name);
		memcpy(hash_buf, graphics_composer_hash, MAX_SHA_256_SZ);
	}
	
	if (!strncmp(pkg_name, "/vendor/bin/hw/android.hardware.media.omx@1.0-service", 53)) {
		tlogd("Spoof now %s process\n",pkg_name);
		memcpy(hash_buf, omx_hash, MAX_SHA_256_SZ);
	}
}

/* 
 To change path - please change this function and spoof path hash
*/
static int check_hidl_path_access(void)
{
	unsigned char digest[SHA256_DIGEST_LENTH] = {0};

	if (calc_path_hash(true, digest, SHA256_DIGEST_LENTH)) {
		tloge("calc path hash failed\n");
		return CHECK_PATH_HASH_FAIL;
	}
	
	if (memcmp(digest, g_hidl_path_hash, SHA256_DIGEST_LENTH)) {
		tlogd("process is not libteec hidl service, keep going\n");
		return ENTER_BYPASS_CHANNEL;
	}

	if (check_proc_selinux_access(current,
		"u:r:hal_libteec_default:s0")) {
		tloge("check libteec hidl service seclabel failed\n");
		return CHECK_SECLABEL_FAIL;
	}

	return CHECK_ACCESS_SUCC;
}

static int check_proc_state(bool is_hidl_srvc, struct task_struct **hidl_struct,
	const struct tc_ns_client_context *context)
{
	bool check_value = false;

	if (is_hidl_srvc) {
		rcu_read_lock();
		*hidl_struct = pid_task(find_vpid(context->calling_pid),
			PIDTYPE_PID);
		check_value = !*hidl_struct ||
			(*hidl_struct)->state == TASK_DEAD;
		if (check_value) {
			tloge("task is dead\n");
			rcu_read_unlock();
			return -EFAULT;
		}

		get_task_struct(*hidl_struct);
		rcu_read_unlock();
		return EOK;
	}

	check_value = (context->calling_pid && current->mm);
	if (check_value) {
		tloge("non hidl service, non-zero callingpid, reject\n");
		return -EFAULT;
	}

	return EOK;
}

static int get_hidl_client_task(struct tc_ns_client_context *context,
	bool is_hidl_srvc, struct task_struct **cur_struct)
{
	int ret;
	struct task_struct *hidl_struct = NULL;

	ret = check_proc_state(is_hidl_srvc, &hidl_struct, context);
	if (ret)
		return ret;

	if (hidl_struct)
		*cur_struct = hidl_struct;
	else
		*cur_struct = current;

	return EOK;
}

int check_hidl_access(void)
{
	int ret;

	if (!current->mm) {
		tlogd("kernel thread need not check\n");
		return ENTER_BYPASS_CHANNEL;
	}

	ret = check_hidl_path_access();
	if (ret != CHECK_ACCESS_SUCC)
		return ret;

	if (calc_hidl_process_hash() != CHECK_ACCESS_SUCC) {
		tloge("calc hidl process hash failed\n");
		return CHECK_ACCESS_FAIL;
	}

	if (check_hidl_code_hash() != CHECK_ACCESS_SUCC) {
		tloge("check hidl process hash failed\n");
		return CHECK_CODE_HASH_FAIL;
	}

	return CHECK_ACCESS_SUCC;
}
#endif

#define LIBTEEC_CODE_PAGE_SIZE 8
#define DEFAULT_TEXT_OFF 0
#define LIBTEEC_NAME_MAX_LEN 50
const char g_libso[KIND_OF_SO][LIBTEEC_NAME_MAX_LEN] = {
						"libteec_vendor.so",
						"libteec.huawei.so",
};

static int find_lib_code_area(struct mm_struct *mm,
	struct vm_area_struct **lib_code_area, int so_index)
{
	struct vm_area_struct *vma = NULL;
	bool is_valid_vma = false;
	bool is_so_exists = false;
	bool param_check = (!mm || !mm->mmap ||
		!lib_code_area || so_index >= KIND_OF_SO);

	if (param_check) {
		tloge("illegal input params\n");
		return -EFAULT;
	}
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		is_valid_vma = (vma->vm_file &&
			vma->vm_file->f_path.dentry &&
			vma->vm_file->f_path.dentry->d_name.name);
		if (is_valid_vma) {
			is_so_exists = !strcmp(g_libso[so_index],
				vma->vm_file->f_path.dentry->d_name.name);
			if (is_so_exists && (vma->vm_flags & VM_EXEC)) {
				*lib_code_area = vma;
				tlogd("so name is %s\n",
					vma->vm_file->f_path.dentry->d_name.name);
				return EOK;
			}
		}
	}
	return -EFAULT;
}

struct get_code_info {
	unsigned long code_start;
	unsigned long code_end;
	unsigned long code_size;
};
static int update_so_hash(struct mm_struct *mm,
	struct task_struct *cur_struct, struct shash_desc *shash, int so_index)
{
	struct vm_area_struct *vma = NULL;
	int rc = -EFAULT;
	struct get_code_info code_info;
	unsigned long in_size;
	struct page *ptr_page = NULL;
	void *ptr_base = NULL;

	if (find_lib_code_area(mm, &vma, so_index)) {
		tlogd("get lib code vma area failed\n");
		return -EFAULT;
	}

	code_info.code_start = vma->vm_start;
	code_info.code_end = vma->vm_end;
	code_info.code_size = code_info.code_end - code_info.code_start;

	while (code_info.code_start < code_info.code_end) {
		// Get a handle of the page we want to read
		rc = get_user_pages_remote(cur_struct, mm, code_info.code_start,
			1, FOLL_FORCE, &ptr_page, NULL, NULL);
		if (rc != 1) {
			tloge("get user pages locked error[0x%x]\n", rc);
			rc = -EFAULT;
			break;
		}

		ptr_base = kmap_atomic(ptr_page);
		if (!ptr_base) {
			rc = -EFAULT;
			put_page(ptr_page);
			break;
		}
		in_size = (code_info.code_size > PAGE_SIZE) ? PAGE_SIZE : code_info.code_size;

		rc = crypto_shash_update(shash, ptr_base, in_size);
		if (rc) {
			kunmap_atomic(ptr_base);
			put_page(ptr_page);
			break;
		}
		kunmap_atomic(ptr_base);
		put_page(ptr_page);
		code_info.code_start += in_size;
		code_info.code_size = code_info.code_end - code_info.code_start;
	}
	return rc;
}

/* Calculate the SHA256 library digest */
static int calc_task_so_hash(unsigned char *digest, uint32_t dig_len,
	struct task_struct *cur_struct, int so_index)
{
	struct mm_struct *mm = NULL;
	int rc;
	size_t size;
	size_t shash_size;
	struct sdesc *desc = NULL;

	if (!digest || dig_len != SHA256_DIGEST_LENTH) {
		tloge("tee hash: digest is NULL\n");
		return -EFAULT;
	}

	shash_size = crypto_shash_descsize(get_shash_handle());
	size = sizeof(desc->shash) + shash_size;
	if (size < sizeof(desc->shash) || size < shash_size) {
		tloge("size overflow\n");
		return -ENOMEM;
	}

	desc = kzalloc(size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR((unsigned long)(uintptr_t)desc)) {
		tloge("alloc desc failed\n");
		return -ENOMEM;
	}

	desc->shash.tfm = get_shash_handle();
	desc->shash.flags = 0;
	if (crypto_shash_init(&desc->shash)) {
		kfree(desc);
		return -EFAULT;
	}

	mm = get_task_mm(cur_struct);
	if (!mm) {
		tloge("so does not have mm struct\n");
		if (memset_s(digest, MAX_SHA_256_SZ, 0, dig_len))
			tloge("memset digest failed\n");
		kfree(desc);
		return -EFAULT;
	}

	down_read(&mm->mmap_sem);
	rc = update_so_hash(mm, cur_struct, &desc->shash, so_index);
	up_read(&mm->mmap_sem);
	mmput(mm);
	if (!rc)
		rc = crypto_shash_final(&desc->shash, digest);

	kfree(desc);
	return rc;
}

static int proc_calc_hash(char *pkg_name, uint8_t kernel_api, struct tc_ns_session *session,
	struct task_struct *cur_struct)
{
	int rc, i;
	int so_found = 0;

	mutex_crypto_hash_lock();
	if (kernel_api == TEE_REQ_FROM_USER_MODE) {
		for (i = 0; so_found < NUM_OF_SO && i < KIND_OF_SO; i++) {
			rc = calc_task_so_hash(session->auth_hash_buf + MAX_SHA_256_SZ * so_found,
				(uint32_t)SHA256_DIGEST_LENTH, cur_struct, i);
			if (!rc)
				so_found++;
		}
		if (so_found != NUM_OF_SO)
			tlogd("so library found: %d\n", so_found);
	} else {
		tlogd("request from kernel\n");
	}


	rc = calc_task_hash(session->auth_hash_buf + MAX_SHA_256_SZ * NUM_OF_SO,
		(uint32_t)SHA256_DIGEST_LENTH, cur_struct);
	if (rc) {
		mutex_crypto_hash_unlock();
		tloge("tee calc ca hash failed\n");
		return -EFAULT;
	}

	tlogd("Auth hash buff before spoof:\n");
	dump_hash_auth(session->auth_hash_buf + MAX_SHA_256_SZ * NUM_OF_SO);
	
	spoof_hash_auth(session->auth_hash_buf + MAX_SHA_256_SZ * NUM_OF_SO, pkg_name);
	
	tlogd("Auth hash buff after spoof:\n");
	dump_hash_auth(session->auth_hash_buf + MAX_SHA_256_SZ * NUM_OF_SO);
	
	mutex_crypto_hash_unlock();
	return EOK;
}

int calc_client_auth_hash(struct tc_ns_dev_file *dev_file,
	struct tc_ns_client_context *context, struct tc_ns_session *session)
{
	int ret;
	struct task_struct *cur_struct = NULL;
	bool check = false;
	
#ifdef CONFIG_ANDROID_HIDL
	bool is_hidl_srvc = false;
#endif
	check = (!dev_file || !context || !session);
	if (check) {
		tloge("bad params\n");
		return -EFAULT;
	}

	if (tee_init_shash_handle("sha256")) {
		tloge("init code hash error\n");
		return -EFAULT;
	}

#ifdef CONFIG_ANDROID_HIDL
	ret = check_hidl_access();
	if (ret != CHECK_ACCESS_SUCC) {
		if (ret != ENTER_BYPASS_CHANNEL) {
			tloge("libteec hidl service may be exploited ret 0x%x\n", ret);
			return -EACCES;
		}
		/* vendor ca take this branch */
	} else {
		is_hidl_srvc = true;
	}

	ret = get_hidl_client_task(context, is_hidl_srvc, &cur_struct);
	if (ret)
		return -EFAULT;
#else
	cur_struct = current;
#endif

	ret = proc_calc_hash(dev_file->pkg_name, dev_file->kernel_api, session, cur_struct);
#ifdef CONFIG_ANDROID_HIDL
	if (is_hidl_srvc)
		put_task_struct(cur_struct);
#endif
	return ret;
}
