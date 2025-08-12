#include "../include/types.h"
#include "../include/stat.h"
#include "../include/user.h"
#include "../include/errno.h"

int
main(int argc, char **argv)
{
	int signo, pid, res;
	int errlev;
	int errno = 1;

	if (argc <= 1) {
	usage:
		fprintf(stderr, "usage: kill [ -signo ] pid ...\n");
		exit(2);
	}
	if (*argv[1] == '-') {
		signo = atoi(argv[1]+1);
		argc--;
		argv++;
	} else
		signo = 15;
	argv++;
	while (argc > 1) {
		if (**argv<'0' || **argv>'9')
			goto usage;
		res = kill(pid = atoi(*argv), signo);
		if (res<0) {
			printf("%d: %s\n", pid, strerror(errno));
			errlev = 1;
		}
		argc--;
		argv++;
	}
	return(errlev);
}

