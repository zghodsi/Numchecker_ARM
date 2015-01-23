/*
 * Copyright 2012 Calxeda, Inc.
 *
 * Based on arch/arm/plat-mxc/cpuidle.c:
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/cpuidle.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <asm/cpuidle.h>
#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>

extern void highbank_set_cpu_jump(int cpu, void *jump_addr);
extern void *scu_base_addr;

static struct cpuidle_device __percpu *calxeda_idle_cpuidle_devices;

static noinline void calxeda_idle_restore(void)
{
	set_cr(get_cr() | CR_C);
	set_auxcr(get_auxcr() | 0x40);
	scu_power_mode(scu_base_addr, SCU_PM_NORMAL);
}

static int calxeda_idle_finish(unsigned long val)
{
	/* Already flushed cache, but do it again as the outer cache functions
	 * dirty the cache with spinlocks */
	flush_cache_all();

	set_auxcr(get_auxcr() & ~0x40);
	set_cr(get_cr() & ~CR_C);

	scu_power_mode(scu_base_addr, SCU_PM_DORMANT);

	cpu_do_idle();

	/* Restore things if we didn't enter power-gating */
	calxeda_idle_restore();
	return 1;
}

static int calxeda_pwrdown_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	highbank_set_cpu_jump(smp_processor_id(), cpu_resume);
	cpu_suspend(0, calxeda_idle_finish);
	return index;
}

static void calxeda_idle_cpuidle_devices_uninit(void)
{
	int i;
	struct cpuidle_device *dev;

	for_each_possible_cpu(i) {
		dev = per_cpu_ptr(calxeda_idle_cpuidle_devices, i);
		cpuidle_unregister_device(dev);
	}

	free_percpu(calxeda_idle_cpuidle_devices);
}

static struct cpuidle_driver calxeda_idle_driver = {
	.name = "calxeda_idle",
	.en_core_tk_irqen = 1,
	.states = {
		ARM_CPUIDLE_WFI_STATE,
		{
			.name = "PG",
			.desc = "Power Gate",
			.flags = CPUIDLE_FLAG_TIME_VALID,
			.exit_latency = 30,
			.power_usage = 50,
			.target_residency = 200,
			.enter = calxeda_pwrdown_idle,
		},
	},
	.state_count = 2,
};

static int __init calxeda_cpuidle_init(void)
{
	int cpu_id;
	int ret;
	struct cpuidle_device *dev;
	struct cpuidle_driver *drv = &calxeda_idle_driver;

	if (!of_machine_is_compatible("calxeda,highbank"))
		return -ENODEV;

	ret = cpuidle_register_driver(drv);
	if (ret)
		return ret;

	calxeda_idle_cpuidle_devices = alloc_percpu(struct cpuidle_device);
	if (calxeda_idle_cpuidle_devices == NULL) {
		ret = -ENOMEM;
		goto unregister_drv;
	}

	/* initialize state data for each cpuidle_device */
	for_each_possible_cpu(cpu_id) {
		dev = per_cpu_ptr(calxeda_idle_cpuidle_devices, cpu_id);
		dev->cpu = cpu_id;
		dev->state_count = drv->state_count;

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("Failed to register cpu %u, error: %d\n",
			       cpu_id, ret);
			goto uninit;
		}
	}

	return 0;

uninit:
	calxeda_idle_cpuidle_devices_uninit();
unregister_drv:
	cpuidle_unregister_driver(drv);
	return ret;
}
module_init(calxeda_cpuidle_init);
