// my test
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	uint32_t priority = sys_get_currenv_priority();
	cprintf("Mi prioridad es: %d\n", priority);

	int id = 0;
	if ((id = fork()) < 0)
		panic("fork: %e", id);
	if (id == 0) {
		priority = sys_get_currenv_priority();
		cprintf("Soy el hijo y mi prioridad es: %d\n", priority);
		for (int i = 0; i < 500; i++) {
			priority = sys_get_currenv_priority();
			cprintf("Soy el hijo y mi prioridad es: %d\n", priority);
			// sys_yield();
		}
	} else {
		for (int i = 0; i < 500; i++) {
			priority = sys_get_currenv_priority();
			cprintf("Soy el padre y mi prioridad es: %d\n", priority);
			// sys_yield();
		}
	}
}
