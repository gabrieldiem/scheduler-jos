/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

#define MAX_ENV_HISTORY 3000

struct env_info {
	envid_t envid;
	uint32_t yield_counter_at_creation;
	uint32_t yield_counter_at_destruction;
	uint32_t env_runs;
	uint32_t env_sched_runs_total;
} typedef env_info_t;

struct sched_info {
	uint32_t yield_counter;
	env_info_t env_history[MAX_ENV_HISTORY];
	uint32_t env_history_size;
	/* head and tail to make a circular array and overwrite old elements */
	uint32_t env_history_head;
	uint32_t env_history_tail;
	uint32_t last_env_destroyed_index;
} typedef sched_info_t;

sched_info_t scheduler_info;

// This function does not return.
void sched_yield(void) __attribute__((noreturn));

void sched_init();
void sched_add_env_data_to_history(struct Env *env);

#endif  // !JOS_KERN_SCHED_H
