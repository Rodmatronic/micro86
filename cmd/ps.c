#include <stdio.h>
#include <types.h>
#include <pwd.h>

int jflg;
int errflg;
int aflg;

usage()		/* print usage message and quit */
{
	static char usage[] = "ps [ -aj ]";

	fprintf(stderr, "usage: %s\n", usage);
	exit(1);
}

int printprocs() {
	struct uproc proc;
	int pamount = devctl(3, 0, 0);
	int isusr = 1;
	register struct passwd *pw;
	printf("    PID");
	if (jflg) printf("   PGID");
	printf(" USER     ");
	printf("CMD");
	printf("\n");
	for (int i = 0; i < pamount; i++) {

		if (!getproc(i, &proc)) {
			if (!aflg) {
				if (proc.p_uid == getuid()) isusr = 1;
				else isusr = 0;
			}
			if (isusr) {
				pw = getpwuid(proc.p_uid);
				printf("%7d ", proc.p_pid);
				if (jflg) printf("%6d ", proc.p_gid);
				if (pw) printf("%-8s ", pw->pw_name);
				else printf("%-8s ", proc.p_uid);
				printf("%s\n", proc.name);
			}

		}
	}
}

main (int argc, char *argv[]) {
	char c;
	int pgerrflg = 0;

	while ((c = getopt(argc, argv, "ja")) != EOF) {
		switch (c) {
			case 'j':
				jflg++;
				break;
			case 'a':
				aflg++;
				break;
			default:
				errflg++;
				break;
		}
	}
	if (errflg || optind < argc || pgerrflg)
		usage();

	printprocs();
}
