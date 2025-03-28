/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * Description: unistore implement
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

#include "ufs_unistore_internal.h"
#include "ufs_unistore_read.h"
#include "ufs_unistore_write.h"
#include "ufs_vcmd_proc.h"
#include <linux/hisi/rdr_hisi_platform.h>

#include <scsi/scsi_device.h>
#include <scsi/ufs/ufs.h>
#include <scsi/scsi_host.h>
#include <asm/unaligned.h>

#include "ufshcd.h"

void ufshcd_prepare_utp_query_req_upiu_unistore(
	struct ufs_query *query, struct utp_upiu_req *ucd_req_ptr,
	u8 *descp, u16 len)
{
	struct ufshcd_data_move_buf *data_move_buf = NULL;

	if ((query->request.upiu_req.opcode ==
		UPIU_QUERY_OPCODE_VENDOR_WRITE) &&
		(query->request.upiu_req.idn == QUERY_VENDOR_DATA_MOVE)) {
		ucd_req_ptr->header.dword_2 =
			UPIU_HEADER_DWORD(0, 0, (len >> 8), (u8)len);

		data_move_buf =
			(struct ufshcd_data_move_buf *)(query->descriptor);
		ufschd_data_move_prepare_buf(data_move_buf->sdev,
			data_move_buf->data_move_info, descp);

		query->request.upiu_req.length = cpu_to_be16(DATA_MOVE_LENGTH);
		ucd_req_ptr->qr.length = cpu_to_be16(DATA_MOVE_LENGTH);
	}
}

static int ufshcd_unistore_bad_block_notify_register(
	struct scsi_device *sdev,
	void (*func)(struct Scsi_Host *host,
		struct stor_dev_bad_block_info *bad_block_info))
{
	struct scsi_host_template *hostt = NULL;
	struct Scsi_Host *host = NULL;

	if (!sdev || !func)
		return -EINVAL;

	host = sdev->host;
	if (!host)
		return -EINVAL;

	hostt = host->hostt;
	if (!hostt)
		return -EINVAL;

	hostt->dev_bad_block_notify = func;

	return 0;
}

void ufshcd_unistore_op_register(struct ufs_hba *hba)
{
	struct scsi_host_template *hostt = NULL;

	if (!hba || !hba->host)
		return;

	hostt = hba->host->hostt;
	if (!hostt)
		return;

	hostt->dev_pwron_info_sync = ufshcd_dev_pwron_info_sync;
	hostt->dev_stream_oob_info_fetch = ufshcd_stream_oob_info_fetch;
	hostt->dev_reset_ftl = ufshcd_dev_reset_ftl;
	hostt->dev_read_section = ufshcd_dev_read_section_size;
	hostt->dev_read_lrb_in_use = ufshcd_dev_read_lrb_in_use;
	hostt->dev_read_op_size = ufshcd_dev_read_op_size;
	hostt->dev_config_mapping_partition =
		ufshcd_dev_config_mapping_partition;
	hostt->dev_read_mapping_partition =
		ufshcd_dev_read_mapping_partition;
	hostt->dev_fs_sync_done = ufshcd_dev_fs_sync_done;
	hostt->dev_data_move = ufshcd_dev_data_move;
	hostt->dev_slc_mode_configuration = ufshcd_dev_slc_mode_configuration;
	hostt->dev_sync_read_verify = ufshcd_dev_sync_read_verify;
	hostt->dev_get_bad_block_info = ufshcd_dev_get_bad_block_info;
	hostt->dev_get_program_size = ufshcd_dev_get_program_size;
	hostt->dev_bad_block_notify_register =
		ufshcd_unistore_bad_block_notify_register;
}

static void ufshcd_enable_bad_block_occur(struct ufs_hba *hba)
{
	int err;

	err = ufshcd_enable_ee(hba, MASK_EE_BAD_BLOCK_OCCUR);
	if (err)
		dev_err(hba->dev, "%s: failed to enable exception event %d\n",
			__func__, err);

	return;
}

#define UNISTORE_MQ_QUEUE_DEPTH 32
static int ufshcd_unistore_update(struct Scsi_Host *host)
{
	int ret = 0;

	if ((host->mq_reserved_queue_depth != UNISTORE_MQ_QUEUE_DEPTH) ||
		(host->mq_high_prio_queue_depth != UNISTORE_MQ_QUEUE_DEPTH)) {
		host->mq_reserved_queue_depth = UNISTORE_MQ_QUEUE_DEPTH;
		host->mq_high_prio_queue_depth = UNISTORE_MQ_QUEUE_DEPTH;
		host->tag_set.reserved_tags = host->mq_reserved_queue_depth;
		host->tag_set.high_prio_tags = host->mq_high_prio_queue_depth;

		ret = mas_blk_mq_update_unistore_tags(&host->tag_set);
	}

	return ret;
}

static void ufshcd_unistore_set_sec_size(struct ufs_hba *hba)
{
	int ret;
	hba->host->mas_sec_size = 0x9000; /* 144M */

	ret = ufshcd_dev_read_section_size_hba(hba, &(hba->host->mas_sec_size));
	if (ret)
		dev_err(hba->dev, "%s: read sec size ret err %d\n", __func__, ret);
}

int ufshcd_unistore_init(struct ufs_hba *hba)
{
	int ret;
	struct Scsi_Host *host = hba->host;

	if (!host)
		return -EINVAL;

	if (!host->unistore_enable)
		return 0;

	ret = ufshcd_unistore_update(host);
	if (ret) {
		dev_err(hba->dev, "%s: ufshcd_unistore_update err %d\n",
			__func__, ret);

		return ret;
	}

	ret = ufshcd_dev_data_move_init();
	if (ret) {
		dev_err(hba->dev, "%s: ufshcd_dev_data_move_init err %d\n",
			__func__, ret);

		return ret;
	}

	ufshcd_enable_bad_block_occur(hba);

	ufshcd_unistore_set_sec_size(hba);

	return 0;
}

void ufshcd_unistore_done(struct ufs_hba *hba, struct scsi_cmnd *cmd,
	struct utp_upiu_rsp *ucd_rsp_ptr)
{
	if (ufshcd_rw_buffer_is_enabled(hba))
		ufshcd_dev_data_move_done(cmd, ucd_rsp_ptr);
}
