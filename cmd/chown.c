/*
 * chown uid file ...
 */

#include <stdio.h>
#include <ctype.h>
#include <types.h>
#include <stat.h>
#include <pwd.h>

struct	passwd	*pwd,*getpwnam();
struct	stat	stbuf;
int	uid;
int	status;

int
main(argc, argv)
char *argv[];
{
	int c;
	if(argc < 3) {
		printf("usage: chown uid file ...\n");
		exit(4);
	}
	if(isnumber(argv[1])) {
		uid = atoi(argv[1]);
		goto cho;
	}
	if((pwd=getpwnam(argv[1])) == NULL) {
		printf("unknown user id: %s\n",argv[1]);
		exit(4);
	}
	uid = pwd->pw_uid;

cho:
	for(c=2; c<argc; c++) {
		if (stat(argv[c], &stbuf) < 0) {
			printf("%s: file not found\n", argv[c]);
			status = 1;
			continue;
		}
		if (chown(argv[c], uid, stbuf.st_gid) < 0) {
			printf("%s: cannot change ownership\n", argv[c]);
			status = 1;
		}
	}
	exit(status);
}

int
isnumber(s)
char *s;
{
	register c;

	while((c = *s++))
		if(!isdigit(c))
			return(0);
	return(1);
}

