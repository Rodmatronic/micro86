#include "../include/types.h"
#include "../include/stat.h"
#include "../include/user.h"

/*
 * ln [ -f ] target [ new name ]
 */

char	*rindex();

int
main(argc, argv)
int argc;
char **argv;
{
	struct stat statb;
	register char *np;
	int fflag = 0;
	char nb[100], *name=nb, *arg2;
	int statres;

	if (argc >1 && strcmp(argv[1], "-f")==0) {
		argc--;
		argv++;
		fflag++;
	}
	if (argc<2 || argc>3) {
		fprintf(stderr, "Usage: ln target [ newname ]\n");
		exit(1);
	}
	np = strrchr(argv[1], '/');
	if (np==0)
		np = argv[1];
	else
		np++;
	if (argc==2)
		arg2 = np;
	else
		arg2 = argv[2];
	statres = stat(argv[1], &statb);
	if (statres<0) {
		fprintf(stderr, "ln: %s does not exist\n", argv[1]);
		exit(1);
	}
	if (fflag==0 && (statb.mode&S_IFMT) == S_IFDIR) {
		fprintf(stderr, "ln: %s is a directory\n", argv[1]);
		exit(1);
	}
	statres = stat(arg2, &statb);
	if (statres>=0 && (statb.mode&S_IFMT) == S_IFDIR)
		sprintf(name, "%s/%s", arg2, np);
	else
		name = arg2;
	if (link(argv[1], name)<0) {
		fprintf(stderr, "ln: failed to link file\n");
		exit(1);
	}
	exit(0);
}

