#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/sched.h>

#define BOOST_THRESHOLD 45
#define YIELD_COUNTER_DECREASE_PRIORITY 5

void sched_halt(void);

/*
 * Initialize scheduler statistics.
 */
void
sched_init()
{
	scheduler_info.yield_counter = 0;
	scheduler_info.env_history_size = 0;
	scheduler_info.env_history_head = 0;
	scheduler_info.env_history_tail = 0;
	scheduler_info.last_env_destroyed_index = 0;
}

/*
 * Adds data from an env to the scheduler history, 
 * updating the history tail and head indexes as necessary.
 */
void
sched_add_env_data_to_history(struct Env *env)
{
	scheduler_info.last_env_destroyed_index = ENVX(env->env_id);

	if (scheduler_info.env_history_size == MAX_ENV_HISTORY) {
		scheduler_info.env_history_tail =
		        (scheduler_info.env_history_tail + 1) % MAX_ENV_HISTORY;
	} else {
		scheduler_info.env_history_size++;
	}

	env_info_t *history_entry =
	        &scheduler_info.env_history[scheduler_info.env_history_head];
	scheduler_info.env_history_head =
	        (scheduler_info.env_history_head + 1) % MAX_ENV_HISTORY;

	history_entry->envid = env->env_id;
	history_entry->env_runs = env->env_runs;
	history_entry->env_sched_runs_total = env->env_sched_runs_total;
	history_entry->yield_counter_at_creation =
	        env->env_yield_counter_at_creation;
	history_entry->yield_counter_at_destruction = scheduler_info.yield_counter;
}

/*
 * Finds the first env of a given type in the envs array.
 */
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

/*
 * Returns the index of the last env that was saved to the history.
 */
static int
get_latest_env_index_saved_to_history()
{
	if (scheduler_info.last_env_destroyed_index == NENV - 1) {
		scheduler_info.last_env_destroyed_index = 0;
	}

	return scheduler_info.last_env_destroyed_index;
}

/*
 * Returns true if the scheduler is running for the first time.
 */
static bool
is_first_run()
{
	return curenv == NULL && scheduler_info.env_history_size == 0;
}

/*
 * Returns true if the last process that was running has just finished.
 */
static bool
did_process_just_finished()
{
	return curenv == NULL && scheduler_info.env_history_size > 0;
}

/*
 * Finds the next executable environment in a round-robin scheduler, starting from 
 * the current environment or from the next environment after a process has terminated,
 * and if there are none, returns the current environment if it is still running.
 */
static struct Env *
round_robin_find_next()
{
	// If no env is running currently, return the first runnable env
	if (is_first_run()) {
		return find_first_env_of_type(0, NENV - 1, ENV_RUNNABLE);
	}

	int start_index = 0;

	if (did_process_just_finished()) {
		start_index = get_latest_env_index_saved_to_history() + 1;
	} else {
		start_index = ENVX(curenv->env_id) + 1;
	}

	start_index = MIN(start_index, NENV - 1);

	struct Env *next =
	        find_first_env_of_type(start_index, NENV - 1, ENV_RUNNABLE);

	if (next == NULL) {
		next = find_first_env_of_type(0, start_index, ENV_RUNNABLE);
	}

	// There is not a runnable process in envs nor curenv is alive
	if (next == NULL && curenv == NULL) {
		return NULL;
	}

	if (next == NULL && curenv->env_status == ENV_RUNNING) {
		next = curenv;
	}

	return next;
}

/*
 * Finds the first environment of a specific type within a range of indexes, 
 * saves the first environment found of each priority in an array, and returns 
 * the environment with the highest priority if found.
 */
static struct Env *
find_first_env_of_type_of_highest_priority_and_scan(int start_index,
                                                    int end_index,
                                                    int env_type,
                                                    struct Env **first_env_by_priority)
{
	struct Env *env = NULL;

	for (int i = start_index; i <= end_index; i++) {
		struct Env *i_env = &envs[i];
		if (i_env->env_status != env_type) {
			continue;
		}

		// Save scanned values from all priorities just the first time
		if (first_env_by_priority[i_env->env_priority] == NULL) {
			first_env_by_priority[i_env->env_priority] = i_env;
		}

		if (i_env->env_priority == HIGHEST_PRIORITY) {
			env = i_env;
			break;
		}
	}

	return env;
}

/*
 * Finds the first runnable environment of the highest priority in the envs array.
 */
static struct Env *
find_first_env_of_type_of_highest_priority(int start_index, int env_type)
{
	struct Env *env_selected = NULL;
	struct Env *first_env_by_priority[LOWEST_PRIORITY + 1] = { NULL };

	env_selected = find_first_env_of_type_of_highest_priority_and_scan(
	        start_index, NENV - 1, env_type, first_env_by_priority);

	if (env_selected != NULL) {
		return env_selected;
	}

	env_selected = find_first_env_of_type_of_highest_priority_and_scan(
	        0, start_index, env_type, first_env_by_priority);

	if (env_selected != NULL) {
		return env_selected;
	}

	/*
	 *  If there was no HIGHEST_PRIORITY env runnable in the whole envs
	 * array, iterate through the references saved throughout the iteration
	 * to return the highest priority first encountered runnable env
	 */
	for (int i = HIGHEST_PRIORITY; i <= LOWEST_PRIORITY; i++) {
		if (first_env_by_priority[i] != NULL) {
			env_selected = first_env_by_priority[i];
			break;
		}
	}

	return env_selected;
}

/*
 * Finds the next executable environment in a priority scheduler, starting from
 * the current environment or from the next environment after a process has terminated,
 * and if there are none, returns the current environment if it is still running.
 */
static struct Env *
priority_sched_find_next()
{
	// If no env is running currently, return the first runnable env of highest priority
	if (is_first_run()) {
		return find_first_env_of_type_of_highest_priority(0, ENV_RUNNABLE);
	}

	int start_index = 0;

	if (did_process_just_finished()) {
		start_index = get_latest_env_index_saved_to_history() + 1;
	} else {
		start_index = ENVX(curenv->env_id) + 1;
	}

	start_index = MIN(start_index, NENV - 1);

	struct Env *next =
	        find_first_env_of_type_of_highest_priority(start_index,
	                                                   ENV_RUNNABLE);

	// There is not a runnable process in envs nor curenv is alive
	if (next == NULL && curenv == NULL) {
		return NULL;
	}

	if (next == NULL && curenv->env_status == ENV_RUNNING) {
		next = curenv;
	}

	return next;
}

/*
 * Boosts the priority of all environments to the highest priority.
 */
static void
boost_all_envs()
{
	for (int i = 0; i < NENV; i++) {
		envs[i].env_priority = HIGHEST_PRIORITY;
		envs[i].env_sched_runs_current = 0;
	}
}

/*
 * Returns true if the priority of an environment should be decreased.
 */
static bool
should_decrease_priority(struct Env *env)
{
	return env->env_sched_runs_current % YIELD_COUNTER_DECREASE_PRIORITY == 0;
}

/*
 * Decreases the priority of an environment by one.
 */
static void
decrease_env_priority(struct Env *env)
{
	if (env->env_priority < LOWEST_PRIORITY) {
		env->env_priority++;
	}
}

/*
 * Prints information about an environment history entry.
 */
static void
sched_history_entry_show_info(env_info_t *history_entry)
{
	cprintf("Environment with ENVID: %012x started with the scheduler "
	        "yield counter at %lu and finished at %lu. It run %lu "
	        "sched_yield "
	        "cycles and %lu env_run cycles.\n",
	        history_entry->envid,
	        history_entry->yield_counter_at_creation,
	        history_entry->yield_counter_at_destruction,
	        history_entry->env_sched_runs_total,
	        history_entry->env_runs);
}

/*
 * Prints information about the scheduler.
 */
static void
sched_show_info()
{
	cprintf("\nExecution was yielded to the scheduler %d times.\n",
	        scheduler_info.yield_counter);

	if (scheduler_info.env_history_size == MAX_ENV_HISTORY) {
		cprintf("The env history was maxed out at %d entries so the "
		        "first entry is not the first ever executed "
		        "environment but rather the last %d-th environment.\n",
		        MAX_ENV_HISTORY,
		        MAX_ENV_HISTORY);
	}

	cprintf("Information about the last %d environments executed:\n",
	        scheduler_info.env_history_size);

	for (int i = 0; i < scheduler_info.env_history_size; i++) {
		int index =
		        (scheduler_info.env_history_tail + i) % MAX_ENV_HISTORY;
		sched_history_entry_show_info(&scheduler_info.env_history[index]);
	}

	cprintf("\n");
}

/*
 * Returns true if the scheduler should decrease the priority of an environment.
 */
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
		next_env->env_sched_runs_current++;
		next_env->env_sched_runs_total++;
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
		next_env->env_sched_runs_current++;
		next_env->env_sched_runs_total++;
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
