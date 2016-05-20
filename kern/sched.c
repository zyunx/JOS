#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.
    //cprintf("CPU %d will sched\n", cpunum());
        struct Env *e;
        if (curenv == NULL) {
            //cprintf("sched_yied: curenv NULL\n");
                for (e = envs; e < envs + NENV; e++)
                        if (e->env_status == ENV_RUNNABLE)
                                env_run(e);
        } else {
            //cprintf("sched_yied: curenv id %x\n", curenv->env_id);

                // sched others, and finally self
                for (e = curenv + 1; e < envs + NENV; e++)
                        if (e->env_status == ENV_RUNNABLE)
                                env_run(e);
                for (e = envs; e < curenv; e++)
                        if (e->env_status == ENV_RUNNABLE)
                                env_run(e); 
                if (e->env_status == ENV_RUNNABLE ||
                    e->env_status == ENV_RUNNING)
                {
                    env_run(e);
                }

        }
    //cprintf("CPU %d no env.\n", cpunum());
        // sched_halt never returns
	sched_halt();
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
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));
    //cprintf("CPU %d kern_pgdir loaded\n", cpunum());
	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);
    cprintf("CPU %d chage status CPU_HALTED\n", cpunum());

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();
    
	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

