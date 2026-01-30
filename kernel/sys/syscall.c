#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <syscall.h>
#include <config.h>
#include <debug.h>
#include <sys.h>
#include <errno.h>

/*
 * User code makes a system call with INT T_SYSCALL.
 * Syscalls are defined by EAX, with the arguments
 * being the remaining EBX, ECX, EDX, ESI, EDI, EBP.
 */

// Fetch the int at addr from the current process.
int fetchint(unsigned int addr, int *ip){
	struct proc *curproc = myproc();

	if(addr >= curproc->sz || addr+4 > curproc->sz)
		return -1;
	*ip = *(int*)(addr);
	return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int fetchstr(unsigned int addr, char **pp){
	char *s, *ep;
	struct proc *curproc = myproc();

	if(addr >= curproc->sz)
		return -1;
	*pp = (char*)addr;
	ep = (char*)curproc->sz;
	for(s = *pp; s < ep; s++){
		if(*s == 0)
			return s - *pp;
	}
	return -1;
}

int argint(int n, int *ip){
	struct trapframe *tf = myproc()->tf;

	switch(n){
		case 0: *ip = tf->ebx; return 0;
		case 1: *ip = tf->ecx; return 0;
		case 2: *ip = tf->edx; return 0;
		case 3: *ip = tf->esi; return 0;
		case 4: *ip = tf->edi; return 0;
		case 5: *ip = tf->ebp; return 0;
	}
	return -1;
}

int argptr(int n, char **pp, int size){
	int i;
	struct proc *curproc = myproc();

	if(argint(n, &i) < 0)
		return -1;
	if(size < 0 || (unsigned int)i >= curproc->sz || (unsigned int)i+size > curproc->sz)
		return -1;

	*pp = (char*)i;
	return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int argstr(int n, char **pp){
	int addr;
	if(argint(n, &addr) < 0)
		return -1;
	return fetchstr(addr, pp);
}

static int (*syscalls[])(void) = {
	[SYS_syscall]	= sys_syscall,
	[SYS_exit]	= sys_exit,
	[SYS_fork]	= sys_fork,
	[SYS_read]	= sys_read,
	[SYS_write]	= sys_write,
	[SYS_open]	= sys_open,
	[SYS_close]	= sys_close,
	[SYS_waitpid]	= sys_waitpid,
	[SYS_creat]	= sys_creat,
	[SYS_link]	= sys_link,
	[SYS_unlink]	= sys_unlink,
	[SYS_exec]	= sys_exec,
	[SYS_chdir]	= sys_chdir,
	[SYS_time]	= sys_time,
	[SYS_mknod]	= sys_mknod,
	[SYS_chmod]	= sys_chmod,
	[SYS_chown]	= sys_chown,
	[SYS_break]	= sys_break,
	[SYS_stat]	= sys_stat,
	[SYS_lseek]	= sys_lseek,
	[SYS_getpid]	= sys_getpid,
	[SYS_mount]	= sys_mount,
	[SYS_umount]	= sys_umount,
	[SYS_setuid]	= sys_setuid,
	[SYS_getuid]	= sys_getuid,
	[SYS_stime]	= sys_stime,
	[SYS_ptrace]	= sys_ptrace,
	[SYS_alarm]	= sys_alarm,
	[SYS_fstat]	= sys_fstat,
	[SYS_pause]	= sys_pause,
	[SYS_utime]	= sys_utime,
	[SYS_stty]	= sys_stty,
	[SYS_gtty]	= sys_gtty,
	[SYS_access]	= sys_access,
	[SYS_nice]	= sys_nice,
	[SYS_ftime]	= sys_ftime,
	[SYS_sync]	= sys_sync,
	[SYS_kill]	= sys_kill,
	[SYS_rename]	= sys_rename,
	[SYS_mkdir]	= sys_mkdir,
	[SYS_rmdir]	= sys_rmdir,
	[SYS_dup]	= sys_dup,
	[SYS_pipe]	= sys_pipe,
	[SYS_times]	= sys_times,
	[SYS_prof]	= sys_prof,
	[SYS_brk]	= sys_brk,
	[SYS_setgid]	= sys_setgid,
	[SYS_getgid]	= sys_getgid,
	[SYS_signal]	= sys_signal,
	[SYS_geteuid]	= sys_geteuid,
	[SYS_getegid]	= sys_getegid,
	[SYS_acct]	= sys_acct,
	[SYS_phys]	= sys_phys,
	[SYS_lock]	= sys_lock,
	[SYS_ioctl]	= sys_ioctl,
	[SYS_fcntl]	= sys_fcntl,
	[SYS_mpx]	= sys_mpx,
	[SYS_setpgid]	= sys_setpgid,
	[SYS_ulimit]	= sys_ulimit,
	[SYS_sbrk]	= sys_sbrk,
	[SYS_sethostname]	= sys_sethostname,
	[SYS_umask]	= sys_umask,
	[SYS_chroot]	= sys_chroot,
	[SYS_ustat]	= sys_ustat,
	[SYS_dup2]	= sys_dup2,
	[SYS_getppid]	= sys_getppid,
	[SYS_getpgrp]	= sys_getpgrp,
	[SYS_setsid]	= sys_setsid,
	[SYS_sigaction]	= sys_sigaction,
	[SYS_sgetmask]	= sys_sgetmask,
	[SYS_ssetmask]	= sys_ssetmask,
	[SYS_setreuid]	= sys_setreuid,
	[SYS_setregid]	= sys_setregid,
	[SYS_getrusage]	= sys_getrusage,
	[SYS_setgroups]	= sys_setgroups,
	[SYS_symlink]	= sys_symlink,
	[SYS_reboot]	= sys_reboot,
	[SYS_iopl]	= sys_iopl,
	[SYS_idle]	= sys_idle,
	[SYS_wait4]	= sys_wait4,
	[SYS_sigreturn]	= sys_sigreturn,
	[SYS_setdomainname]	= sys_setdomainname,
	[SYS_uname]	= sys_uname,
	[SYS_getpgid]	= sys_getpgid,
	[SYS_getdents]	= sys_getdents,
	[SYS_writev]	= sys_writev,
	[SYS_nanosleep]	= sys_nanosleep,
	[SYS_setresuid]	= sys_setresuid,
	[SYS_setresgid]	= sys_setresgid,
	[SYS_rt_sigreturn]	= sys_rt_sigreturn,
	[SYS_rt_sigprocmask]	= sys_rt_sigprocmask,
	[SYS_rt_sigaction]	= sys_rt_sigaction,
	[SYS_rt_sigsuspend]	= sys_rt_sigsuspend,
	[SYS_getcwd]	= sys_getcwd,
	[SYS_vfork]	= sys_vfork,
	[SYS_getuid32]	= sys_getuid32,
	[SYS_getgid32]	= sys_getgid32,
	[SYS_geteuid32]	= sys_geteuid32,
	[SYS_getegid32]	= sys_getegid32,
	[SYS_setgroups32]	= sys_setgroups32,
	[SYS_setresuid32]	= sys_setresuid32,
	[SYS_setresgid32]	= sys_setresgid32,
	[SYS_setuid32]	= sys_setuid32,
	[SYS_setgid32]	= sys_setgid32,
	[SYS_getdents64]	= sys_getdents64,
	[SYS_fcntl64]	= sys_fcntl64,
	[SYS_exit_group]	= sys_exit_group,
	[SYS_set_tid_address]	= sys_set_tid_address,
	[SYS_clock_settime32]	= sys_clock_settime32,
	[SYS_clock_gettime]	= sys_clock_gettime,
	[SYS_linkat]	= sys_linkat,
	[SYS_statx]	= sys_statx,
	[SYS_clock_gettime64]	= sys_clock_gettime64,
	[SYS_clock_settime64]   = sys_clock_settime64,
};

void
syscall(void)
{
	struct proc *p;
	int num;

	p = myproc();
	num = p->tf->eax;
	debug("%s: %s(ebx=%d ecx=%d edx=%d esp=%d\n", myproc()->name, syscall_list[p->tf->eax], p->tf->ebx, p->tf->ecx, p->tf->edx, p->tf->esp);
	if(num >= 0 && num < NELEM(syscalls) && syscalls[num]){
		p->tf->eax = syscalls[num]();
	} else {
		debug("!!FIXME!!: unknown syscall %s, %d\n", syscall_list[num], num);
		p->tf->eax = -ENOSYS;
	}
}

