struct cpu_load_data {
	u64 prev_cpu_idle;
	u64 prev_cpu_wall;
	u64 prev_cpu_iowait;
	u64 prev_cpu_nice;
};

u64 get_cpu_idle_time_jiffy(unsigned int , u64 *);
u64 get_cpu_idle_time(unsigned int , u64 *);
u64 get_cpu_iowait_time(unsigned int , u64 *);
unsigned int calc_cur_load(unsigned int );
unsigned int report_load(void);
unsigned int get_lightest_loaded_cpu_n(void);
