#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <x86.h>
#include <proc.h>
#include <spinlock.h>
#include <errno.h>
#include <stat.h>

struct {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
	initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpunum() {
	return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
	int apicid, i;

	if(readeflags()&FL_IF)
		panic("mycpu called with interrupts enabled\n");

	apicid = lapicid();
	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for (i = 0; i < ncpu; ++i) {
		if (cpus[i].apicid == apicid)
			return &cpus[i];
	}
	//panic("unknown apicid\n");
	return &cpus[0];
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
	struct cpu *c;
	struct proc *p;
	pushcli();
	c = mycpu();
	p = c->proc;
	popcli();
	return p;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
	struct proc *p;
	char *sp;

	acquire(&ptable.lock);

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->state == UNUSED)
			goto found;

	release(&ptable.lock);
	return 0;

found:
	p->state = EMBRYO;
	p->pid = nextpid++;
	p->uid = 0;
	p->gid = 0;
	p->alarmticks = 0;
	p->alarminterval = 0;
	p->sigmask = 0;
	p->umask = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
	p->tty = -1; // not a tty

	for(int i = 0; i < NGROUPS; i++)
		p->groups[i] = 0;
	for(int i = 0; i < NSIG; i++)
		p->sighandlers[i] = 0;

	p->sighandlers[SIGCHLD] = 1;

//	p->ttyflags = ECHO;

	release(&ptable.lock);

	// Allocate kernel stack.
	if((p->kstack = kalloc()) == 0){
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe*)sp;

	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(unsigned int*)sp = (unsigned int)trapret;

	sp -= sizeof *p->context;
	p->context = (struct context*)sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (unsigned int)forkret;

	return p;
}

// Set up first user process.
void
userinit(void)
{
	struct proc *p;
	extern char _binary_kernel_initcode_start[], _binary_kernel_initcode_size[];

	p = allocproc();

	p->uid = p->euid = p->suid = p->gid = p->egid = p->sgid = 0;
	p->pgrp = p->pid;

	initproc = p;
	if((p->pgdir = setupkvm()) == 0)
		panic("userinit: out of memory");
	inituvm(p->pgdir, _binary_kernel_initcode_start, (int)_binary_kernel_initcode_size);
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es = p->tf->ds;
	p->tf->ss = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp = PGSIZE;
	p->tf->eip = 0;	// beginning of initcode.S

	safestrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = namei("/");

	// this assignment to p->state lets other cores
	// run this process. the acquire forces the above
	// writes to be visible, and the lock is also needed
	// because the assignment might not be atomic.
	acquire(&ptable.lock);
	p->state = RUNNABLE;
	release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
	unsigned int sz;
	struct proc *curproc = myproc();

	sz = curproc->sz;
	if(n > 0){
		if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
			return -1;
	} else if(n < 0){
		if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
			return -1;
	}
	curproc->sz = sz;
	switchuvm(curproc);
	return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
	int i, pid;
	struct proc *np;
	struct proc *curproc = myproc();

	// Allocate process.
	if((np = allocproc()) == 0){
		return -1;
	}

	np->uid = curproc->uid;
	np->euid = curproc->euid;
	np->suid = curproc->suid;
	np->gid = curproc->gid;
	np->egid = curproc->egid;
	np->sgid = curproc->egid;
	np->pgrp = curproc->pgrp;
	np->sigpending = 0;
	np->sigmask = 0;
	np->saved_trapframe_sp = 0;

	/* i am pretty sure this is fine. I think, I hope.
	 * Kornshell doesn't seem to mind, though. Fixes
	 * an issue where it fails and dies cause of
	 * SIGSEGV.*/
	for(int i = 0; i < NSIG; i++) {
		np->sighandlers[i] = curproc->sighandlers[i];
		np->sigrestorers[i] = 0;
	}
	
	np->alarmticks = 0;
	np->alarminterval = 0;
	np->sigmask = 0;

	// Copy process state from proc.
	if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		return -1;
	}
	np->sz = curproc->sz;
	np->parent = curproc;
	*np->tf = *curproc->tf;

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	for(i = 0; i < NOFILE; i++)
		if(curproc->ofile[i])
			np->ofile[i] = filedup(curproc->ofile[i]);
	np->cwd = idup(curproc->cwd);

	safestrcpy(np->name, curproc->name, sizeof(curproc->name));

	pid = np->pid;

	acquire(&ptable.lock);

	np->state = RUNNABLE;

	release(&ptable.lock);

	return pid;
}

// Exit the current process.	Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.

void
exit(int status)
{
	struct proc *curproc = myproc();
	struct proc *p;
	struct proc *parent = curproc->parent;
	int fd;

	if(curproc == initproc) {
		if (strncmp(curproc->name, "initcode", sizeof(curproc->name)) == 0)
			panic("no init program found!");
		panic("attempted to kill init (exit %d)", curproc->exitstatus);
	}

	// Close all open files.
	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd]){
			fileclose(curproc->ofile[fd]);
			curproc->ofile[fd] = 0;
		}
	}

	begin_op();
	iput(curproc->cwd);
	end_op();
	curproc->cwd = 0;

	acquire(&ptable.lock);

	if(parent) {
		parent->sigpending |= (1 << SIGCHLD);
		if(parent->state == SLEEPING)
			parent->state = RUNNABLE;
	}

	if(parent && parent->sighandlers[SIGCHLD] == 1) {
		curproc->state = UNUSED;
		wakeup1(parent);
		release(&ptable.lock);
		sched();
		panic("unreachable");
	}

	// Set exit status before marking as ZOMBIE
	curproc->exitstatus = status;	// Store exit status
	// Parent might be sleeping in wait().
	wakeup1(curproc->parent);

	// Pass abandoned children to init.
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->parent == curproc){
			p->parent = initproc;
			if(p->state == ZOMBIE)
				wakeup1(initproc);
		}
	}

	// Jump into the scheduler, never to return.
	curproc->state = ZOMBIE;
	sched();
	panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
waitpid(int pid, int *status, int options)
{
	struct proc *p;
	int havekids;
	struct proc *curproc = myproc();
	acquire(&ptable.lock);
	for(;;){
		// Scan through table looking for zombie children.
		havekids = 0;

		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->parent != curproc)
				continue;

			havekids = 1;

			if(pid > 0 && p->pid != pid){
				continue;
			}

			if(p->state == ZOMBIE){
				int childpid = p->pid;

				if(status)
					*status = (p->exitstatus & 0xFF) << 8;

				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);

				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				curproc->sigpending &= ~(1 << SIGCHLD);

				release(&ptable.lock);
				return childpid;
			}
		}

		if(!havekids){
			release(&ptable.lock);
			return -1; // no children
		}

		if(options & 1){ // WNOHANG
			release(&ptable.lock);
			return 0;
		}
		sleep(curproc, &ptable.lock);
	}
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.	It loops, doing:
//	- choose a process to run
//	- swtch to start running that process
//	- eventually that process transfers control
//			via swtch back to the scheduler.
void
scheduler(void)
{
	struct proc *p;
	struct cpu *c = mycpu();
	c->proc = 0;
	
	for(;;){
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->state != RUNNABLE)
				continue;

			// Switch to chosen process.	It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;

			swtch(&(c->scheduler), p->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
		}
		release(&ptable.lock);

	}
}

// Enter scheduler.	Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
	int intena;
	struct proc *p = myproc();

	if(!holding(&ptable.lock))
		panic("sched ptable.lock");
	if(mycpu()->ncli != 1)
		panic("sched locks");
	if(p->state == RUNNING)
		panic("sched running");
	if(readeflags()&FL_IF)
		panic("sched interruptible");
	intena = mycpu()->intena;
	swtch(&p->context, mycpu()->scheduler);
	mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
	acquire(&ptable.lock);	//DOC: yieldlock
	myproc()->state = RUNNABLE;
	sched();
	release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.	"Return" to user space.
void
forkret(void)
{
	static int first = 1;
	// Still holding ptable.lock from scheduler.
	release(&ptable.lock);

	if (first) {
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot
		// be run from main().
		first = 0;
		iinit(ROOTDEV);
		initlog(ROOTDEV);
	}

	// Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
	struct proc *p = myproc();
	
	if(p == 0)
		panic("sleep");

	if(lk == 0)
		panic("sleep without lk");

	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if(lk != &ptable.lock){	//DOC: sleeplock0
		acquire(&ptable.lock);	//DOC: sleeplock1
		release(lk);
	}
	// Go to sleep.
	p->chan = chan;
	p->state = SLEEPING;

	sched();

	// Tidy up.
	p->chan = 0;

	// Reacquire original lock.
	if(lk != &ptable.lock){	//DOC: sleeplock2
		release(&ptable.lock);
		acquire(lk);
	}
}

void pause(void){
	acquire(&ptable.lock);
	sleep(myproc(), &ptable.lock);
	release(&ptable.lock);
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->state == SLEEPING && p->chan == chan)
			p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).

int
kill(int pid, int status)
{
	struct proc *p;
	struct proc *curproc = myproc();
	int found = 0;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid == pid){
			found = 1;

			// Permission check:
			// - Root (UID 0) can kill any process
			// - Users can only kill their own processes
			if(curproc->uid != 0 && p->uid != curproc->uid) {
				release(&ptable.lock);
				return 1;
			}

			// Proceed with kill operation
			if(p->state != ZOMBIE) {
				p->exitstatus = status;
			}

			// Wake process if sleeping
			if(p->state == SLEEPING)
				p->state = RUNNABLE;

			p->sigpending |= (1 << status);

			break;
		}
	}
	release(&ptable.lock);

	if(!found)
		return -ESRCH;
	return 0;
}

/*
 * find process from PID
 */
struct proc*
findproc(int pid, struct proc *parent)
{
	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(pid == -1){ // Child process
			if(p->parent == parent)
				return p;
		} else if(pid == 0){
			return myproc();
		} else {
			if(p->pid == pid)
				return p;
		}
	}
	return 0;
}

/*
 * syscall in proc.c because of ptable lock
 */
int
sys_rt_sigsuspend(void)
{
	unsigned int *mask;
	if(argptr(0, (void*)&mask, sizeof(*mask)) < 0)
		return -1;

	struct proc *p = myproc();
	unsigned int oldmask = p->sigmask;

	acquire(&ptable.lock);
	p->sigmask = *mask;

	while((p->sigpending & ~p->sigmask) == 0)
		sleep(p, &ptable.lock);

	p->sigmask = oldmask;
	release(&ptable.lock);

	dosignal();

	return -1;
}

int sys_setpgid(void){
	int pid, pgid;
	struct proc *p, *cur = myproc();

	if(argint(0, &pid) < 0)
		return -1;
	if(argint(1, &pgid) < 0)
		return -1;

	acquire(&ptable.lock);

	if(pid == 0)
		pid = cur->pid;
	if(pgid == 0)
		pgid = pid;

	// find the target process
	p = 0;
	for(struct proc *pp = ptable.proc; pp < &ptable.proc[NPROC]; pp++){
		if(pp->pid == pid){
			p = pp;
			break;
		}
	}
	if(!p){
		release(&ptable.lock);
		return -1; // no such PID
	}

	if(p->parent != cur){
		release(&ptable.lock);
		return -1;
	}

	p->parent->gid = pgid;
	release(&ptable.lock);
	return 0;
}
