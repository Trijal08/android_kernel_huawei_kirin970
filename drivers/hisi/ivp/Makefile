ccflags-$(CONFIG_IVP_IVPV160) += -Idrivers/hisi/ivp/ivpv160

obj-$(CONFIG_IVP_SMMU)        += ivp_smmu.o
obj-$(CONFIG_DEV_IVP)        += ivp_ipc.o
obj-$(CONFIG_DEV_IVP)        += ivp_main.o
obj-$(CONFIG_DEV_IVP)        += ivp_ioctl.o
obj-$(CONFIG_DEV_IVP)        += ivp_manager.o
obj-$(CONFIG_DEV_IVP)        += ivp_map.o

obj-$(CONFIG_IVP_IVPV160)     += ivpv160/

ifneq (, $(filter eng, $(TARGET_BUILD_VARIANT)))
    EXTRA_CFLAGS += -DENG_BUILDMODE
endif

ifeq ($(CONFIG_IVP_IVPV160), y)
subdir-ccflags-y += -DIVP_FAMA_SUPPORT
endif

subdir-ccflags-y += -I$(srctree)/drivers/hisi/tzdriver/ \
                    -I$(srctree)/lib/libc_sec/securec_v2/include/ \
                    -I$(srctree)/drivers/hisi/load_image
