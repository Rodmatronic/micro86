#include "../include/types.h"
#include "../include/stat.h"
#include "../include/fcntl.h"
#include "../include/user.h"
#include "../include/x86.h"
#include "../include/tty.h"
#include "../include/stdarg.h"
#include "../include/pwd.h"

static char PASSWD[]	= "/etc/passwd";
static char EMPTY[] = "";
static FILE *pwf = NULL;
static char line[BUFSIZ+1];
static struct passwd passwd;

#define ECHO 010

char *
getpass(char *prompt)
{
  static char pwbuf[128];
  char *p;
  int c;
  struct ttyb ttyb;

  write(1, prompt, strlen(prompt));
  ttyb.tflags &= ~ECHO;
  stty(&ttyb);

  p = pwbuf;
  while ((c = getchar()) != '\n') {
    if (c <= 0) {
      pwbuf[0] = '\0';
      break;
    }
    if (p < pwbuf + sizeof(pwbuf) - 1)
      *p++ = c;
  }
  *p = '\0';
  ttyb.tflags |= ECHO;
  stty(&ttyb);
  write(1, "\n", 1);

  return pwbuf;
}

rewind(iop)
{
	lseek(iop, 0L, 0);
	return 0;
}

void setpwent(void)
{
    if (pwf) {
        rewind(pwf);
        return;
    }
    pwf = open(PASSWD, O_RDONLY);
}

void endpwent(void)
{
    if (pwf) {
        fclose(pwf);
        pwf = NULL;
    }
}

static char *pwskip(char *p) {
    while (*p && *p != ':')
        ++p;
    if (*p) *p++ = '\0';
    return p;
}

struct passwd *getpwent(void)
{
    char *p;

    if (!pwf) {
        pwf = open(PASSWD, O_RDONLY);
        if (!pwf) return NULL;
    }

    if (!fgets(line, sizeof line, pwf))
        return NULL;

    p = line;
    char *nl = strchr(p, '\n');
    if (nl) *nl = '\0';

    // name : passwd : uid : gid : gecos : dir : shell
    passwd.pw_name   = p;  p = pwskip(p);
    passwd.pw_passwd = p;  p = pwskip(p);
    passwd.pw_uid    = atoi(p); p = pwskip(p);
    passwd.pw_gid    = atoi(p); p = pwskip(p);
    passwd.pw_quota  = 0;
    passwd.pw_comment= EMPTY;
    passwd.pw_gecos  = p;  p = pwskip(p);
    passwd.pw_dir    = p;  p = pwskip(p);
    passwd.pw_shell  = p;  // last field; may be empty

    return &passwd;
}

char *
getlogin(void)
{
  static char name[32];   // buffer for username
  struct passwd *pw;
  int uid;

  uid = getuid();
  setpwent();             // rewind passwd file

  while((pw = getpwent()) != 0){
    if(pw->pw_uid == uid){
      strcpy(name, pw->pw_name);
      endpwent();
      return name;
    }
  }

  endpwent();
  return 0;   // not found
}

#define ERR(s, c)	if(opterr){\
	(void) puts(argv[0]);\
	(void) puts(s);\
	(void) putchar(c);\
	(void) putchar('\n');}

int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg;

int seek(int fd, int offset, int whence) {
    return lseek(fd, offset, whence);
}

int execl(char *path, const char *arg0, ...) {
  va_list ap;
  const char *argv[512];
  int argc = 0;

  argv[argc++] = arg0;
  va_start(ap, arg0);
  while (argc < 512) {
    const char *arg = va_arg(ap, const char *);
    if (!arg) break;
    argv[argc++] = arg;
  }
  va_end(ap);
  argv[argc] = 0;

  return exec(path, (char **)argv);
}

int
getopt(argc, argv, opts)
int	argc;
char	**argv, *opts;
{
	static int sp = 1;
	register int c;
	register char *cp;

	if(sp == 1)
		if(optind >= argc ||
		   argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(EOF);
		else if(strcmp(argv[optind], "--") == 0) {
			optind++;
			return(EOF);
		}
	optopt = c = argv[optind][sp];
	if(c == ':' || (cp=strchr(opts, c)) == NULL) {
		ERR(": illegal option -- ", c);
		if(argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		return('?');
	}
	if(*++cp == ':') {
		if(argv[optind][sp+1] != '\0')
			optarg = &argv[optind++][sp+1];
		else if(++optind >= argc) {
			ERR(": option requires an argument -- ", c);
			sp = 1;
			return('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if(argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return(c);
}

char *
strtok(char *s, const char *delim)
{
	static char *last;
	return strtok_r(s, delim, &last);
}
char *
strtok_r(char *s, const char *delim, char **last)
{
	char *spanp;
	int c, sc;
	char *tok;
	if (s == NULL && (s = *last) == NULL)
		return (NULL);
	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}
	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;
	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*last = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

char *
strdup (const char *s) {
  size_t len = strlen (s) + 1;
  void *new = malloc (
len);
  if (new == NULL)
    return NULL;
  return (char *) memmove (new, s, len);
}

int
abs (int i)
{
  return i < 0 ? -i : i;
}

char *
strcat(char *s1, const char *s2)
{	/* copy char s2[] to end of s1[] */
	char *s;
	for (s = s1; *s != '\0'; ++s)
		;			/* find end of s1[] */
	for (; (*s = *s2) != '\0'; ++s, ++s2)
		;			/* copy s2[] to end */
	return (s1);
}

int
toupper(int c)
{
  return islower (c) ? c - 'a' + 'A' : c;
}

int
islower(int c)
{
    return c >= 'a' && c <= 'z' ? 0x02 : 0;
}

char *
strrchr (register const char *s, int c)
{
  char *rtnval = 0;

  do {
    if (*s == c)
      rtnval = (char*) s;
  } while (*s++);
  return (rtnval);
}

static int is_leap_year(int year) {
    if (year % 4)   return 0;
    if (year % 100) return 1;
    if (year % 400) return 0;
    return 1;
}

struct tm *
localtime(const unsigned long *timer) {
    static struct tm tm;
    unsigned long t = *timer;
    int year, month;
    const int days_per_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    tm.tm_sec = t % 60;
    t /= 60;
    tm.tm_min = t % 60;
    t /= 60;
    tm.tm_hour = t % 24;
    t /= 24;

    // Calculate day of week (1970-01-01 was Thursday)
    tm.tm_wday = (t + 4) % 7;

    year = 1970;
    while (1) {
        int days_in_year = is_leap_year(year) ? 366 : 365;
        if (t < days_in_year) break;
        t -= days_in_year;
        year++;
    }
    tm.tm_year = year - 1970;

    int leap = is_leap_year(year);
    for (month = 0; month < 12; month++) {
        int days = days_per_month[month];
        if (month == 1 && leap) days++; // February leap day
        
        if (t < days) break;
        t -= days;
    }
    tm.tm_mon = month;
    tm.tm_mday = t + 1;

    return &tm;
}

int
isdigit(int c)
{
	return (unsigned)c - '0' < 10;
}

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

int
getchar()
{
  char c;
  if (read(0, &c, 1) <= 0)
    return -1;
  return (unsigned char)c;
}

int
getc(int fd)
{
  char c;
  if (read(fd, &c, 1) <= 0) // read returns 0 on EOF or -1 on error
    return -1;               // Hence, -1 indicates EOF/error
  return (unsigned char)c;
}

char*
fgets(char *buf, int max, int fd)
{
  int i, cc;
  char c;

  for(i = 0; i+1 < max; ){
    cc = read(fd, &c, 1);
    if(cc < 1){
      if(i == 0)
        return 0;   // EOF with no data
      break;
    }
    buf[i++] = c;
    if(c == '\n')
      break;
  }
  buf[i] = '\0';
  return buf;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

int
fclose(int fd)
{
  return close(fd);
}
