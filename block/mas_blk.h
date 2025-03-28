/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2016-2019. All rights reserved.
 * Description: block header
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

#ifndef MAS_BLK_INTERNAL_H
#define MAS_BLK_INTERNAL_H
#include <linux/hisi/rdr_hisi_platform.h>

#define IO_FROM_SUBMIT_BIO_MAGIC 0x4C
#define IO_FROM_BLK_EXEC 0x4D

#define BLK_LLD_INIT_MAGIC 0x7A
#define IO_BUF_LEN 10

#define TEN_MS 10
#define ONE_HUNDRED 100

#define REQ_FG_META (REQ_FG | REQ_META)

/* Used to device sqr sqw bw */
#define BLK_MID_SQW_BW  300
#define BLK_MAX_SQW_BW  500
#define BLK_MAX_SQR_BW  0
#define DEVICE_CAPACITY_128_G  0x10000000  /* 128G/512 */

enum blk_lld_feature_bits {
	__BLK_LLD_DUMP_SUPPORT = 0,
	__BLK_LLD_LATENCY_SUPPORT,
	__BLK_LLD_FLUSH_REDUCE_SUPPORT,
	__BLK_LLD_BUSYIDLE_SUPPORT,
	__BLK_LLD_IOSCHED_UFS_MQ,
	__BLK_LLD_IOSCHED_UFS_HW_IDLE,
	__BLK_LLD_UFS_CP_EN,
	__BLK_LLD_UFS_ORDER_EN,
	__BLK_LLD_UFS_UNISTORE_EN,
	__BLK_LLD_IOSCHED_MMC_MQ,
};

#define BLK_LLD_DUMP_SUPPORT (1ULL << __BLK_LLD_DUMP_SUPPORT)
#define BLK_LLD_LATENCY_SUPPORT (1ULL << __BLK_LLD_LATENCY_SUPPORT)
#define BLK_LLD_FLUSH_REDUCE_SUPPORT (1ULL << __BLK_LLD_FLUSH_REDUCE_SUPPORT)
#define BLK_LLD_BUSYIDLE_SUPPORT (1ULL << __BLK_LLD_BUSYIDLE_SUPPORT)
#define BLK_LLD_IOSCHED_UFS_MQ (1ULL << __BLK_LLD_IOSCHED_UFS_MQ)
#define BLK_LLD_IOSCHED_UFS_HW_IDLE (1ULL << __BLK_LLD_IOSCHED_UFS_HW_IDLE)
#define BLK_LLD_UFS_CP_EN (1ULL << __BLK_LLD_UFS_CP_EN)
#define BLK_LLD_UFS_ORDER_EN (1ULL << __BLK_LLD_UFS_ORDER_EN)
#define BLK_LLD_UFS_UNISTORE_EN (1ULL << __BLK_LLD_UFS_UNISTORE_EN)
#define BLK_LLD_IOSCHED_MMC_MQ (1ULL << __BLK_LLD_IOSCHED_MMC_MQ)

#define BLK_QUEUE_DURATION_UNIT_NS 10000

enum blk_busyidle_nb_flag {
	BLK_BUSYIDLE_NB_NOT_JOIN_POLL = 0,
};

#define BLK_BUSYIDLE_NB_FLAG_NOT_JOIN_POLL                                    \
	(1 << BLK_BUSYIDLE_NB_NOT_JOIN_POLL)

enum iosched_strategy {
	IOSCHED_NONE = 0,
	IOSCHED_MAS_UFS_MQ,
	IOSCHED_MAS_MMC_MQ,
};

struct blk_mq_ctx;
struct blk_mq_alloc_data;

/*
 * IO Scheduler Operation Function Pointer
 */
struct blk_queue_ops {
	enum iosched_strategy io_scheduler_strategy;
	/* MQ Scheduler Init */
	int (*mq_iosched_init_fn)(struct request_queue *);
	/* MQ Scheduler Deinit */
	void (*mq_iosched_exit_fn)(struct request_queue *);
	/* Prepare for MQ reqeust allocation */
	void (*mq_req_alloc_prep_fn)(struct blk_mq_alloc_data *data,
		unsigned long ioflag, bool fs_submit);
	/* Request init in MQ */
	void (*mq_req_init_fn)(
		struct request_queue *, struct blk_mq_ctx *, struct request *);
	/* Request complete in MQ */
	void (*mq_req_complete_fn)(
		struct request *, struct request_queue *, bool);
	/* Request deinit in MQ */
	void (*mq_req_deinit_fn)(struct request *);
	/* Request insert to MQ */
	void (*mq_req_insert_fn)(struct request *req, struct request_queue *);
	/* Request requeue in MQ */
	void (*mq_req_requeue_fn)(struct request *, struct request_queue *);
	/* Request timeout process in MQ */
	void (*mq_req_timeout_fn)(struct request *);
	/* Release the CTX in MQ */
	void (*mq_ctx_put_fn)(void);
	/* Get hctx object by request */
	void (*mq_hctx_get_by_req_fn)(
		struct request *, struct blk_mq_hw_ctx **);
	/* Get tag in MQ */
	unsigned int (*mq_tag_get_fn)(struct blk_mq_alloc_data *data);
	/* Release tag in MQ */
	int (*mq_tag_put_fn)(
		struct blk_mq_hw_ctx *, unsigned int, struct request *);
	/* wakeup all threads waiting tag */
	int (*mq_tag_wakeup_all_fn)(struct blk_mq_tags *);
	/* Execute queue function directly in MQ */
	void (*mq_exec_queue_fn)(struct request_queue *);
	/* Run hw queue in MQ */
	void (*mq_run_hw_queue_fn)(struct request_queue *);
	/* Run requeue in MQ */
	void (*mq_run_requeue_fn)(struct request_queue *);
	/* Enable poll */
	void (*blk_poll_enable_fn)(bool *);
	void (*blk_write_throttle_fn)(struct request_queue *, bool);
	/* Dump ioscheduler status */
	void (*blk_status_dump_fn)(struct request_queue *,
		enum blk_dump_scene);
	/* Dump req status in ioscheduler */
	void (*blk_req_dump_fn)(struct request_queue *, enum blk_dump_scene);
	void *scheduler_priv;
	char iosched_name[ELV_NAME_MAX];
};

/*
 * MQ tagset Operation Function Pointer
 */
struct blk_tagset_ops {
	/* MQ tagset init */
	struct blk_mq_tags *(*tagset_init_tags_fn)(unsigned int total_tags,
		unsigned int reserved_tags, unsigned int high_prio_tags,
		int node, int alloc_policy);
	/* MQ tagset free */
	void (*tagset_free_tags_fn)(struct blk_mq_tags *);
	/* iterate all the busy tag in whole tagset */
	void (*tagset_all_tag_busy_iter_fn)(
		struct blk_mq_tags *, busy_tag_iter_fn *, const void *);
	void (*tagset_tag_busy_iter_fn)(
		struct blk_mq_hw_ctx *, busy_iter_fn *, void *);
	void (*tagset_power_off_proc_fn)(struct blk_dev_lld *);
	struct blk_queue_ops *queue_ops;
};

struct bio_delay_stage_config {
	char *stage_name;
	void (*function)(struct bio *bio);
};

struct req_delay_stage_config {
	char *stage_name;
	void (*function)(struct request *req);
};

static inline void mas_blk_rdr_panic(char *msg)
{
}

#ifdef CONFIG_MAS_BLK
void mas_blk_bio_clone_fast(
	struct bio *bio, struct bio *bio_src);
void mas_blk_set_data_flag(struct request_queue *q,
	struct blk_mq_alloc_data *data, struct bio *bio, unsigned int op);
void mas_blk_bio_queue_split(
	struct request_queue *q, struct bio **bio, struct bio *split);
bool mas_blk_bio_merge_allow(const struct request *rq,
	const struct bio *bio);
void mas_blk_bio_merge_done(
	const struct request_queue *q, const struct request *req,
	const struct request *next);
int mas_blk_account_io_completion(
	const struct request *req, unsigned int bytes);
struct request_queue *mas_blk_get_queue_by_lld(
	struct blk_dev_lld *lld);
int mas_blk_cust_ioctl(struct block_device *bdev,
	struct blkdev_cmd __user *arg);
int mas_blk_generic_make_request_check(struct bio *bio);
void mas_blk_generic_make_request(const struct bio *bio);
void mas_blk_start_plug(struct blk_plug *plug);
void mas_blk_flush_plug_list(
	const struct blk_plug *plug, bool from_schedule);
void mas_blk_bio_endio(const struct bio *bio);
void mas_blk_bio_free(const struct bio *bio);

void mas_blk_request_init_from_bio(
	struct request *req, struct bio *bio);
void mas_blk_insert_cloned_request(
	const struct request_queue *q, const struct request *rq);
void mas_blk_request_execute_nowait(
	const struct request_queue *q, const struct request *rq,
	rq_end_io_fn *done);
void mas_blk_mq_rq_ctx_init(
	struct request_queue *q, struct blk_mq_ctx *ctx, struct request *rq);
void mas_blk_mq_request_start(const struct request *rq);
void mas_blk_request_start(const struct request *req);
void mas_blk_requeue_request(const struct request *rq);
void mas_blk_request_update(
	const struct request *req, blk_status_t error, unsigned int nr_bytes);
void mas_blk_mq_request_free(struct request *rq);
void mas_blk_request_put(struct request *req);

void mas_blk_check_partition_done(
	struct gendisk *disk, bool has_part_tbl);
void mas_blk_queue_register(
	struct request_queue *q, const struct gendisk *disk);
void mas_blk_mq_init_allocated_queue(struct request_queue *q);
void mas_blk_sq_init_allocated_queue(const struct request_queue *q);
void mas_blk_allocated_queue_init(struct request_queue *q);
void mas_blk_mq_free_queue(const struct request_queue *q);
void mas_blk_cleanup_queue(struct request_queue *q);

void mas_blk_mq_allocated_tagset_init(struct blk_mq_tag_set *set);
void blk_add_queue_tags(
	struct blk_queue_tag *tags, struct request_queue *q);
void mas_blk_allocated_tags_init(struct blk_queue_tag *tags);

int mas_blk_dev_init(void);

struct blk_dev_lld *mas_blk_get_lld(struct request_queue *q);

void mas_blk_busyidle_check_bio(
	const struct request_queue *q, struct bio *bio);
bool mas_blk_busyidle_check_request_bio(
	const struct request_queue *q, const struct request *rq);
void mas_blk_busyidle_check_execute_request(
	const struct request_queue *q, struct request *rq,
	rq_end_io_fn *done);
void mas_blk_busyidle_check_bio_endio(struct bio *bio);
int mas_blk_busyidle_state_init(struct blk_idle_state *blk_idle);

void mas_blk_latency_init(void);
void mas_blk_queue_latency_init(struct request_queue *q);
void mas_blk_latency_bio_check(
	struct bio *bio, enum bio_process_stage_enum bio_stage);
void mas_blk_latency_req_check(
	struct request *req, enum req_process_stage_enum req_stage);
void mas_blk_latency_for_merge(
	const struct request *req, const struct request *next);
void mas_blk_queue_async_flush_init(struct request_queue *q);
void mas_blk_flush_update(const struct request *req, int error);
bool mas_blk_flush_async_dispatch(
	struct request_queue *q, const struct bio *bio);
void mas_blk_dump_register_queue(struct request_queue *q);
void mas_blk_dump_unregister_queue(struct request_queue *q);
void mas_blk_dump_bio(const struct bio *bio, enum blk_dump_scene s);
void mas_blk_dump_request(struct request *rq, enum blk_dump_scene s);
void mas_blk_dump_queue_status(
	const struct request_queue *q,  enum blk_dump_scene s);
int mas_blk_dump_queue_status2(
	struct request_queue *q, char *buf, int len);
int mas_blk_dump_lld_status(
	struct blk_dev_lld *lld, char *buf, int len);
void mas_blk_dump_init(void);

void mas_blk_queue_usr_ctrl_set(struct request_queue *q);
#ifdef CONFIG_MAS_MQ_USING_CP
static inline bool mas_blk_bio_is_cp(const struct bio *bio)
{
	return (bio->bi_opf & REQ_FG_META) ? true : false;
}

#endif

void blk_req_set_make_req_nr(struct request *req);

#define NO_EXTRA_MSG NULL

ssize_t queue_var_store(
	unsigned long *var, const char *page, size_t count);

static inline void mas_blk_dump_fs(const struct bio *bio)
{
	if (bio->dump_fs)
		bio->dump_fs();
}
#endif /* CONFIG_MAS_BLK */

#endif /* MAS_BLK_INTERNAL_H */
