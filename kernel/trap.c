#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <traps.h>
#include <spinlock.h>

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];	// in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
	int i;

	for(i = 0; i < 256; i++)
		SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
	SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

	initlock(&tickslock, "time");
}

void
idtinit(void)
{
	lidt(idt, sizeof(idt));
}

void
dosignal(int signo)
{
	struct proc *p = myproc();
	struct trapframe *tf = p->tf;

	if(signo < 1 || signo >= NSIG)
		return;

	uint handler = p->sighandlers[signo];
	p->signal = 0;

	if(signo != SIGKILL && (p->sigmask & (1 << signo))) {
		// Signal is blocked, keep it pending
		return;
	}

	if(signo == SIGKILL || handler == 0) { // no exceptions
		p->killed = 1;
		return;
	}

	if(handler == 1) {
		return;
	}

	uint sp = tf->esp;

	sp -= 4;
	*(uint*)sp = tf->eip;
	sp -= 4;
	*(uint*)sp = signo;
	sp -= 4;
	*(uint*)sp = 0xFFFFFFFF;

	tf->esp = sp;
	tf->eip = handler;
}

void
trap(struct trapframe *tf)
{
	struct proc *p = myproc();
	if(tf->trapno == T_SYSCALL){
		if(myproc()->killed)
			exit(0);
		myproc()->tf = tf;
		syscall();
		if(myproc()->killed)
			exit(0);
		return;
	}

	switch(tf->trapno){
	case T_IRQ0 + IRQ_TIMER:
		if(cpunum() == 0){
			acquire(&tickslock);
			ticks++;
			wakeup(&ticks);
			release(&tickslock);
		}
		lapiceoi();
		if(myproc() != 0 && (tf->cs&3) == DPL_USER) {
			if(p->alarmticks > 0) {
				p->alarmticks--;

				if(p->alarmticks == 0) {
					if(p->signal == 0) {
						p->signal = SIGALRM;
					}
				}
			}
		}
		break;
	case T_IRQ0 + IRQ_IDE:
		ideintr();
		lapiceoi();
		break;
	case T_IRQ0 + IRQ_IDE+1:
		// Bochs generates spurious IDE1 interrupts.
		break;
	case T_IRQ0 + IRQ_KBD:
		kbdintr();
		lapiceoi();
		break;
	case T_IRQ0 + IRQ_COM1:
		uartintr();
		lapiceoi();
		break;
	case T_IRQ0 + 7:
	case T_IRQ0 + IRQ_SPURIOUS:
		printk("cpu%d: spurious interrupt at %x:%x\n",
						cpunum(), tf->cs, tf->eip);
		lapiceoi();
		break;

	default:
		if(myproc() == 0 || (tf->cs&3) == 0){
			// In kernel, it must be our mistake.
			printk("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
							tf->trapno, cpunum(), tf->eip, rcr2());
			panic("trap");
		}
		// In user space, assume process misbehaved.
		if (tf->eip != -1){
			printk("%s: fatal trap from PID %d (%d)\n", myproc()->name, myproc()->pid, tf->trapno);
		}
		myproc()->signal = SIGSEGV;
	}

	// Force process exit if it has been killed and is in user space.
	// (If it is still executing in the kernel, let it keep running
	// until it gets to the regular system call return.)
	if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
		exit(0);

	if(myproc() && myproc()->signal > 0 && (tf->cs&3) == DPL_USER) {
		dosignal(myproc()->signal);
	}

	// Force process to give up CPU on clock tick.
	// If interrupts were on while locks held, would need to check nlock.
	if(myproc() && myproc()->state == RUNNING &&
		 tf->trapno == T_IRQ0+IRQ_TIMER)
		yield();

	// Check if the process has been killed since we yielded
	if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
		exit(0);
}
