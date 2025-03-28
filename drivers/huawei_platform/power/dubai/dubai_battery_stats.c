#include <linux/power_supply.h>

#include <chipset_common/dubai/dubai_plat.h>
#include <huawei_platform/power/battery_voltage.h>
#include <linux/power/hisi/coul/coul_drv.h>

#define BATTERY_POWER_SUPPLY_NAME	"Battery"

#ifdef CONFIG_COUL_DRV
static int dubai_get_capacity_rm(void)
{
	return coul_drv_battery_rm() * hw_battery_get_series_num();
}
#else
static int dubai_get_capacity_rm(void)
{
	// dubai platform driver will cal "capacity * full" instead.
	return -1;
}
#endif

static struct dubai_battery_stats_ops batt_ops = {
	.psy_name = BATTERY_POWER_SUPPLY_NAME,
	.charge_full_amp = 1,
	.get_capacity_rm = dubai_get_capacity_rm,
};

void dubai_hisi_battery_stats_init(void)
{
	dubai_register_module_ops(DUBAI_MODULE_BATTERY, &batt_ops);
}

void dubai_hisi_battery_stats_exit(void)
{
	dubai_unregister_module_ops(DUBAI_MODULE_BATTERY);
}
