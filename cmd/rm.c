/*
 * remove a file/directory
 */

#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <fs.h>
#include <errno.h>

int errcode;
int rm(char arg[], int fflg, int rflg, int iflg, int level);
int dotname(char *s);
int rmdir(char *f, int iflg);
int yes();

void
usage()
{
	fprintf(stderr, "usage: rm [-fir] file ...\n");
	exit(1);
}

int
main(argc, argv)
int argc;
char *argv[];
{
	int opt;
	int fflg = 0, iflg = 0, rflg = 0;
	while ((opt = getopt(argc, argv, "fir")) != -1) {
		switch(opt) {
		case 'f':
			fflg++;
			break;
		case 'i':
			iflg++;
			break;
		case 'r':
			rflg++;
			break;
		default:
			usage();
		}
	}

	if (optind >= argc) {
		usage();
	}

	for (; optind < argc; optind++) {
		if (strcmp(argv[optind], "..") == 0) {
			fprintf(stderr, "rm: cannot remove `..'\n");
			continue;
		}
		rm(argv[optind], fflg, rflg, iflg, 0);
	}
	exit(errcode);
}

int
rm(arg, fflg, rflg, iflg, level)
char arg[];
int fflg;
int rflg;
int iflg;
int level;
{
	struct stat buf;
	int d;

	if(stat(arg, &buf)) {
		if (fflg==0) {
			perror(arg);
			++errcode;
		}
		return 0;
	}

	if ((buf.st_mode&S_IFMT) == S_IFDIR) {
		if(rflg) {
			if(iflg && level!=0) {
				printf("directory %s: ", arg);
				if(!yes())
					return 1;
			}
			if((d=open(arg, 0)) < 0) {
				perror(arg);
				exit(1);
			}

			struct dirent direct;
			char name[512];

			while(read(d, &direct, sizeof(direct)) == sizeof(direct)) {
				if(direct.inum != 0 && !dotname(direct.name)) {
					sprintf(name, "%s/%s", arg, direct.name);
					rm(name, fflg, rflg, iflg, level+1);
				}
			}
			close(d);
			errcode += rmdir(arg, iflg);
			return 0;
		}
		lperror(EISDIR, arg);
		++errcode;
		return 1;
	}
	if(iflg) {
		printf("%s: ", arg);
		if(!yes())
			return 1;
	}
	else if(!fflg) {
//		if (access(arg, 02)<0) {
//			printf("rm: %s %o mode ", arg, buf.mode&0777);
//			if(!yes())
//				return;
//		}
	}
	if(unlink(arg) && (fflg==0 || iflg)) {
		perror(arg);
		++errcode;
	}
	return 0;
}

int
dotname(s)
char *s;
{
	if(s[0] == '.')
		if(s[1] == '.')
			if(s[2] == '\0')
				return(1);
			else
				return(0);
		else if(s[1] == '\0')
			return(1);
	return(0);
}

int
rmdir(f, iflg)
char *f;
int iflg;
{
	int status, i;

	if(dotname(f))
		return(0);
	if(iflg) {
		printf("%s: ", f);
		if(!yes())
			return(0);
	}
	while((i=fork()) == -1)
		sleep(3);
	if(i) {
		wait(&status);
		return(status);
	}
	execl("/bin/rmdir", "rmdir", f, 0);
	fprintf(stderr, "rm: can't find rmdir\n");
	exit(1);
}

int
yes()
{
	int i, b;

	i = b = getchar();
	while(b != '\n' && b != EOF)
		b = getchar();
	return(i == 'y');
}

