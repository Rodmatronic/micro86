// init: The initial user-level program

#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>

char	shell[] =	"/bin/sh";
char	runc[] =	"/etc/rc";
char	utmp[] =	"/etc/utmp";
char	wtmpf[] =	"/usr/adm/wtmp";
char	getty[] = 	"/sbin/getty";

void runsh(char * path);

struct {
	char	name[8];
	char	tty;
	char	fill;
	int	time[2];
	int	wfill;
} wtmp;

/*
 * Run a shell script
 */

void
runsh(char * path)
{
    syslog(LOG_INFO, "running script %s", path);
    int pid = fork();
    if (pid < 0) {
        syslog(LOG_PERROR, "failed to fork for shell script");
        return 1;
    }

    if (pid == 0) {
        execl(shell, "sh", path, (char *)0);
        syslog(LOG_PERROR, "failed to exec %s", path);
        return 1;
    }

    while (wait(0) != pid);
    syslog(LOG_INFO, "completed script %s\n", path);
}

/*
 * We are the first process
 * Get the system working
 */

void
sysinit(int argc, char** argv)
{
	int i;

	if(open("/dev/console", O_RDWR) < 0){
		mknod("/dev/console", 1, 1);
		open("/dev/console", O_RDWR);
	}

	// run shell sequence
	dup(0);
	dup(0);

	runsh(runc);

	close(open(utmp, O_CREAT | O_RDWR));
	if ((i = open(wtmpf, 1)) >= 0) {
		wtmp.tty = '~';
		time(wtmp.time);
		write(i, &wtmp, 16);
		close(i);
	}
}

int
main(int argc, char** argv)
{
	int pid, wpid;

	setprogname(basename(argv[0]));
	/* Dispose of random users. */
	if (getuid() != 0) {
		fprintf(stderr, "init: %s\n", strerror(EPERM));
		exit(1);
	}

	/* Are we the first process? */
	if (getpid() != 1) {
		fprintf(stderr, "init: %s\n", strerror(EALREADY));
		exit(1);
	} else {
		sysinit(argc, argv);
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
