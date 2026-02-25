/*
 * Kernel traps
 */

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
extern unsigned int vectors[];	// in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
unsigned int ticks;

const char* exceptions[] = {
	"Division Error",			// 0
	"Debug",				// 1
	"Non-maskable Interrupt",		// 2
	"Breakpoint",				// 3
	"Overflow",				// 4
	"Bound Range Exceeded",			// 5
	"Invalid Opcode",			// 6
	"Device Not Available",			// 7
	"Double Fault",				// 8
	"Coprocessor Segment Overrun",		// 9
	"Invalid TSS",				// 10
	"Segment Not Present",			// 11
	"Stack-Segment Fault",			// 12
	"General Protection Fault",		// 13
	"Page Fault",				// 14
	"Reserved",				// 15
	"x87 Floating-Point Exception",		// 16
	"Alignment Check",			// 17
	"Machine Check",			// 18
	"SIMD Floating-Point Exception",	// 19
	"Virtualization Exception",		// 20
	"Control Protection Exception",		// 21
	"Reserved",				// 22
	"Reserved",				// 23
	"Reserved",				// 24
	"Reserved",				// 25
	"Reserved",				// 26
	"Reserved",				// 27
	"Hypervisor Injection Exception",	// 28
	"VMM Communication Exception",		// 29
	"Security Exception",			// 30
	"Reserved"				// 31
};

void trap_init(void){
	int i;

	for (i = 0; i < 256; i++)
		SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);

	SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
	initlock(&tickslock, "time");
}

void idt_init(void){
	lidt(idt, sizeof(idt));
}

void interrupt_eoi(void){
	outb(0x20, 0x20);	// End-Of-Interrupt command
}

void trap(struct trapframe *tf){
	struct proc *p = myproc();

	if (tf->trapno == T_SYSCALL){	// Syscall
		if (myproc()->killed)
			exit(0);
		myproc()->tf = tf;
		syscall();
		if (myproc()->killed)
			exit(0);
		dosignal();
		return;
	}

	switch(tf->trapno){
	case T_IRQ0 + IRQ_TIMER:	// Timer increment
		// Up the timer
		acquire(&tickslock);
		ticks++;
		wakeup(&ticks);
		release(&tickslock);
		interrupt_eoi();
		if (myproc() != 0 && (tf->cs&3) == DPL_USER){
			if (p->alarmticks > 0){
				p->alarmticks--;

				if (p->alarmticks == 0){
					if ((p->sigpending = 0))
						p->sigpending |= SIGALRM;
				}
			}
		}
		break;
	case T_IRQ0 + IRQ_IDE:
		ide_interrupt();
		interrupt_eoi();
		break;
	case T_IRQ0 + IRQ_IDE+1:
		// Bochs generates spurious IDE1 interrupts.
		break;
	case T_IRQ0 + IRQ_KBD:
		kbd_interrupt();
		interrupt_eoi();
		break;
	case T_IRQ0 + IRQ_COM1:
		uart_interrupt();
		interrupt_eoi();
		break;
	case T_IRQ0 + 7:
	case T_IRQ0 + IRQ_SPURIOUS:
		printk("warning: spurious interrupt at 0x%x:0x%x\n", tf->cs, tf->eip);
		interrupt_eoi();
		break;

	default:
		if (myproc() == 0 || (tf->cs&3) == 0){
			// In kernel, it must be our mistake.
			printk("unexpected trap %d [eip=0x%x][cr2=0x%x]\n", tf->trapno, tf->eip, rcr2());
			panic("%s", exceptions[tf->trapno]);
		}
		// In user space, assume process misbehaved.
		if (tf->eip != -1) {
			debug("%s[%d] %s[%d] ip:0x%x sp:0x%x\n", myproc()->name, myproc()->pid, exceptions[myproc()->tf->trapno], myproc()->tf->trapno, tf->eip, myproc()->tf->esp);
		} else	// we are returning
			exit(myproc()->exitstatus);
		myproc()->sigpending |= (1 << SIGSEGV);
		dosignal();
	}

	// Force process exit if it has been killed and is in user space.
	// (If it is still executing in the kernel, let it keep running
	// until it gets to the regular system call return.)
	if (myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
		exit(0);

	// Force process to give up CPU on clock tick.
	// If interrupts were on while locks held, would need to check nlock.
	if (myproc() && myproc()->state == RUNNING)
		yield();

	// Check if the process has been killed since we yielded
	if (myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
		exit(0);
}
