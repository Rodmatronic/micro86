/*
 * This is the first userland process. Its job is to set up console and run /sbin/init.
 */

#include <syscall.h>
#include <traps.h>

.globl start
start:
	# mkdir /dev octal 700
	movl	$SYS_mkdir, %eax
	movl	$devdir, %ebx
	movl	$0700, %ecx
	int	$T_SYSCALL

	# mknod /dev/console
	movl    $SYS_mknod, %eax
	movl    $console, %ebx
	movl    $0x21B0, %ecx               # S_IFCHR=0x2000, 0660=0x1B0
	movl    $((5 << 8) | 1), %edx       # makedev(major=5, minor=1)
	int     $T_SYSCALL

	# open /dev/console
	movl	$SYS_open, %eax
	movl	$console, %ebx
	movl	$0x002, %ecx
	int	$T_SYSCALL

	# dup 1
	movl	$SYS_dup, %eax
	movl	$0x0, %ebx
	int	$T_SYSCALL

	# dup 2
	movl	$SYS_dup, %eax
	movl	$0x0, %ebx
	int	$T_SYSCALL

	# write userland message
	movl	$SYS_write, %eax
	movl	$0x1, %ebx
	movl	$message, %ecx
	movl	$message_end - message -1, %edx
	int	$T_SYSCALL

	# exec("/sbin/init", argv)
	movl	$SYS_exec, %eax
	movl	$init, %ebx		# arg0 = path
	movl	$argv, %ecx		# arg1 = argv
	int	$T_SYSCALL

exit:
	movl	$SYS_exit, %eax
	movl	$0, %ebx
	int	$T_SYSCALL
	jmp	exit

init:
	.string "/sbin/init\0"
console:
	.string "/dev/console\0"
devdir:
	.string "/dev\0"
message:
	.string "initcode: trying /sbin/init\n"
message_end:

.p2align 2
argv:
	.long init
	.long 0

