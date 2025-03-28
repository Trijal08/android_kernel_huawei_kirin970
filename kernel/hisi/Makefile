#
# Makefile for the linux kernel cgroup feature.
#

obj-$(CONFIG_CGROUP_IOLIMIT) += cgroup_io_limit.o
obj-$(CONFIG_HW_CGROUP_PIDS) += cgroup_pids.o

obj-$(CONFIG_HW_CGROUP_WORKINGSET) += cgroup_workingset.o \
				cgroup_workingset_backup.o \
				cgroup_workingset_collect.o \
				cgroup_workingset_preread.o

obj-y += cpu_netlink.o

obj-$(CONFIG_HW_VIP_THREAD) += proc_fork_nl.o hwcfs_common.o hwcfs_rwsem.o hwcfs_mutex.o hwcfs_fork.o
obj-$(CONFIG_HW_QOS_THREAD) += hwqos_common.o iaware_qos.o hwqos_rwsem.o hwqos_fork.o
obj-$(CONFIG_HW_IAWARE_THREAD_BOOST) += proc_comm_nl.o

obj-$(CONFIG_DETECT_HUAWEI_HUNG_TASK) += huawei_hung_task.o
obj-$(CONFIG_DETECT_HUAWEI_MMAP_SEM) += huawei_check_mmap_sem.o
