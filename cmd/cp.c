#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <fs.h>
#include <fcntl.h>

int
main(argc,argv)
char **argv;
{
	static struct stat buf, dirbuf;
	static char iobuf[512];
	static char pathbuf[256];
	int fold, fnew, n;
	register char *p1, *p2, *bp;

	if(argc != 3) {
		write(1, "Usage: cp oldfile newfile\n", 26);
		exit(1);
	}
	if((fold = open(argv[1], 0)) < 0) {
		perror(argv[1]);
		exit(1);
	}
	fstat(fold, &buf);

	/* is target a directory? */
	if (stat(argv[2], &dirbuf) >= 0 && (dirbuf.st_mode & S_IFMT) == S_IFDIR) {
		p1 = argv[1];
		p2 = argv[2];
		bp = pathbuf;
		while((*bp++ = *p2++));
		bp[-1] = '/';
		p2 = bp;
		while((*bp = *p1++))
			if(*bp++ == '/')
				bp = p2;
		argv[2] = pathbuf;
	}
	if (stat(argv[2], &dirbuf) >= 0) {
		if (buf.st_dev == dirbuf.st_dev && buf.st_ino == dirbuf.st_ino) {
			write(1, "Copying file to itself.\n", 24);
			exit(1);
		}
	}
	if ((fnew = open(argv[2], O_CREAT | O_WRONLY, 0666)) < 0) {
		write(1, "Can't create new file.\n", 23);
		exit(1);
	}
	while((n = read(fold,  iobuf,  512))) {
	if(n < 0) {
		write(1, "Read error\n", 11);
		exit(1);
	} else
		if(write(fnew, iobuf, n) != n){
			write(1, "Write error.\n", 13);
			exit(1);
		}
	}
	exit(0);
}
