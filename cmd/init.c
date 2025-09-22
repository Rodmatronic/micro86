// init: The initial user-level program

#include "../include/types.h"
#include "../include/stat.h"
#include "../include/stdio.h"
#include "../include/fcntl.h"
#include "../include/errno.h"
#include "../include/signal.h"

#define LVLQ		SIGHUP
#define	LVL0		SIGINT
#define	LVL1		SIGQUIT
#define	LVL2		SIGILL
#define	LVL3		SIGTRAP
#define	LVL4		SIGIOT
#define	LVL5		SIGEMT
#define	LVL6		SIGFPE
#define	SINGLE_USER	SIGBUS
#define	LVLa		SIGSEGV
#define	LVLb		SIGSYS
#define	LVLc		SIGPIPE

char	shell[] =	"/bin/sh";
char	su[]	=	"/usr/bin/su";
char	runc[] =	"/etc/rc";
char	utmp[] =	"/etc/utmp";
char	wtmpf[] =	"/usr/adm/wtmp";
char	getty[] = 	"/etc/getty";

int	init_signal = 0;
int	chg_lvl_flag = -1;

struct {
	char	name[8];
	char	tty;
	char	fill;
	int	time[2];
	int	wfill;
} wtmp;

void console(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "INIT: ");
    vprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

void
single()
{
	int pid;
	pid = fork();
	if(pid == 0) {
		console("SINGLE USER MODE");
		execl(su, 0);
		fprintf(stderr, "INIT: execl of %s failed; errno = %d\n", su, errno);
		exit(1);
	}
	while(wait(0) != pid);
	/* When CTRL+D'd out of single user, run default state */
	runrc(rstateid("is"));
}

char
level(state)
int	state;
{
	register char	answer;

	switch(state) {

	case LVL0:
		answer = '0';
		break;
	case LVL1:
		answer = '1';
		break;
	case LVL2:
		answer = '2';
		break;
	case LVL3:
		answer = '3';
		break;
	case LVL4:
		answer = '4';
		break;
	case LVL5:
		answer = '5';
		break;
	case LVL6:
		answer = '6';
		break;
	case SINGLE_USER:
		answer = 'S';
		break;
	case LVLa:
		answer = 'a';
		break;
	case LVLb:
		answer = 'b';
		break;
	case LVLc:
		answer = 'c';
		break;
	default:
		answer = '?';
		break;
	}
	return(answer);
}

void
userinit(int argc, char** argv)
{
	if (argc != 2 || argv[1][1] != '\0') {
		fprintf(stderr, "usage: init [0123456SsQqabc]\n");
		exit(0);
	}

	switch (argv[1][0]) {

	case 'Q':
	case 'q':
		init_signal = LVLQ;
		break;

	case '0':
		init_signal = LVL0;
		break;

	case '1':
		init_signal = LVL1;
		break;

	case '2':
		init_signal = LVL2;
		break;

	case '3':
		init_signal = LVL3;
		break;

	case '4':
		init_signal = LVL4;
		break;

	case '5':
		init_signal = LVL5;
		break;

	case '6':
		init_signal = LVL6;
		break;

	case 'S':
	case 's':
		init_signal = SINGLE_USER;
		break;

	case 'a':
		init_signal = LVLa;
		break;

	case 'b':
		init_signal = LVLb;
		break;

	case 'c':
		init_signal = LVLc;
		break;

	default:
		fprintf(stderr, "usage: init [0123456SsQqabc]\n");
		exit(1);
	}
}

/*
 * Get inittab state ID for inittab parsing
 */

int
rstateid(const char* target_id)
{
    static char rstate[16];
    char buf[256];
    int fd = open("/etc/inittab", O_RDONLY);
    if (fd < 0) {
        console("Failed to open /etc/inittab");
        return 0;
    }

    int n;
    while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        char *line = strtok(buf, "\n");
        while (line) {
            char *p = line;
            char *fields[4];
            int i = 0;

            while (i < 4 && (fields[i] = strsep(&p, ":")) != NULL) {
                i++;
            }

            if (i == 4 && strcmp(fields[0], target_id) == 0) {
                strcpy(rstate, fields[1]);
                close(fd);
                return atoi(rstate);
            }

            line = strtok(NULL, "\n");
        }
    }

    close(fd);
    return 0;  // not found
}

/*
 * Run an RC script from level
 */

void
runrc(int level)
{
    char rc_path[32];
    if (level == 's' || level == 'S')
	sprintf(rc_path, "/etc/rcS");
    else
	sprintf(rc_path, "/etc/rc%d", level);

    int pid = fork();
    if (pid < 0) {
        console("Failed to fork for rc script");
	single();
        return 1;
    }

    if (pid == 0) {
        execl(shell, "sh", rc_path, (char *)0);
        console("Failed to exec %s", rc_path);
	single();
        return 1;
    }

    while (wait(0) != pid);
}

/*
 * We are the first process, not from user.
 * Get the system working based on what we
 * see in /etc/inittab
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

	console("default run level: %d", rstateid("is"));
	runrc(rstateid("is"));

	close(open(utmp, O_CREATE | O_RDWR));
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
	int chg_lvl_flag;

	/* Dispose of random users. */
	if (getuid() != 0) {
		printf("init: %s\n", strerror(EPERM));
		exit(1);
	}

	/* Are we invoked from the user? */
	if (getpid() != 1) {
		userinit(argc, argv);
		chg_lvl_flag = 0;
	} else {
		/* No */
		chg_lvl_flag = -1;
		sysinit(argc, argv);
	}

	if (init_signal == SINGLE_USER) {
		runrc(level(SINGLE_USER));
		single();
		chg_lvl_flag = -1;
	}

	if (chg_lvl_flag != -1) {
		chg_lvl_flag = -1;
		console("New run level: %c", level(init_signal));
		runrc(atoi(argv[1]));
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
