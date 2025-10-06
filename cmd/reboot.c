
/*
 * reboot or halt the system
 */

#include "../include/stdio.h"
#include "../include/errno.h"
#include "../include/syslog.h"
#include "../include/pwd.h"

int syncint;
int halt;

void
usage()
{
	fprintf(stderr,
	    "usage: %s [ -hn ]\n", getprogname());
	exit(1);
}

int
reboot_call()
{
	char *user;
	struct passwd *pw;
        if ( getuid() != 0 ) {
                errno = EPERM;
		return -1;
	}
	user = getlogin();
	if (user == (char *)0 && (pw = getpwuid(getuid())))
		user = pw->pw_name;
	if (user == (char *)0)
		user = "root";
	if (halt)
		syslog(2, "halted by %s", user);
	else
		syslog(2, "rebooted by %s", user);
	if (!syncint){
		sync();
	}
        if (halt) {
		reboot(0);
	} else {
		reboot(1);
	}
	return -1;
}

int
main(int argc, char **argv)
{
  int c;

  // halt or reboot
  setprogname(argv[0]);
  while ((c = getopt(argc, argv, "hn")) != EOF) {
	switch((char)c) {
	case 'h':
		halt=1;
		break;
	case 'n':
		syncint=1;
		break;
	default:
		usage();
	}
  }

  reboot_call();
  exit(1);
}
