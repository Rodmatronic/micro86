#include "../include/types.h"
#include "../include/stat.h"
#include "../include/stdio.h"
#include "../include/fs.h"
#include "../include/fcntl.h"

/*
 * Editor
 */

#define	FNSIZE	64
#define	LBSIZE	512
#define	ESIZE	128
#define	GBSIZE	256
#define	NBRA	5
#define	KSIZE	9

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#undef CEOF
#define	CEOF	11
#define	CKET	12
#define	CBACK	14

#define	STAR	01

char	Q[]	= "";
char	T[]	= "TMP";
#define	READ	0
#define	WRITE	1

int	peekc;
int	lastc;
char	savedfile[FNSIZE];
char	file[FNSIZE];
char	linebuf[LBSIZE];
char	rhsbuf[LBSIZE/2];
char	expbuf[ESIZE+4];
int	circfl;
int	*zero;
int	*dot;
int	*dol;
int	*addr1;
int	*addr2;
char	genbuf[LBSIZE];
long	count;
char	*nextip;
char	*linebp;
int	ninbuf;
int	io;
int	pflag;
void (*oldquit)(int);
void (*oldhup)(int);
int	vflag	= 1;
int	xflag;
int	xtflag;
int	kflag;
char	key[KSIZE + 1];
char	crbuf[512];
char	perm[768];
char	tperm[768];
int	listf;
int	col;
char	*globp;
int	tfile	= -1;
int	tline;
char	*tfname;
char	*loc1;
char	*loc2;
char	*locs;
char	ibuff[512];
int	iblock	= -1;
char	obuff[512];
int	oblock	= -1;
int	ichanged;
int	nleft;
char	WRERR[]	= "WRITE ERROR";
int	names[26];
int	anymarks;
char	*braslist[NBRA];
char	*braelist[NBRA];
int	nbra;
int	subnewa;
int	subolda;
int	fchange;
int	wrapp;
unsigned nlall = 128;

void newline();
void commands();
void filename(int comm);
void putfile();
void ederror(char* s);
void delete();
void callunix();
void rdelete(int* addr1, int* addr2);
void blkio(int iblock, char* ibuff, int (*iofcn)());
void init();
void makekey(char* a, char* b);
void global(int k);
void join();
void substitute(int inglob);
void dosub();
void move(int cflag);
void crblock(char* permp, char* buf, int nchar, long startn);
void putchr(int ac);
void edputs(register char* sp);
void putd();
void reverse(register int* a1, register int* a2);
void compile(int aeof);
void setdot();
void setall();
void setnoaddr();
void nonzero();
void exfile();

char *
mktemp(as)
char *as;
{
	register char *s;
	register unsigned pid;
	register i;

	pid = getpid();
	s = as;
	while (*s++)
		;
	s--;
	while (*--s == 'X') {
		*s = (pid%10) + '0';
		pid /= 10;
	}
	s++;
	i = 'a';
//	while (access(as, 0) != -1) {
		if (i=='z')
			return("/");
		*s = i++;
//	}
	return(as);
}

char *
strncpy (char *s1, const char *s2, size_t n)
{
  size_t size = strlen (s2);
  if (size != n)
    memset (s1 + size, '\0', n - size);
  return memmove (s1, s2, size);
}

int	*address();
char	*edgetline();
char	*getblock();
char	*place();
//jmp_buf	savej;
int 	savej;
char template[8] = "eXXXXXX";

void quit(int);
void onintr(int);
void onhup(int);

main(argc, argv)
char **argv;
{
	register char *p1, *p2;
//        void (*oldintr)(int);

	argv++;
	while (argc > 1 && **argv=='-') {
		switch((*argv)[1]) {

		case '\0':
			vflag = 0;
			break;

		case 'q':
			vflag = 1;
			break;

		case 'x':
			xflag = 1;
			break;
		}
		argv++;
		argc--;
	}
	if(xflag){
		edgetkey();
		kflag = crinit(key, perm);
	}

	if (argc>1) {
		p1 = *argv;
		p2 = savedfile;
		while ((*p2++ = *p1++))
			;
		globp = "r";
	}
	zero = (int *)malloc(nlall*sizeof(int));
	tfname = mktemp(template);
	init();
//	setjmp(savej);
	commands();
	quit(0);
}

void
commands()
{
	int getfile(), gettty();
	register *a1, c;

	for (;;) {
	if (pflag) {
		pflag = 0;
		addr1 = addr2 = dot;
		goto print;
	}
	addr1 = 0;
	addr2 = 0;
	do {
		addr1 = addr2;
		if ((a1 = address())==0) {
			c = getchr();
			break;
		}
		addr2 = a1;
		if ((c=getchr()) == ';') {
			c = ',';
			dot = a1;
		}
	} while (c==',');
	if (addr1==0)
		addr1 = addr2;
	switch(c) {

	case 'a':
		setdot();
		newline();
		append(gettty, addr2);
		continue;

	case 'c':
		delete();
		append(gettty, addr1-1);
		continue;

	case 'd':
		delete();
		continue;

	case 'E':
		fchange = 0;
		c = 'e';
	case 'e':
		setnoaddr();
		if (vflag && fchange) {
			fchange = 0;
			ederror(Q);
		}
		filename(c);
		init();
		addr2 = zero;
		goto caseread;

	case 'f':
		setnoaddr();
		filename(c);
		edputs(savedfile);
		continue;

	case 'g':
		global(1);
		continue;

	case 'i':
		setdot();
		nonzero();
		newline();
		append(gettty, addr2-1);
		continue;


	case 'j':
		if (addr2==0) {
			addr1 = dot;
			addr2 = dot+1;
		}
		setdot();
		newline();
		nonzero();
		join();
		continue;

	case 'k':
		if ((c = getchr()) < 'a' || c > 'z')
			ederror(Q);
		newline();
		setdot();
		nonzero();
		names[c-'a'] = *addr2 & ~01;
		anymarks |= 01;
		continue;

	case 'm':
		move(0);
		continue;

	case '\n':
		if (addr2==0)
			addr2 = dot+1;
		addr1 = addr2;
		goto print;

	case 'l':
		listf++;
	case 'p':
	case 'P':
		newline();
	print:
		setdot();
		nonzero();
		a1 = addr1;
		do {
			edputs(edgetline(*a1++));
		} while (a1 <= addr2);
		dot = addr2;
		listf = 0;
		continue;

	case 'Q':
		fchange = 0;
	case 'q':
		setnoaddr();
		newline();
		quit(0);

	case 'r':
		filename(c);
	caseread:
//		if ((io = open(file, 0)) < 0) {
                if ((io = open(file, O_CREATE | O_RDWR)) < 0) {
			lastc = '\n';
			ederror(file);
		}
		setall();
		ninbuf = 0;
		c = zero != dol;
		append(getfile, addr2);
		exfile();
		fchange = c;
		continue;

	case 's':
		setdot();
		nonzero();
		substitute(globp!=0);
		continue;

	case 't':
		move(1);
		continue;

	case 'u':
		setdot();
		nonzero();
		newline();
		if ((*addr2&~01) != subnewa)
			ederror(Q);
		*addr2 = subolda;
		dot = addr2;
		continue;

	case 'v':
		global(0);
		continue;

	case 'W':
		wrapp++;
	case 'w':
		setall();
		nonzero();
		filename(c);
		if (wrapp) {
			if ((io = open(file, O_RDWR)) >= 0) {
				lseek(io, 0, 2);
			} else {
				if ((io = open(file, O_CREATE | O_RDWR)) < 0)
					ederror(file);
			}
		} else {
			if (io >= 0) close(io);
			unlink(file);
			if ((io = open(file, O_CREATE | O_RDWR)) < 0)
				ederror(file);
		}
		wrapp = 0;
		putfile();
		exfile();
		if (addr1==zero+1 && addr2==dol)
			fchange = 0;
		continue;
	case 'x':
		setnoaddr();
		newline();
		xflag = 1;
		edputs("Entering encrypting mode!");
		edgetkey();
		kflag = crinit(key, perm);
		continue;

	case '=':
		setall();
		newline();
		count = (addr2-zero)&077777;
		putd();
		putchr('\n');
		continue;

	case '!':
		callunix();
		continue;

	case EOF:
		return;

	}
	ederror(Q);
	}
}

int *
address()
{
	register *a1, minus, c;
	int n, relerr;

	minus = 0;
	a1 = 0;
	for (;;) {
		c = getchr();
		if ('0'<=c && c<='9') {
			n = 0;
			do {
				n *= 10;
				n += c - '0';
			} while ((c = getchr())>='0' && c<='9');
			peekc = c;
			if (a1==0)
				a1 = zero;
			if (minus<0)
				n = -n;
			a1 += n;
			minus = 0;
			continue;
		}
		relerr = 0;
		if (a1 || minus)
			relerr++;
		switch(c) {
		case ' ':
		case '\t':
			continue;

		case '+':
			minus++;
			if (a1==0)
				a1 = dot;
			continue;

		case '-':
		case '^':
			minus--;
			if (a1==0)
				a1 = dot;
			continue;

		case '?':
		case '/':
			compile(c);
			a1 = dot;
			for (;;) {
				if (c=='/') {
					a1++;
					if (a1 > dol)
						a1 = zero;
				} else {
					a1--;
					if (a1 < zero)
						a1 = dol;
				}
				if (execute(0, a1))
					break;
				if (a1==dot)
					ederror(Q);
			}
			break;

		case '$':
			a1 = dol;
			break;

		case '.':
			a1 = dot;
			break;

		case '\'':
			if ((c = getchr()) < 'a' || c > 'z')
				ederror(Q);
			for (a1=zero; a1<=dol; a1++)
				if (names[c-'a'] == (*a1 & ~01))
					break;
			break;

		default:
			peekc = c;
			if (a1==0)
				return(0);
			a1 += minus;
			if (a1<zero || a1>dol)
				ederror(Q);
			return(a1);
		}
		if (relerr)
			ederror(Q);
	}
}

void
setdot()
{
	if (addr2 == 0)
		addr1 = addr2 = dot;
	if (addr1 > addr2)
		ederror(Q);
}

void
setall()
{
	if (addr2==0) {
		addr1 = zero+1;
		addr2 = dol;
		if (dol==zero)
			addr1 = zero;
	}
	setdot();
}

void
setnoaddr()
{
	if (addr2)
		ederror(Q);
}

void
nonzero()
{
	if (addr1<=zero || addr2>dol)
		ederror(Q);
}

void
newline()
{
	register c;

	if ((c = getchr()) == '\n')
		return;
	if (c=='p' || c=='l') {
		pflag++;
		if (c=='l')
			listf++;
		if (getchr() == '\n')
			return;
	}
	ederror(Q);
}

void
filename(int comm)
{
	register char *p1, *p2;
	register c;

	count = 0;
	c = getchr();
	if (c=='\n' || c==EOF) {
		p1 = savedfile;
		if (*p1==0 && comm!='f')
			ederror(Q);
		p2 = file;
		while ((*p2++ = *p1++))
			;
		return;
	}
	if (c!=' ')
		ederror(Q);
	while ((c = getchr()) == ' ')
		;
	if (c=='\n')
		ederror(Q);
	p1 = file;
	do {
		*p1++ = c;
		if (c==' ' || c==EOF)
			ederror(Q);
	} while ((c = getchr()) != '\n');
	*p1++ = 0;
	if (savedfile[0]==0 || comm=='e' || comm=='f') {
		p1 = savedfile;
		p2 = file;
		while ((*p1++ = *p2++))
			;
	}
}

void
exfile()
{
	close(io);
	io = -1;
	if (vflag) {
		putd();
		putchr('\n');
	}
}

void
onintr(int _x)
{
	putchr('\n');
	lastc = '\n';
	ederror(Q);
}

void
onhup(int _x)
{
	if (dol > zero) {
		addr1 = zero+1;
		addr2 = dol;
		io = open("ed.hup", O_CREATE | O_RDWR);
		if (io > 0)
			putfile();
	}
	fchange = 0;
	quit(0);
}

void
ederror(s)
char *s;
{
	register c;

	wrapp = 0;
	listf = 0;
	putchr('?');
	edputs(s);
	count = 0;
	lseek(0, (long)0, 2);
	pflag = 0;
	if (globp)
		lastc = '\n';
	globp = 0;
	peekc = lastc;
	if(lastc)
		while ((c = getchr()) != '\n' && c != EOF)
			;
	if (io > 0) {
		close(io);
		io = -1;
	}
//	longjmp(savej, 1);
}

int
getchr()
{
	char c;
	if ((lastc=peekc)) {
		peekc = 0;
		return(lastc);
	}
	if (globp) {
		if ((lastc = *globp++) != 0)
			return(lastc);
		globp = 0;
		return(EOF);
	}
	if (read(0, &c, 1) <= 0)
		return(lastc = EOF);
	lastc = c&0177;
	return(lastc);
}

int
gettty()
{
	register c;
	register char *gf;
	register char *p;

	p = linebuf;
	gf = globp;
	while ((c = getchr()) != '\n') {
		if (c==EOF) {
			if (gf)
				peekc = c;
			return(c);
		}
		if ((c &= 0177) == 0)
			continue;
		*p++ = c;
		if (p >= &linebuf[LBSIZE-2])
			ederror(Q);
	}
	*p++ = 0;
	if (linebuf[0]=='.' && linebuf[1]==0)
		return(EOF);
	return(0);
}

int
getfile()
{
	register c;
	register char *lp, *fp;

	lp = linebuf;
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			if ((ninbuf = read(io, genbuf, LBSIZE)-1) < 0)
				return(EOF);
			fp = genbuf;
			while(fp < &genbuf[ninbuf]) {
				if (*fp++ & 0200) {
					if (kflag)
						crblock(perm, genbuf, ninbuf+1, count);
					break;
				}
			}
			fp = genbuf;
		}
		c = *fp++;
		if (c=='\0')
			continue;
		if (c&0200 || lp >= &linebuf[LBSIZE]) {
			lastc = '\n';
			ederror(Q);
		}
		*lp++ = c;
		count++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return(0);
}

void
putfile()
{
	int *a1, n;
	register char *fp, *lp;
	register nib;

	nib = 512;
	fp = genbuf;
	a1 = addr1;
	do {
		lp = edgetline(*a1++);
		for (;;) {
			if (--nib < 0) {
				n = fp-genbuf;
				if(kflag)
					crblock(perm, genbuf, n, count-n);
				if(write(io, genbuf, n) != n) {
					edputs(WRERR);
					ederror(Q);
				}
				nib = 511;
				fp = genbuf;
			}
			count++;
			if ((*fp++ = *lp++) == 0) {
				fp[-1] = '\n';
				break;
			}
		}
	} while (a1 <= addr2);
	n = fp-genbuf;
	if(kflag)
		crblock(perm, genbuf, n, count-n);
	if(write(io, genbuf, n) != n) {
		edputs(WRERR);
		ederror(Q);
	}
}

int
append(f, a)
int *a;
int (*f)();
{
    register *a1, *a2, *rdot;
    int nline, tl;
    int *new_zero;

    nline = 0;
    dot = a;
    while ((*f)() == 0) {
        if ((dol - zero) + 1 >= nlall) {
            int *ozero = zero;
            int offset = dot - zero;
            int doffset = dol - zero;
            nlall += 512;
            new_zero = malloc(nlall * sizeof(int));
            
            if (new_zero == NULL) {
                lastc = '\n';
                ederror("MEM?");
            }
            
            for (int i = 0; i <= doffset; i++) {
                new_zero[i] = ozero[i];
            }
            
            dot = new_zero + offset;
            dol = new_zero + doffset;
            free(ozero);
            zero = new_zero;
        }
        tl = edputline();
        nline++;
        a1 = ++dol;
        a2 = a1 + 1;
        rdot = ++dot;
        while (a1 > rdot) {
            *--a2 = *--a1;
        }
        *rdot = tl;
    }
    return(nline);
}

void
callunix()
{
//	register void (*savint)(int);
        int pid, rpid;
	int retcode;

	setnoaddr();
	if ((pid = fork()) == 0) {
		execl("/bin/sh", "sh", "-t", NULL);
		exit(0100);
	}
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;
	edputs("!");
}

void
quit(int _dummy)
{
	if (vflag && fchange && dol!=zero) {
		fchange = 0;
		ederror(Q);
	}
	unlink(tfname);
	exit(0);
}

void
delete()
{
	setdot();
	newline();
	nonzero();
	rdelete(addr1, addr2);
}

void
rdelete(ad1, ad2)
int *ad1, *ad2;
{
	register *a1, *a2, *a3;

	a1 = ad1;
	a2 = ad2+1;
	a3 = dol;
	dol -= a2 - a1;
	do {
		*a1++ = *a2++;
	} while (a2 <= a3);
	a1 = ad1;
	if (a1 > dol)
		a1 = dol;
	dot = a1;
	fchange = 1;
}

void
gdelete()
{
	register *a1, *a2, *a3;

	a3 = dol;
	for (a1=zero+1; (*a1&01)==0; a1++)
		if (a1>=a3)
			return;
	for (a2=a1+1; a2<=a3;) {
		if (*a2&01) {
			a2++;
			dot = a1;
		} else
			*a1++ = *a2++;
	}
	dol = a1-1;
	if (dot>dol)
		dot = dol;
	fchange = 1;
}

char *
edgetline(tl)
{
	register char *bp, *lp;
	register nl;

	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~0377;
	while ((*lp++ = *bp++))
		if (--nl == 0) {
			bp = getblock(tl+=0400, READ);
			nl = nleft;
		}
	return(linebuf);
}

int
edputline()
{
	register char *bp, *lp;
	register nl;
	int tl;

	fchange = 1;
	lp = linebuf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl &= ~0377;
	while ((*bp = *lp++)) {
		if (*bp++ == '\n') {
			*--bp = 0;
			linebp = lp;
			break;
		}
		if (--nl == 0) {
			bp = getblock(tl+=0400, WRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline += (((lp-linebuf)+03)>>1)&077776;
	return(nl);
}

char *
getblock(atl, iof)
{
	register bno, off;
	register char *p1, *p2;
	register int n;

	bno = (atl>>8)&0377;
	off = (atl<<1)&0774;
	if (bno >= 255) {
		lastc = '\n';
		ederror(T);
	}
	nleft = 512 - off;
	if (bno==iblock) {
		ichanged |= iof;
		return(ibuff+off);
	}
	if (bno==oblock)
		return(obuff+off);
	if (iof==READ) {
		if (ichanged) {
			if(xtflag)
				crblock(tperm, ibuff, 512, (long)0);
			blkio(iblock, ibuff, write);
		}
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, read);
		if(xtflag)
			crblock(tperm, ibuff, 512, (long)0);
		return(ibuff+off);
	}
	if (oblock>=0) {
		if(xtflag) {
			p1 = obuff;
			p2 = crbuf;
			n = 512;
			while(n--)
				*p2++ = *p1++;
			crblock(tperm, crbuf, 512, (long)0);
			blkio(oblock, crbuf, write);
		} else
			blkio(oblock, obuff, write);
	}
	oblock = bno;
	return(obuff+off);
}

void
blkio(b, buf, iofcn)
char *buf;
int (*iofcn)();
{
	lseek(tfile, (long)b<<9, 0);
	if ((*iofcn)(tfile, buf, 512) != 512) {
		ederror(T);
	}
}

void
init()
{
	register *markp;

	close(tfile);
	tline = 2;
	for (markp = names; markp < &names[26]; )
		*markp++ = 0;
	subnewa = 0;
	anymarks = 0;
	iblock = -1;
	oblock = -1;
	ichanged = 0;
	close(open(tfname, O_CREATE | O_RDWR));
	tfile = open(tfname, 2);
	if(xflag) {
		xtflag = 1;
		makekey(key, tperm);
	}
	dot = dol = zero;
}

void
global(k)
{
	register char *gp;
	register c;
	register int *a1;
	char globuf[GBSIZE];

	if (globp)
		ederror(Q);
	setall();
	nonzero();
	if ((c=getchr())=='\n')
		ederror(Q);
	compile(c);
	gp = globuf;
	while ((c = getchr()) != '\n') {
		if (c==EOF)
			ederror(Q);
		if (c=='\\') {
			c = getchr();
			if (c!='\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2])
			ederror(Q);
	}
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			*a1 |= 01;
	}
	/*
	 * Special case: g/.../d (avoid n^2 algorithm)
	 */
	if (globuf[0]=='d' && globuf[1]=='\n' && globuf[2]=='\0') {
		gdelete();
		return;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands();
			a1 = zero;
		}
	}
}

void
join()
{
	register char *gp, *lp;
	register *a1;

	gp = genbuf;
	for (a1=addr1; a1<=addr2; a1++) {
		lp = edgetline(*a1);
		while ((*gp = *lp++))
			if (gp++ >= &genbuf[LBSIZE-2])
				ederror(Q);
	}
	lp = linebuf;
	gp = genbuf;
	while ((*lp++ = *gp++))
		;
	*addr1 = edputline();
	if (addr1<addr2)
		rdelete(addr1+1, addr2);
	dot = addr1;
}

void
substitute(inglob)
{
	register *markp, *a1, nl;
	int gsubf;
	int getsub();

	gsubf = compsub();
	for (a1 = addr1; a1 <= addr2; a1++) {
		int *ozero;
		if (execute(0, a1)==0)
			continue;
		inglob |= 01;
		dosub();
		if (gsubf) {
			while (*loc2) {
				if (execute(1, (int *)0)==0)
					break;
				dosub();
			}
		}
		subnewa = edputline();
		*a1 &= ~01;
		if (anymarks) {
			for (markp = names; markp < &names[26]; markp++)
				if (*markp == *a1)
					*markp = subnewa;
		}
		subolda = *a1;
		*a1 = subnewa;
		ozero = zero;
		nl = append(getsub, a1);
		nl += zero-ozero;
		a1 += nl;
		addr2 += nl;
	}
	if (inglob==0)
		ederror(Q);
}

int
compsub()
{
	register seof, c;
	register char *p;

	if ((seof = getchr()) == '\n' || seof == ' ')
		ederror(Q);
	compile(seof);
	p = rhsbuf;
	for (;;) {
		c = getchr();
		if (c=='\\')
			c = getchr() | 0200;
		if (c=='\n') {
			if (globp)
				c |= 0200;
			else
				ederror(Q);
		}
		if (c==seof)
			break;
		*p++ = c;
		if (p >= &rhsbuf[LBSIZE/2])
			ederror(Q);
	}
	*p++ = 0;
	if ((peekc = getchr()) == 'g') {
		peekc = 0;
		newline();
		return(1);
	}
	newline();
	return(0);
}

int
getsub()
{
	register char *p1, *p2;

	p1 = linebuf;
	if ((p2 = linebp) == 0)
		return(EOF);
	while ((*p1++ = *p2++))
		;
	linebp = 0;
	return(0);
}

void
dosub()
{
	register char *lp, *sp, *rp;
	int c;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while ((c = *rp++&0377)) {
		if (c=='&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if (c&0200 && (c &= 0177) >='1' && c < nbra+'1') {
			sp = place(sp, braslist[c-'1'], braelist[c-'1']);
			continue;
		}
		*sp++ = c&0177;
		if (sp >= &genbuf[LBSIZE])
			ederror(Q);
	}
	lp = loc2;
	loc2 = sp - genbuf + linebuf;
	while ((*sp++ = *lp++))
		if (sp >= &genbuf[LBSIZE])
			ederror(Q);
	lp = linebuf;
	sp = genbuf;
	while ((*lp++ = *sp++))
		;
}

char *
place(sp, l1, l2)
register char *sp, *l1, *l2;
{

	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LBSIZE])
			ederror(Q);
	}
	return(sp);
}

void
move(cflag)
{
	register int *adt, *ad1, *ad2;
	int getcopy();

	setdot();
	nonzero();
	if ((adt = address())==0)
		ederror(Q);
	newline();
	if (cflag) {
		int *ozero, delta;
		ad1 = dol;
		ozero = zero;
		append(getcopy, ad1++);
		ad2 = dol;
		delta = zero - ozero;
		ad1 += delta;
		adt += delta;
	} else {
		ad2 = addr2;
		for (ad1 = addr1; ad1 <= ad2;)
			*ad1++ &= ~01;
		ad1 = addr1;
	}
	ad2++;
	if (adt<ad1) {
		dot = adt + (ad2-ad1);
		if ((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else if (adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		ederror(Q);
	fchange = 1;
}

void
reverse(a1, a2)
register int *a1, *a2;
{
	register int t;

	for (;;) {
		t = *--a2;
		if (a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

int
getcopy()
{
	if (addr1 > addr2)
		return(EOF);
	edgetline(*addr1++);
	return(0);
}

void
compile(aeof)
{
	register eof, c;
	register char *ep;
	char *lastep;
	char bracket[NBRA], *bracketp;
	int cclcnt;

	ep = expbuf;
	eof = aeof;
	bracketp = bracket;
	if ((c = getchr()) == eof) {
		if (*ep==0)
			ederror(Q);
		return;
	}
	circfl = 0;
	nbra = 0;
	if (c=='^') {
		c = getchr();
		circfl++;
	}
	peekc = c;
	lastep = 0;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			goto cerror;
		c = getchr();
		if (c==eof) {
			if (bracketp != bracket)
				goto cerror;
			*ep++ = CEOF;
			return;
		}
		if (c!='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if ((c = getchr())=='(') {
				if (nbra >= NBRA)
					goto cerror;
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket)
					goto cerror;
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			if (c>='1' && c<'1'+NBRA) {
				*ep++ = CBACK;
				*ep++ = c-'1';
				continue;
			}
			*ep++ = CCHR;
			if (c=='\n')
				goto cerror;
			*ep++ = c;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			goto cerror;

		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '$':
			if ((peekc=getchr()) != eof)
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=getchr()) == '^') {
				c = getchr();
				ep[-2] = NCCL;
			}
			do {
				if (c=='\n')
					goto cerror;
				if (c=='-' && ep[-1]!=0) {
					if ((c=getchr())==']') {
						*ep++ = '-';
						cclcnt++;
						break;
					}
					while (ep[-1]<c) {
						*ep = ep[-1]+1;
						ep++;
						cclcnt++;
						if (ep>=&expbuf[ESIZE])
							goto cerror;
					}
				}
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE])
					goto cerror;
			} while ((c = getchr()) != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
   cerror:
	expbuf[0] = 0;
	nbra = 0;
	ederror(Q);
}

int
execute(gf, addr)
int *addr;
{
	register char *p1, *p2, c;

	for (c=0; c<NBRA; c++) {
		braslist[c] = 0;
		braelist[c] = 0;
	}
	if (gf) {
		if (circfl)
			return(0);
		p1 = linebuf;
		p2 = genbuf;
		while ((*p1++ = *p2++))
			;
		locs = p1 = loc2;
	} else {
		if (addr==zero)
			return(0);
		p1 = edgetline(*addr);
		locs = 0;
	}
	p2 = expbuf;
	if (circfl) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* fast check for first character */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
	/* regular algorithm */
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

int
advance(lp, ep)
register char *ep, *lp;
{
	register char *curlp;
	int i;

	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		if (braelist[i = *ep++]==0)
			ederror(Q);
		if (backref(i, lp)) {
			lp += braelist[i] - braslist[i];
			continue;
		}
		return(0);

	case CBACK|STAR:
		if (braelist[i = *ep++] == 0)
			ederror(Q);
		curlp = lp;
		while (backref(i, lp))
			lp += braelist[i] - braslist[i];
		while (lp >= curlp) {
			if (advance(lp, ep))
				return(1);
			lp -= braelist[i] - braslist[i];
		}
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++)
			;
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep)
			;
		ep++;
		goto star;

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)))
			;
		ep += *ep;
		goto star;

	star:
		do {
			lp--;
			if (lp==locs)
				break;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		ederror(Q);
	}
}

int
backref(i, lp)
register i;
register char *lp;
{
	register char *bp;

	bp = braslist[i];
	while (*bp++ == *lp++)
		if (bp >= braelist[i])
			return(1);
	return(0);
}

int
cclass(set, c, af)
register char *set, c;
{
	register n;

	if (c==0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}

void
putd()
{
	register r;

	r = count%10;
	count /= 10;
	if (count)
		putd();
	putchr(r + '0');
}

void
edputs(sp)
register char *sp;
{
	col = 0;
	while (*sp)
		putchr(*sp++);
	putchr('\n');
}

char	line[70];
char	*linp	= line;

void
putchr(ac)
{
	register char *lp;
	register c;

	lp = linp;
	c = ac;
	if (listf) {
		col++;
		if (col >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		if (c=='\t') {
			c = '>';
			goto esc;
		}
		if (c=='\b') {
			c = '<';
		esc:
			*lp++ = '-';
			*lp++ = '\b';
			*lp++ = c;
			goto out;
		}
		if (c<' ' && c!= '\n') {
			*lp++ = '\\';
			*lp++ = (c>>3)+'0';
			*lp++ = (c&07)+'0';
			col += 2;
			goto out;
		}
	}
	*lp++ = c;
out:
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(1, line, lp-line);
		return;
	}
	linp = lp;
}

void
crblock(permp, buf, nchar, startn)
char *permp;
char *buf;
long startn;
{
	register char *p1;
	int n1;
	int n2;
	register char *t1, *t2, *t3;

	t1 = permp;
	t2 = &permp[256];
	t3 = &permp[512];

	n1 = startn&0377;
	n2 = (startn>>8)&0377;
	p1 = buf;
	while(nchar--) {
		*p1 = t2[(t3[(t1[(*p1+n1)&0377]+n2)&0377]-n2)&0377]-n1;
		n1++;
		if(n1==256){
			n1 = 0;
			n2++;
			if(n2==256) n2 = 0;
		}
		p1++;
	}
}

int
edgetkey()
{
//	void (*sig)(int);
	register char *p;
	register c;

	puts("Key:");
	p = key;
	while(((c=getchr()) != EOF) && (c!='\n')) {
		if(p < &key[KSIZE])
			*p++ = c;
	}
	*p = 0;
	return(key[0] != 0);
}

/*
 * Besides initializing the encryption machine, this routine
 * returns 0 if the key is null, and 1 if it is non-null.
 */
int
crinit(keyp, permp)
char	*keyp, *permp;
{
	register char *t1, *t2, *t3;
	register i;
	int ic, k, temp, pf[2];
	unsigned random;
	char buf[13];
	long seed;

	t1 = permp;
	t2 = &permp[256];
	t3 = &permp[512];
	if(*keyp == 0)
		return(0);
	strncpy(buf, keyp, 8);
	while (*keyp)
		*keyp++ = '\0';
	buf[8] = buf[0];
	buf[9] = buf[1];
	if (pipe(pf)<0)
		pf[0] = pf[1] = -1;
	if (fork()==0) {
		close(0);
		close(1);
		dup(pf[0]);
		dup(pf[1]);
//		execl("/usr/lib/makekey", "-", NULL);
//		execl("/lib/makekey", "-", NULL);
		exit(1);
	}
	write(pf[1], buf, 10);
	if (wait((int *)NULL)==-1 || read(pf[0], buf, 13)!=13)
		ederror("crypt: cannot generate key");
	close(pf[0]);
	close(pf[1]);
	seed = 123;
	for (i=0; i<13; i++)
		seed = seed*buf[i] + i;
	for(i=0;i<256;i++){
		t1[i] = i;
		t3[i] = 0;
	}
	for(i=0; i<256; i++) {
		seed = 5*seed + buf[i%13];
		random = seed % 65521;
		k = 256-1 - i;
		ic = (random&0377) % (k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0) continue;
		ic = (random&0377) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0; i<256; i++)
		t2[t1[i]&0377] = i;
	return(1);
}

void
makekey(a, b)
char *a, *b;
{
	register int i;
	long t;
	char temp[KSIZE + 1];

	for(i = 0; i < KSIZE; i++)
		temp[i] = *a++;
	time(&t);
	t += getpid();
	for(i = 0; i < 4; i++)
		temp[i] ^= (t>>(8*i))&0377;
	crinit(temp, b);
}

