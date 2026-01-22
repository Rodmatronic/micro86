// System call numbers
#define SYS_syscall	0
#define SYS_exit	1
#define SYS_fork	2
#define SYS_read	3
#define SYS_write	4
#define SYS_open	5
#define SYS_close	6
#define SYS_waitpid	7
#define SYS_creat	8
#define SYS_link	9
#define SYS_unlink	10
#define SYS_exec	11 // replace with execve
#define SYS_chdir	12
#define SYS_time	13
#define SYS_mknod	14
#define SYS_chmod	15
#define SYS_chown	16
#define SYS_break	17 // TODO
#define SYS_stat	18
#define SYS_lseek	19
#define SYS_getpid	20
#define SYS_mount	21 // TODO
#define SYS_umount	22 // TODO
#define SYS_setuid	23
#define SYS_getuid	24
#define SYS_stime	25
#define SYS_ptrace	26 // TODO
#define SYS_alarm	27
#define SYS_fstat	28 // TODO
#define SYS_pause	29
#define SYS_utime	30
#define SYS_stty	31
#define SYS_gtty	32
#define SYS_access	33
#define SYS_nice	34 // TODO
#define SYS_ftime	35 // TODO
#define SYS_sync	36
#define SYS_kill	37
#define SYS_rename	38 // TODO
#define SYS_mkdir	39
#define SYS_rmdir	40 // TODO
#define SYS_dup		41
#define SYS_pipe	42
#define SYS_times	43 // TODO
#define SYS_prof	44 // TODO
#define SYS_brk		45
#define SYS_setgid	46
#define SYS_getgid	47
#define SYS_signal	48
#define SYS_geteuid	49
#define SYS_getegid	50
#define SYS_acct	51 // TODO
#define SYS_phys	52 // TODO
#define SYS_lock	53 // TODO
#define SYS_ioctl	54
#define SYS_fcntl	55
#define SYS_mpx		56 // TODO
#define SYS_setpgid	57 // TODO
#define SYS_ulimit	58 // TODO
#define SYS_sbrk	59
#define SYS_umask	60 // TODO
#define SYS_chroot	61 // TODO
#define SYS_ustat	62 // TODO
#define SYS_dup2	63 // TODO
#define SYS_getppid	64
#define SYS_getpgrp	65
#define SYS_setsid	66 // TODO
#define SYS_sigaction	67 // TODO
#define SYS_sgetmask	68 // TODO
#define SYS_ssetmask	69 // TODO
#define SYS_sethostname	74
#define SYS_getrusage	77
#define SYS_setgroups	81
#define SYS_symlink	83
#define SYS_reboot	88
/*
  don't worry about it
*/
#define SYS_wait4	114
#define SYS_sigreturn	119
#define SYS_setdomainname	121
#define SYS_uname	122
#define SYS_getpgid	132
#define SYS_getdents	141
#define SYS_writev	146
#define SYS_nanosleep	162
#define SYS_setresuid	164
#define SYS_setresgid	170
#define SYS_rt_sigreturn	173
#define SYS_rt_sigprocmask	175
#define SYS_rt_sigaction	174
#define SYS_rt_sigsuspend	179
#define SYS_getcwd	183
#define SYS_vfork	190
#define SYS_getuid32	199
#define SYS_getgid32	200
#define SYS_geteuid32	201
#define SYS_getegid32	202
#define SYS_setgroups32	206
#define SYS_setresuid32	208
#define SYS_setresgid32	210
#define SYS_setuid32	213
#define SYS_setgid32	214
#define SYS_getdents64	220
#define SYS_fcntl64	221
/*
 * rather large gap
 */
#define SYS_exit_group	252
#define SYS_set_tid_address	258
#define SYS_clock_settime32	264
#define SYS_clock_gettime	265
/*
 * another rather large gap
 */
#define SYS_linkat	303
#define SYS_statx	383
/*
 * mind the gap
 */
#define SYS_clock_gettime64	403
