obj-y += charger/
obj-y += wireless/
obj-y += hardware_ic/

ifeq ($(HLTHERM_RUNTEST), true)
CFLAGS_KERNEL += -DCONFIG_HLTHERM_RUNTEST
endif

obj-$(CONFIG_HUAWEI_KSTATE) += hw_kstate/
obj-$(CONFIG_HUAWEI_FEATURE_PRINT_PID_NAME) += pid_socket/
obj-$(CONFIG_DOZE_FILTER) += wifi_filter/
obj-$(CONFIG_HUAWEI_DUBAI) += dubai/
