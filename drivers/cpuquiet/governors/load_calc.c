#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/cpu.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include "load_calc.h"

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

/* Consider IO as busy */
static bool io_is_busy = false;
static bool ignore_nice = true;

u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

u64 get_cpu_idle_time(unsigned int cpu, u64 *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

u64 get_cpu_iowait_time(unsigned int cpu, u64 *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

unsigned int calc_cur_load(unsigned int cpu)
{
	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	u64 cur_wall_time, cur_idle_time, cur_iowait_time;
	unsigned int idle_time, wall_time, iowait_time;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time);
	cur_iowait_time = get_cpu_iowait_time(cpu, &cur_wall_time);

	wall_time = (unsigned int) (cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int) (cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	iowait_time = (unsigned int) (cur_iowait_time - pcpu->prev_cpu_iowait);
	pcpu->prev_cpu_iowait = cur_iowait_time;

	if (ignore_nice) {
		u64 cur_nice;
		unsigned long cur_nice_jiffies;

		cur_nice = kcpustat_cpu(cpu).cpustat[CPUTIME_NICE] - pcpu->prev_cpu_nice;
		cur_nice_jiffies = (unsigned long) cputime64_to_jiffies64(cur_nice);

		pcpu->prev_cpu_nice = kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

		idle_time += jiffies_to_usecs(cur_nice_jiffies);
	}

	if (io_is_busy && idle_time >= iowait_time)
		idle_time -= iowait_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	return 100 * (wall_time - idle_time) / wall_time;
}

unsigned int report_load(void)
{
	int cpu;
	unsigned int cur_load = 0;
	
	for_each_online_cpu(cpu) {
		cur_load += calc_cur_load(cpu);

	}

	//printk("Total load is %d\n", cur_load);

	cur_load /= num_online_cpus();

	//printk("Per-CPU load is %d\n", cur_load);
	
	return cur_load;
}

unsigned int get_lightest_loaded_cpu_n(void)
{
	unsigned long min_avg_runnables = ULONG_MAX;
	unsigned int cpu = nr_cpu_ids;
	int i;

	for_each_online_cpu(i) {
		unsigned int nr_runnables = get_avg_nr_running(i);

		if (i > 0 && min_avg_runnables > nr_runnables) {
			cpu = i;
			min_avg_runnables = nr_runnables;
		}
	}

	return cpu;
}
