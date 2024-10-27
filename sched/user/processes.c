#include <inc/lib.h>

void get_priority_and_yield_in_loop(int executions){
	for(int i = 0; i < executions; i++){
		int priority = sys_get_currenv_priority();
		cprintf("Env %08x is paused with priority %d\n", sys_getenvid(),priority);
		sys_yield();
	}
}

void
umain(int argc, char **argv)
{
int cont = 0;
	int id = 0;
	if ((id = fork()) < 0)
		panic("fork: %e", id);
	if (id == 0) {
		int id2 = 0;
		int priority = sys_get_currenv_priority();
		
		if ((id2 = fork()) < 0)
			panic("fork: %e", id);
		if (id2 == 0) {
			int priority = sys_get_currenv_priority();
			cprintf("Env %08x starts with priority %d\n", sys_getenvid(), priority);

			get_priority_and_yield_in_loop(40);
		
		} else{
			cprintf("Env %08x starts with priority %d\n", sys_getenvid(), priority);
			get_priority_and_yield_in_loop(40);
		}

	} else {
		int priority = sys_get_currenv_priority();
        cprintf("Env %08x starts with priority %d\n", sys_getenvid(), priority);
		get_priority_and_yield_in_loop(10);
	}
}