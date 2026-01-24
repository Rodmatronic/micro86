#ifndef PROC_H
#define PROC_H

#include <param.h>
#include <signal.h>
#include <types.h>

// Per-CPU state
struct cpu {
	uchar apicid;	// Local APIC ID
	struct context *scheduler;	// swtch() here to enter scheduler
	struct taskstate ts;		// Used by x86 to find stack for interrupt
	struct segdesc gdt[NSEGS];	// x86 global descriptor table
	volatile unsigned int started;	// Has the CPU started?
	int ncli;	// Depth of pushcli nesting.
	int intena;	// Were interrupts enabled before pushcli?
	struct proc *proc;	 // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
	unsigned int edi;
	unsigned int esi;
	unsigned int ebx;
	unsigned int ebp;
	unsigned int eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
	unsigned int sz;	// Size of process memory (bytes)
	pde_t* pgdir;		// Page table
	char *kstack;		// Bottom of kernel stack for this process
	enum procstate state;	// Process state
	int pid;		// Process ID
	struct proc *parent;	// Parent process
	struct trapframe *tf;	// Trap frame for current syscall
	struct context *context;	// swtch() here to run process
	void *chan;		// If non-zero, sleeping on chan
	int killed;		// If non-zero, have been killed
	struct file *ofile[NOFILE];	// Open files
	struct inode *cwd;	// Current directory
	char name[16];		// Process name (debugging)
	unsigned short uid, euid, suid; // User ID
	unsigned short gid, egid, sgid; // Group ID
	long pgrp;		// Process Group
	int exitstatus;		// Exit status number
	int ttyflags;		// TTY flags
	unsigned int sighandlers[NSIG];	// List of signal handlers
	unsigned int sigrestorers[NSIG];	// List of signal restorers
	int alarmticks;		// Ticks until alarm fires
	int alarminterval;	// Original alarm interval
	unsigned int sigmask;	// Signal mask
	unsigned int sigpending;	// Pending signal
	unsigned int saved_trapframe_sp;	// Signal trapframe (for returning)
	char cloexec[NOFILE];	// Close-on-exec
	unsigned short umask;	// Permission mask
	int session;		// Session ID
	int leader;		// Is session leader?
	int tty;		// Current tty, -1 if not tty
	int groups[NGROUPS];	// Supplementary group IDs
};

// Process memory is laid out contiguously, low addresses first:
//	 text
//	 original data and bss
//	 fixed-size stack
//	 expandable heap
#endif
