/*
 * signal.c - signal handling code.
 */

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
void dosignal(void){
	struct proc *p = myproc();
	struct trapframe *tf = p->tf;
	uint32_t sp, handler, saved_tf_addr, restorer;

	for (int signo = 1; signo < NSIG; signo++){
		if (!(p->sigpending & (1 << signo)))
			continue;

		if (signo != SIGKILL && (p->sigmask & (1 << signo)))
			continue;

		p->sigpending &= ~(1 << signo);
		handler = p->sighandlers[signo];

		if (signo == SIGKILL || handler == (uint32_t)SIG_DFL){
			p->killed = 1;
			return;
		}

		if (handler == (uint32_t)SIG_IGN)
			continue;

		restorer = p->sigrestorers[signo];
		if (restorer == 0){
			// Would return to garbage. Kill cleanly instead.
			p->sigpending = 0;
			p->killed = 1;
			return;
		}

		p->sigmask |= (1 << signo);
		sp = tf->esp;

		// Save trapframe so rt_sigreturn can restore it
		sp -= sizeof(struct trapframe);
		if(copyout(p->pgdir, sp, (char*)tf, sizeof(struct trapframe)) < 0)
			goto deliver_failed;

		saved_tf_addr = sp;

		// sigrestorer
		sp -= 4;
		if(copyout(p->pgdir, sp, (char*)&p->sigrestorers[signo], 4) < 0)
			goto deliver_failed;

		// signo
		sp -= 4;
		if(copyout(p->pgdir, sp, (char*)&signo, 4) < 0)
			goto deliver_failed;

		// Fake return address
		sp -= 4;
		if(copyout(p->pgdir, sp, (char*)&p->sigrestorers[signo], 4) < 0)
			goto deliver_failed;

		p->saved_trapframe_sp = saved_tf_addr;

		tf->esp = sp;
		tf->eip = handler;
		return;
	}
	return;

deliver_failed:
	// Signal cannot be written and the userspace stack is wildly broken.
	// The process must be immidiately terminated to stop it from destroying
	// things even further.
//	p->sigpending = 0;
	p->killed = 1;
}
