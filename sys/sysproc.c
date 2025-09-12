#include "../include/types.h"
#include "../include/x86.h"
#include "../include/defs.h"
#include "../include/param.h"
#include "../include/memlayout.h"
#include "../include/mmu.h"
#include "../include/proc.h"
#include "../include/time.h"
#include "../include/utsname.h"
#include "../include/tty.h"
#include "../include/version.h"

int
sys_gtty(void)
{
  return myproc()->ttyflags;
}

extern struct ttyb ttyb;
extern struct cons cons;

int
sys_stty(void)
{
  struct ttyb *uttyb;
  if (argptr(0, (char **)&uttyb, sizeof(struct ttyb)) < 0)
    return -1;

  acquire(&cons.lock);
  ttyb.speeds = uttyb->speeds;
  ttyb.tflags = uttyb->tflags;
  ttyb.erase = uttyb->erase;
  ttyb.kill = uttyb->kill;
  release(&cons.lock);
  return 0;
}

#define MAX 255

char sys_nodename[MAX] = "localhost";

int
sys_uname(void)
{
  struct utsname *u;
  char *addr;

  if(argptr(0, &addr, sizeof(*u)) < 0)
    return -1;
  u = (struct utsname *)addr;
  safestrcpy(u->sysname, sys_name, sizeof(u->sysname));
  safestrcpy(u->nodename, sys_nodename, sizeof(u->nodename));
  safestrcpy(u->release, sys_release, sizeof(u->release));
  // version-candidate
  safestrcpy(u->version, sys_version, sizeof(u->version));
  safestrcpy(u->machine, "i386", sizeof(u->machine));
  safestrcpy(u->domainname, "domainname", sizeof(u->domainname));
  return 0;
}


int sys_sethostname(void) {
    char *name;
    int len;

    if (argstr(0, &name) < 0 || argint(1, &len) < 0)
        return -1;

    if (len <= 0 || len >= MAX)
        return -1;

    if (myproc()->uid != 0)
        return -1;

    if (safestrcpy(sys_nodename, name, MAX) < 0)
        return -1;

    return 0;
}

int sys_stime(void) {
    unsigned long epoch;
    if (argint(0, (int*)&epoch) < 0)
        return -1;
    struct proc *p = myproc();
    if (p->uid != 0) return 1; // Operation not permitted
    set_kernel_time((unsigned long)epoch);
    return 0;
}

// seconds since epoch
int
sys_time(void)
{
  int esec;
  if (argint(0, &esec) < 0)
    return -1;
  return epoch_mktime();
}

int
sys_usleep(void)
{
  int usec;
  if (argint(0, &usec) < 0)
    return -1;
  int ticks_needed = usec / 10000; // each tick ~10,000 us
  if (ticks_needed == 0)
    ticks_needed = 1; // minimum sleep = 1 tick
  acquire(&tickslock);
  uint ticks0 = ticks;
  while (ticks - ticks0 < ticks_needed)
    sleep(&ticks, &tickslock);
  release(&tickslock);
  return 0;
}

int
sys_setgid(void) {
  int gid;
  if (argint(0, &gid) < 0) return -1;

  struct proc *p = myproc();
//  if (p->gid != 0) return 1; // Operating not permitted
  p->gid = gid;
  return 0;
}

int
sys_setuid(void) {
  int uid;
  if (argint(0, &uid) < 0) return -1;

  struct proc *p = myproc();
//  if (p->uid != 0) return 1; // Operation not permitted
  p->uid = uid;
  return 0;
}

int
sys_getgid(void)
{
  return myproc()->gid;
}

int
sys_getuid(void)
{
  return myproc()->uid;
}

int
sys_fork(void)
{
  return fork();
}

void
sys_exit(void)
{
  int status;
  if(argint(0, &status) < 0)  // Get exit status from first argument
    return;
  exit(status);
}

int
sys_wait(void)
{
  int *status;
  if(argptr(0, (void*)&status, sizeof(*status)) < 0) // Get status pointer
    return -1;
  return wait(status);
}

int
sys_kill(void)
{
  int pid;
  int status;

  if(argint(0, &pid) < 0 || argint(1, &status) < 0)
    return -1;
  
  return kill(pid, status);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
