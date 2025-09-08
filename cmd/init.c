// init: The initial user-level program

#include "../include/types.h"
#include "../include/stat.h"
#include "../include/stdio.h"
#include "../include/fcntl.h"
#include "../include/errno.h"

char	shell[] =	"/bin/sh";
char	runc[] =	"/etc/rc";
char	utmp[] =	"/etc/utmp";
char	wtmpf[] =	"/usr/adm/wtmp";
char	getty[] = 	"/etc/getty";

int s_flag = 0;

struct {
	char	name[8];
	char	tty;
	char	fill;
	int	time[2];
	int	wfill;
} wtmp;

int
main(void)
{
  int pid, wpid;
  int i;

  setprogname("init");

  /* Dispose of random users. */
  if (getuid() != 0)
    errc(1, EPERM, NULL);

  /* System V users like to reexec init. */
  if (getpid() != 1)
    err(1, "already running");

  if(open("/dev/console", O_RDWR) < 0){
    mknod("/dev/console", 1, 1);
    open("/dev/console", O_RDWR);
  }

//  mknod("/dev/null", 3, 0);
//  mknod("/dev/random", 4, 0);

  // run shell sequence
  dup(0);
  dup(0);

  i = fork();
  if(i == 0) {
    execl("/bin/sh", shell, runc, 0);
    exit(0);
  }
  while(wait(0) != i);
  close(open(utmp, O_CREATE | O_RDWR));
  if ((i = open(wtmpf, 1)) >= 0) {
    wtmp.tty = '~';
    time(wtmp.time);
    write(i, &wtmp, 16);
    close(i);
  }

  for(;;){
    pid = fork();
    if(pid < 0){
      fprintf(stderr, "init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      execl(getty, 0);
      exit(1);
    }
    while((wpid=wait(0)) >= 0 && wpid != pid){}
  }
}
