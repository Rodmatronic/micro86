/*
 * List files or directories
 */

#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <fs.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>

#define	NFILES	1024
char	*pwdf, *dirf;
char	stdbuf[BUFSIZ];

struct lbuf {
	union {
		char	lname[15];
		char	*namep;
	} ln;
	char	ltype;
	short	lnum;
	short	lflags;
	short	lnl;
	short	luid;
	short	lgid;
	long	lsize;
	long	lmtime;
};

int	aflg, dflg, lflg, sflg, tflg, uflg, iflg, fflg, gflg, cflg;
int	rflg	= 1;
long	year;
int	flags;
int	lastuid	= -1;
char	tbuf[16];
long	tblocks;
int	statreq;
struct	lbuf	*flist[NFILES];
struct	lbuf	**lastp = flist;
struct	lbuf	**firstp = flist;
char	*dotp	= ".";

char	*makename();
struct	lbuf *gstat();
long	nblock();

#define	ISARG	0100000

char *pwdfname = NULL;
void rewind_pwdf() {
    if (pwdf >= 0) close(pwdf);
    pwdf = open(pwdfname, O_RDONLY);
    if (pwdf < 0) {
        fprintf(stderr, "ls: cannot rewind %s\n", pwdfname);
        exit(1);
    }
}

void
get_cols_rows(cols_p, rows_p)
int *cols_p;
int *rows_p;
{
	char *s;

	*cols_p = 80;
	*rows_p = 25;

	s = getenv("COLUMNS");
	if (s && *s)
		*cols_p = atoi(s);
	s = getenv("ROWS");
	if (s && *s)
		*rows_p = atoi(s);

	if (*cols_p <= 0) *cols_p = 80;
	if (*rows_p <= 0) *rows_p = 25;
}

void
printcols(entries, n)
struct lbuf **entries;
int n;
{
	int i, j, cols, rows;
	int maxlen = 0;
	int term_cols, term_rows;
	char *name;
	int colwidth;

	if (n <= 0)
	return;

	get_cols_rows(&term_cols, &term_rows);

	/* find longest name length */
	for (i = 0; i < n; i++) {
		if (entries[i]->lflags & ISARG)
			name = entries[i]->ln.namep;
		else
			name = entries[i]->ln.lname;
		if (name) {
			int len = strlen(name);
			if (len > maxlen)
				maxlen = len;
		}
	}

	colwidth = maxlen + 2;
	if (colwidth <= 0)
		colwidth = 1;

	cols = term_cols / colwidth;
	if (cols < 1)
		cols = 1;
	rows = (n + cols - 1) / cols;

	/* print row by row */
	for (i = 0; i < rows; i++) {
		int linelen = 0;
		for (j = 0; j < cols; j++) {
			int idx = j * rows + i;
			int len;
			if (idx >= n)
				continue;
			if (entries[idx]->lflags & ISARG)
				name = entries[idx]->ln.namep;
			else
				name = entries[idx]->ln.lname;
			if (!name)
				name = "";
			fputs(name, stdout);
			len = strlen(name);
			linelen += len;
			/* only pad if not last column */
			if (j < cols - 1 && (j + 1) * rows < n) {
				int spaces = colwidth - len;
				while (spaces-- > 0 && linelen < term_cols - 1) {
					putchar(' ');
					linelen++;
				}
			}
		}
		putchar('\n');
	}
}

int
main(argc, argv)
char *argv[];
{
	int i, c;
	register struct lbuf *ep, **ep1;
	register struct lbuf **slastp;
	struct lbuf **epp;
	struct lbuf lb;
	char *t;
	int compar();
	int onecol = 0;
	int opterr=0;

	time(&lb.lmtime);
	year = lb.lmtime - 6L*30L*24L*60L*60L; /* 6 months ago */
	while ((c=getopt(argc, argv,
			"1lasdglrtucif")) != EOF) switch(c) {
		case '1':
			onecol=1;
			continue;
		case 'a':
			aflg++;
			continue;

		case 's':
			sflg++;
			statreq++;
			continue;

		case 'd':
			dflg++;
			continue;

		case 'g':
			gflg++;
			continue;

		case 'l':
			lflg++;
			statreq++;
			continue;

		case 'r':
			rflg = -1;
			continue;

		case 't':
			tflg++;
			statreq++;
			continue;

		case 'u':
			uflg++;
			continue;

		case 'c':
			cflg++;
			continue;

		case 'i':
			iflg++;
			continue;

		case 'f':
			fflg++;
			continue;

		case '?':
			opterr++;

		default:
			continue;
	}

	if(opterr) {
		fprintf(stderr,"usage: ls -1lasdglrtucif [files]\n");
		exit(2);
	}

	if (fflg) {
		aflg++;
		lflg = 0;
		sflg = 0;
		tflg = 0;
		statreq = 0;
	}
	if(lflg) {
		t = "/etc/passwd";
		if(gflg)
			t = "/etc/group";
		pwdf = open(t, O_RDONLY);
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		static char *default_args[] = { ".", NULL };
		argc = 1;
		argv = default_args;
	}
	for (i = 0; i < argc; i++) {
		if ((ep = gstat(argv[i], 1)) == NULL)
			continue;
		ep->ln.namep = argv[i];
		ep->lflags |= ISARG;
	}
	qsort(firstp, lastp - firstp, sizeof *lastp, compar);
	slastp = lastp;
	for (epp=firstp; epp<slastp; epp++) {
		ep = *epp;
		if ((ep->ltype=='d' && dflg==0) || fflg) {
			if (argc>1)
				printf("\n%s:\n", ep->ln.namep);
			lastp = slastp;
			readdir(ep->ln.namep);
			if (fflg==0)
				qsort(slastp,lastp - slastp,sizeof *lastp,compar);
			if (lflg || sflg)
				printf("total %d\n", tblocks);

			if (!lflg && !sflg && !iflg && !onecol) {
				int n = lastp - slastp;
				struct lbuf **arr;
				int k;
				if (n > 0) {
					arr = (struct lbuf **) malloc(sizeof(struct lbuf *) * n);
					if (arr) {
						for (k = 0; k < n; k++)
							arr[k] = slastp[k];
						printcols(arr, n);
						free(arr);
					} else {
						/* On malloc failure, fall back to line by line */
						for (ep1=slastp; ep1<lastp; ep1++)
							pentry(*ep1);
					}
				}
			} else {
				for (ep1=slastp; ep1<lastp; ep1++)
					pentry(*ep1);
			}
		} else
			pentry(ep);
	}
	exit(0);
}

pentry(ap)
struct lbuf *ap;
{
//	struct { char dminor, dmajor;};
	register t;
	register struct lbuf *p;
	register struct passwd *pw;
//	register char *cp;

	p = ap;
	if (p->lnum == -1)
		return 1;
	if (iflg)
		printf("%5u ", p->lnum);
	if (sflg)
	printf("%d ", nblock(p->lsize));
	if (lflg) {
		putchar(p->ltype);
		pmode(p->lflags);
		printf("%2d ", p->lnl);
		t = p->luid;
		if(gflg)
			t = p->lgid;
		pw = getpwuid(t);
		if (pw->pw_name != NULL)
			printf("%-6.6s", pw->pw_name);
		else
			printf("%-6d", t);
		if (p->ltype=='b' || p->ltype=='c')
			printf("%3d,%3d", major((int)p->lsize), minor((int)p->lsize));
		else
			printf("%7ld", p->lsize);

//		struct stat st;
		struct tm tm;
		struct tm cm;

//		fstat(fd, &st);
		epoch_to_tm(p->lmtime, &tm);

		uint current_time = time(0);
		epoch_to_tm(current_time, &cm);

//		unsigned long epoch;
//		epoch_to_tm(time(&epoch), &cm);

		if(tm.tm_year + 1970 < cm.tm_year + 1970)
			printf(" %s %02d  %d ", month[tm.tm_mon], tm.tm_mday, tm.tm_year + 1970); else
			printf(" %s %02d %02d:%02d ", month[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min);

//	printf(" %s %d %02d:%02d ", month[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min);

	}
	if (p->lflags&ISARG)
		printf("%s\n", p->ln.namep);
	else
		printf("%s\n", p->ln.lname);
	return 0;
}

getname(uid, buf)
int uid;
char buf[];
{
	int j, c, n, i;

	if (uid==lastuid)
		return(0);
	if(pwdf == NULL)
		return(-1);
	rewind_pwdf();
	lastuid = -1;
	do {
		i = 0;
		j = 0;
		n = 0;
		while((c=getc(pwdf)) != '\n') {
			if (c==EOF)
				return(-1);
			if (c==':') {
				j++;
				c = '0';
			}
			if (j==0)
				buf[i++] = c;
			if (j==2)
				n = n*10 + c - '0';
		}
	} while (n != uid);
	buf[i++] = '\0';
	lastuid = uid;
	return(0);
}

long
nblock(size)
long size;
{
	return((size+511)>>9);
}

int	m1[] = { 1, S_IREAD>>0, 'r', '-' };
int	m2[] = { 1, S_IWRITE>>0, 'w', '-' };
int	m3[] = { 2, S_ISUID, 's', S_IEXEC>>0, 'x', '-' };
int	m4[] = { 1, S_IREAD>>3, 'r', '-' };
int	m5[] = { 1, S_IWRITE>>3, 'w', '-' };
int	m6[] = { 2, S_ISGID, 's', S_IEXEC>>3, 'x', '-' };
int	m7[] = { 1, S_IREAD>>6, 'r', '-' };
int	m8[] = { 1, S_IWRITE>>6, 'w', '-' };
int	m9[] = { 2, S_ISVTX, 't', S_IEXEC>>6, 'x', '-' };

int	*m[] = { m1, m2, m3, m4, m5, m6, m7, m8, m9};

pmode(aflag)
{
	register int **mp;

	flags = aflag;
	for (mp = &m[0]; mp < &m[sizeof(m)/sizeof(m[0])];)
		select(*mp++);
	return 0;
}

select(pairp)
register int *pairp;
{
	register int n;

	n = *pairp++;
	while (--n>=0 && (flags&*pairp++)==0)
		pairp++;
	putchar(*pairp);
	return 0;
}

char *
makename(dir, file)
char *dir, *file;
{
	static char dfile[100];
	register char *dp, *fp;
	register int i;

	dp = dfile;
	fp = dir;
	while (*fp)
		*dp++ = *fp++;
	*dp++ = '/';
	fp = file;
	for (i=0; i<DIRSIZ; i++)
		*dp++ = *fp++;
	*dp = 0;
	return(dfile);
}

readdir(dir)
char *dir;
{
    static struct dirent dentry;
    register int j;
    register struct lbuf *ep;
    int dirf;  // Local file descriptor to avoid conflicts

    if ((dirf = open(dir, O_RDONLY)) == -1) {
        printf("%s unreadable\n", dir);
        return 1;
    }
    tblocks = 0;
    for(;;) {
        if (read(dirf, (char *)&dentry, sizeof(dentry)) != sizeof(dentry))
            break;
        if (dentry.inum == 0
         || (aflg == 0 && dentry.name[0]=='.' &&
             (dentry.name[1]=='\0' ||
              (dentry.name[1]=='.' && dentry.name[2]=='\0'))))
            continue;
        ep = gstat(makename(dir, dentry.name), 0);
        if (ep==NULL)
            continue;
        for (j=0; j<DIRSIZ; j++)
            ep->ln.lname[j] = dentry.name[j];
    }
    fclose(dirf);
    return 0;
}

struct lbuf *
gstat(file, argfl)
char *file;
{
	struct stat statb;
	register struct lbuf *rep;
	static int nomocore;

	if (nomocore)
		return(NULL);
	rep = (struct lbuf *)malloc(sizeof(struct lbuf));
	if (rep==NULL) {
		fprintf(stderr, "ls: out of memory\n");
		nomocore = 1;
		return(NULL);
	}
	if (lastp >= &flist[NFILES]) {
		static int msg;
		lastp--;
		if (msg==0) {
			fprintf(stderr, "ls: too many files\n");
			msg++;
		}
	}
	*lastp++ = rep;
	rep->lflags = 0;
	rep->lnum = 0;
	rep->ltype = '-';
	if (argfl || statreq) {
		if (stat(file, &statb)<0) {
			perror(file);
			statb.st_ino = -1;
			statb.st_size = 0;
			statb.st_mode = 0;
			if (argfl) {
				lastp--;
				return(0);
			}
		}
		rep->lnum = statb.st_ino;
		rep->lsize = statb.st_size;
		switch(statb.st_mode&S_IFMT) {

		case S_IFDIR:
			rep->ltype = 'd';
			break;

		case S_IFBLK:
			rep->ltype = 'b';
			rep->lsize = statb.st_dev;
			break;

		case S_IFCHR:
			rep->ltype = 'c';
			rep->lsize = statb.st_dev;
			break;
		}
		rep->lflags = statb.st_mode & ~S_IFMT;
		rep->luid = statb.st_uid;
		rep->lgid = statb.st_gid;
		rep->lnl = statb.st_nlink;
/*		if(uflg)
			rep->lmtime = statb.st_atime;
		else if (cflg)
			rep->lmtime = statb.st_ctime;
		else
			rep->lmtime = statb.st_mtime;*/
		tblocks += nblock(statb.st_size);
		rep->lmtime = statb.st_lmtime;
		tblocks += nblock(statb.st_size);
	}
	return(rep);
}

compar(pp1, pp2)
struct lbuf **pp1, **pp2;
{
	register struct lbuf *p1, *p2;

	p1 = *pp1;
	p2 = *pp2;
	if (dflg==0) {
		if (p1->lflags&ISARG && p1->ltype=='d') {
			if (!(p2->lflags&ISARG && p2->ltype=='d'))
				return(1);
		} else {
			if (p2->lflags&ISARG && p2->ltype=='d')
				return(-1);
		}
	}
	if (tflg) {
		if(p2->lmtime == p1->lmtime)
			return(0);
		if(p2->lmtime > p1->lmtime)
			return(rflg);
		return(-rflg);
	}
	return(rflg * strcmp(p1->lflags&ISARG? p1->ln.namep: p1->ln.lname,
				p2->lflags&ISARG? p2->ln.namep: p2->ln.lname));
}

