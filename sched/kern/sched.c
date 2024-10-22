#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/sched.h>

#define BOOST_THRESHOLD 25
#define YIELD_COUNTER_DECREASE_PRIORITY 5

void sched_halt(void);

void
sched_init()
{
	scheduler_info.env_history_size = 0;
	scheduler_info.yield_counter = 0;
}

void
sched_add_env_data_to_history(struct Env *env)
{
	if (scheduler_info.env_history_size >= MAX_ENV_HISTORY) {
		return;
	}

	env_info_t *history_entry =
	        &scheduler_info.env_history[scheduler_info.env_history_size];
	scheduler_info.env_history_size++;

	history_entry->envid = env->env_id;
	history_entry->env_runs = env->env_runs;
	history_entry->env_sched_runs = env->env_sched_runs;
	history_entry->yield_counter_at_creation =
	        env->env_yield_counter_at_creation;
	history_entry->yield_counter_at_destruction = scheduler_info.yield_counter;
}

static struct Env *
find_first_env_of_type(int start_index, int end_index, int env_type)
{
	struct Env *env = NULL;

	for (int i = start_index; i <= end_index; i++) {
		if (envs[i].env_status == env_type) {
			env = &envs[i];
			break;
		}
	}

	return env;
}

static struct Env *
find_first_env_of_type_with_priority(int start_index,
                                     int end_index,
                                     int env_type,
                                     int priority)
{
	struct Env *env = NULL;

	for (int i = start_index; i <= end_index; i++) {
		if (envs[i].env_status == env_type &&
		    envs[i].env_priority == priority) {
			env = &envs[i];
			break;
		}
	}

	return env;
}

static struct Env *
round_robin_find_next()
{
	int start_index = 0;

	// If no env is running currently, return the first runnable env
	if (curenv == NULL) {
		return find_first_env_of_type(start_index, NENV - 1, ENV_RUNNABLE);
	}

	start_index = ENVX(curenv->env_id) + 1;
	struct Env *next =
	        find_first_env_of_type(start_index, NENV - 1, ENV_RUNNABLE);

	// If not found after curenv
	if (next == NULL) {
		next = find_first_env_of_type(0, start_index, ENV_RUNNABLE);
	}

	// If not found before curenv (because the logic is circular)
	// and curenv is still running
	if (next == NULL && curenv->env_status == ENV_RUNNING) {
		next = curenv;
	}

	return next;
}

static struct Env *
find_first_env_of_type_prioritized(int start_index, int end_index, int env_type)
{
	struct Env *env = NULL;

	for (int i = HIGHEST_PRIORITY; i < LOWEST_PRIORITY; i++) {
		env = find_first_env_of_type_with_priority(
		        start_index, end_index, env_type, i);
		if (env != NULL) {
			break;
		}
	}

	return env;
}

static struct Env *
priority_sched_find_next()
{
	int start_index = 0;

	// If no env is running currently, return the first runnable env
	if (curenv == NULL) {
		return find_first_env_of_type_prioritized(start_index,
		                                          NENV - 1,
		                                          ENV_RUNNABLE);
	}

	start_index = ENVX(curenv->env_id) + 1;
	struct Env *next = find_first_env_of_type_prioritized(start_index,
	                                                      NENV - 1,
	                                                      ENV_RUNNABLE);

	// If not found after curenv
	if (next == NULL) {
		next = find_first_env_of_type_prioritized(0,
		                                          start_index,
		                                          ENV_RUNNABLE);
	}

	// If not found before curenv (because the logic is circular)
	// and curenv is still running
	if (next == NULL && curenv->env_status == ENV_RUNNING) {
		next = curenv;
	}

	return next;
}

static void
boost_all_envs()
{
	for (int i = 0; i < NENV; i++) {
		envs[i].env_priority = HIGHEST_PRIORITY;
		envs[i].env_sched_runs = 0;
	}
}

static bool
should_decrease_priority(struct Env *env)
{
	return env->env_sched_runs % YIELD_COUNTER_DECREASE_PRIORITY == 0;
}

static void
decrease_env_priority(struct Env *env)
{
	if (env->env_priority < LOWEST_PRIORITY) {
		env->env_priority++;
	}
}

static void
sched_history_entry_show_info(env_info_t *history_entry)
{
	cprintf("Environment with ENVID: %012x started with the scheduler "
	        "yield counter at %d and finished at %d. It run %d sched_yield "
	        "cycles and %d env_run cycles.\n",
	        history_entry->envid,
	        history_entry->yield_counter_at_creation,
	        history_entry->yield_counter_at_destruction,
	        history_entry->env_runs,
	        history_entry->env_sched_runs);
}

static void
sched_show_info()
{
	cprintf("\nExecution was yielded to the scheduler %d times.\n",
	        scheduler_info.yield_counter);

	cprintf("Information about the last %d environments executed:\n",
	        scheduler_info.env_history_size);

	for (int i = 0; i < scheduler_info.env_history_size; i++) {
		sched_history_entry_show_info(&scheduler_info.env_history[i]);
	}

	cprintf("\n");
}

static bool
should_boost()
{
	return scheduler_info.yield_counter % BOOST_THRESHOLD == 0;
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	scheduler_info.yield_counter++;
	struct Env *next_env = NULL;

#ifdef SCHED_ROUND_ROBIN
	//    Implement simple round-robin scheduling.
	//
	//    Search through 'envs' for an ENV_RUNNABLE environment in
	//    circular fashion starting just after the env this CPU was
	//    last running. Switch to the first such environment found.
	//
	//    If no envs are runnable, but the environment previously
	//    running on this CPU is still ENV_RUNNING, it's okay to
	//    choose that environment.
	//
	//    Never choose an environment that's currently running on
	//    another CPU (env_status == ENV_RUNNING). If there are
	//    no runnable environments, simply drop through to the code
	//    below to halt the cpu.

	next_env = round_robin_find_next();

	if (next_env != NULL) {
		next_env->env_sched_runs++;
		env_run(next_env);
	}

#endif

#ifdef SCHED_PRIORITIES
	//  Implement simple priorities scheduling.
	//
	//  Environments now have a "priority" so it must be consider
	//  when the selection is performed.
	//
	//  Be careful to not fall in "starvation" such that only one
	//  environment is selected and run every time.

	if (should_boost()) {
		boost_all_envs();
	}

	next_env = priority_sched_find_next();

	if (next_env != NULL) {
		next_env->env_sched_runs++;
		if (should_decrease_priority(next_env)) {
			decrease_env_priority(next_env);
		}
		env_run(next_env);
	}
#endif

	// sched_halt never returns
	sched_halt();
	panic("Panic: sched_halt should have never returned"); /* mostly to placate the compiler */
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		sched_show_info();
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Once the scheduler has finishied it's work, print statistics
	// on performance. Your code here

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
