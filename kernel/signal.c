#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <traps.h>

/*
 * preform any pending signals
 */	
void
dosignal(void)
{
	struct proc *p = myproc();
	struct trapframe *tf = p->tf;

	for(int signo = 1; signo < NSIG; signo++) {
		if(!(p->sigpending & (1 << signo)))
			continue;

		if(signo != SIGKILL && (p->sigmask & (1 << signo)))
			continue;

		p->sigpending &= ~(1 << signo);

		unsigned int handler = p->sighandlers[signo];

		if(signo == SIGKILL || handler == 0) {
			p->killed = 1;
			return;
		}

		if(handler == 1)  // SIG_IGN
			return;

		unsigned int sp = tf->esp;

		// Save trapframe so rt_sigreturn can restore it
		sp -= sizeof(struct trapframe);
		*(struct trapframe*)sp = *tf;
		unsigned int saved_tf_addr = sp;

		sp -= 4;
		*(unsigned int*)sp = p->sigrestorers[signo];

		sp -= 4;
		*(unsigned int*)sp = signo;

		sp -= 4;
		*(unsigned int*)sp = p->sigrestorers[signo];

		p->saved_trapframe_sp = saved_tf_addr;

		tf->esp = sp;
		tf->eip = handler;
		return;
	}
}

