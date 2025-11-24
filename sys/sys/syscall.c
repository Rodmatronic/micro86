#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <syscall.h>

int errno=0;

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
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

int
argint(int n, int *ip)
{
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

int
argptr(int n, char **pp, int size)
{
	int i;
	struct proc *curproc = myproc();

	if(argint(n, &i) < 0)
		return -1;
	if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
		return -1;

	*pp = (char*)i;
	return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
	int addr;
	if(argint(n, &addr) < 0)
		return -1;
	return fetchstr(addr, pp);
}

extern int sys_syscall(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_read(void);
extern int sys_write(void);
extern int sys_open(void);
extern int sys_close(void);
extern int sys_wait(void);
extern int sys_creat(void);
extern int sys_link(void);
extern int sys_unlink(void);
extern int sys_exec(void);
extern int sys_chdir(void);
extern int sys_time(void);
extern int sys_mknod(void);
extern int sys_chmod(void);
extern int sys_chown(void);
extern int sys_break(void);
extern int sys_stat(void);
extern int sys_lseek(void);
extern int sys_getpid(void);
extern int sys_mount(void);
extern int sys_umount(void);
extern int sys_setuid(void);
extern int sys_getuid(void);
extern int sys_stime(void);
extern int sys_ptrace(void);
extern int sys_alarm(void);
extern int sys_fstat(void);
extern int sys_pause(void);
extern int sys_utime(void);
extern int sys_stty(void);
extern int sys_gtty(void);
extern int sys_access(void);
extern int sys_nice(void);
extern int sys_ftime(void);
extern int sys_sync(void);
extern int sys_kill(void);
extern int sys_rename(void);
extern int sys_mkdir(void);
extern int sys_rmdir(void);
extern int sys_dup(void);
extern int sys_pipe(void);
extern int sys_times(void);
extern int sys_prof(void);
extern int sys_brk(void);
extern int sys_setgid(void);
extern int sys_getgid(void);
extern int sys_signal(void);
extern int sys_geteuid(void);
extern int sys_getegid(void);
extern int sys_acct(void);
extern int sys_phys(void);
extern int sys_lock(void);
extern int sys_ioctl(void);
extern int sys_fcntl(void);
extern int sys_mpx(void);
extern int sys_setpgid(void);
extern int sys_ulimit(void);
extern int sys_uname(void);
extern int sys_umask(void);
extern int sys_chroot(void);
extern int sys_ustat(void);
extern int sys_dup2(void);
extern int sys_getppid(void);
extern int sys_getpgrp(void);
extern int sys_setsid(void);
extern int sys_sigaction(void);
extern int sys_sgetmask(void);
extern int sys_ssetmask(void);
extern int sys_writev(void);

static int (*syscalls[])(void) = {
	[SYS_syscall]	sys_syscall,
	[SYS_exit]	sys_exit,
	[SYS_fork]	sys_fork,
	[SYS_read]	sys_read,
	[SYS_write]	sys_write,
	[SYS_open]	sys_open,
	[SYS_close]	sys_close,
	[SYS_wait]	sys_wait,
	[SYS_creat]	sys_creat,
	[SYS_link]	sys_link,
	[SYS_unlink]	sys_unlink,
	[SYS_exec]	sys_exec,
	[SYS_chdir]	sys_chdir,
	[SYS_time]	sys_time,
	[SYS_mknod]	sys_mknod,
	[SYS_chmod]	sys_chmod,
	[SYS_chown]	sys_chown,
	[SYS_break]	sys_break,
	[SYS_stat]	sys_stat,
	[SYS_lseek]	sys_lseek,
	[SYS_getpid]	sys_getpid,
	[SYS_mount]	sys_mount,
	[SYS_umount]	sys_umount,
	[SYS_setuid]	sys_setuid,
	[SYS_getuid]	sys_getuid,
	[SYS_stime]	sys_stime,
	[SYS_ptrace]	sys_ptrace,
	[SYS_alarm]	sys_alarm,
	[SYS_fstat]	sys_fstat,
	[SYS_pause]	sys_pause,
	[SYS_utime]	sys_utime,
	[SYS_stty]	sys_stty,
	[SYS_gtty]	sys_gtty,
	[SYS_access]	sys_access,
	[SYS_nice]	sys_nice,
	[SYS_ftime]	sys_ftime,
	[SYS_sync]	sys_sync,
	[SYS_kill]	sys_kill,
	[SYS_rename]	sys_rename,
	[SYS_mkdir]	sys_mkdir,
	[SYS_rmdir]	sys_rmdir,
	[SYS_dup]	sys_dup,
	[SYS_pipe]	sys_pipe,
	[SYS_times]	sys_times,
	[SYS_prof]	sys_prof,
	[SYS_brk]	sys_brk,
	[SYS_setgid]	sys_setgid,
	[SYS_getgid]	sys_getgid,
	[SYS_signal]	sys_signal,
	[SYS_geteuid]	sys_geteuid,
	[SYS_getegid]	sys_getegid,
	[SYS_acct]	sys_acct,
	[SYS_phys]	sys_phys,
	[SYS_lock]	sys_lock,
	[SYS_ioctl]	sys_ioctl,
	[SYS_fcntl]	sys_fcntl,
	[SYS_mpx]	sys_mpx,
	[SYS_setpgid]	sys_setpgid,
	[SYS_ulimit]	sys_ulimit,
	[SYS_uname]	sys_uname,
	[SYS_umask]	sys_umask,
	[SYS_chroot]	sys_chroot,
	[SYS_ustat]	sys_ustat,
	[SYS_dup2]	sys_dup2,
	[SYS_getppid]	sys_getppid,
	[SYS_getpgrp]	sys_getpgrp,
	[SYS_setsid]	sys_setsid,
	[SYS_sigaction]	sys_sigaction,
	[SYS_sgetmask]	sys_sgetmask,
	[SYS_ssetmask]	sys_ssetmask,
	[SYS_writev]	sys_writev,
};

void
syscall(void)
{
	struct proc *p;
	int num;

	p = myproc();

	num = p->tf->eax;

	if(num >= 0 && num < NELEM(syscalls) && syscalls[num]){
		p->tf->eax = syscalls[num]();
	} else {
		p->tf->eax = -1;
	}

}

