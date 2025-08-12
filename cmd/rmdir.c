#include "../include/types.h"
#include "../include/stat.h"
#include "../include/user.h"
#include "../include/fcntl.h"
#include "../include/fs.h"

int	Errors = 0;
char	*rindex();
char	*strcat();
char	*strcpy();

int
main(argc,argv)
int argc;
char **argv;
{
	if(argc < 2) {
		fprintf(stderr, "rmdir: arg count\n");
		exit(1);
	}
	while(--argc)
		rmdir(*++argv);
	exit(Errors!=0);
}

int
rmdir(d)
char *d;
{
	int	fd;
	char	*np, name[500];
	struct	stat	st, cst;
	struct	dirent	dir;

	strcpy(name, d);
	if((np = strchr(name, '/')) == NULL)
		np = name;
	if(stat(name,&st) < 0) {
		fprintf(stderr, "rmdir: %s non-existent\n", name);
		++Errors;
		return 1;
	}
	if (stat("", &cst) < 0) {
		fprintf(stderr, "rmdir: cannot stat \"\"");
		++Errors;
		exit(1);
	}
	if((st.mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "rmdir: %s not a directory\n", name);
		++Errors;
		return 1;
	}
	if(st.ino==cst.ino &&st.dev==cst.dev) {
		fprintf(stderr, "rmdir: cannot remove current directory\n");
		++Errors;
		return 1;
	}
	if((fd = open(name,0)) < 0) {
		fprintf(stderr, "rmdir: %s unreadable\n", name);
		++Errors;
		return 1;
	}
	while(read(fd, (char *)&dir, sizeof dir) == sizeof dir) {
		if(!strcmp(dir.name, ".") || !strcmp(dir.name, ".."))
			continue;
		fprintf(stderr, "rmdir: %s not empty\n", name);
		++Errors;
		close(fd);
		return 1;
	}
	close(fd);
	if(!strcmp(np, ".") || !strcmp(np, "..")) {
		fprintf(stderr, "rmdir: cannot remove . or ..\n");
		++Errors;
		return 1;
	}
	strcat(name, "/.");
	strcat(name, ".");
//unl:
	unlink(name);	// unlink name/..
//unl2:
	name[strlen(name)-1] = '\0';
	unlink(name);	// unlink name/.
	name[strlen(name)-2] = '\0';
	if (unlink(name) < 0) {
		fprintf(stderr, "rmdir: %s not removed\n", name);
		++Errors;
	}
	return 0;
}

