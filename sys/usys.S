#include "../include/syscall.h"
#include "../include/traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(getuid)
SYSCALL(getgid)
SYSCALL(setuid)
SYSCALL(setgid)
SYSCALL(usleep)
SYSCALL(time)
SYSCALL(uname)
SYSCALL(sync)
SYSCALL(stty)
SYSCALL(lseek)
SYSCALL(devctl)
SYSCALL(stime)
SYSCALL(utime)
SYSCALL(sethostname)
SYSCALL(setenv)
SYSCALL(secure_getenv)
SYSCALL(environ)
