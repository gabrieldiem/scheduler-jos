#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int priority = sys_get_currenv_priority();
	cprintf("Env %08x starts with priority %d\n", sys_getenvid(), priority);

	for (int i = 0; i < 1000000000; i++) {
		if (i % 20000000 == 0) {
			int priority = sys_get_currenv_priority();
			cprintf("Env %08x is running with priority %d\n",
			        sys_getenvid(),
			        priority);
		}
	}
}