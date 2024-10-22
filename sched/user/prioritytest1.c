// my test
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	uint32_t priority = sys_get_currenv_priority();
	cprintf("Mi prioridad es: %d\n", priority);

	sys_decrease_currenv_priority(50000);
	priority = sys_get_currenv_priority();
	cprintf("Mi nueva prioridad es: %d\n", priority);

	sys_decrease_currenv_priority(-2);
	priority = sys_get_currenv_priority();
	cprintf("Intenté 'subir la prioridad' y mi nueva prioridad es: %d\n",
	        priority);

	int id = 0;
	if ((id = fork()) < 0)
		panic("fork: %e", id);
	if (id == 0) {
		priority = sys_get_currenv_priority();
		cprintf("Soy el hijo y mi prioridad es: %d\n", priority);
	} else {
		cprintf("Soy el padre y mi prioridad es: %d\n", priority);
	}
}
