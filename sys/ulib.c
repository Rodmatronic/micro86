#include "../include/types.h"
#include "../include/stat.h"
#include "../include/fcntl.h"
#include "../include/user.h"
#include "../include/x86.h"
#include "../include/tty.h"
#include "../include/stdarg.h"
#include "../include/pwd.h"
#include "../include/errno.h"
#include "../include/fs.h"

static char PASSWD[]	= "/etc/passwd";
static char EMPTY[] = "";
static FILE *pwf = NULL;
static char line[BUFSIZ+1];
static struct passwd passwd;
#define ECHO 010

char* getcwd() {
    static char cwd[512];  // Returned buffer
    int file;
    int rdev, rino;
    char ddot[] = ".";
    char dotdot[] = "..";
    char name[512];
    int off = -1;
    struct stat d, dd;
    struct dirent dir;

    stat("/", &d);
    rdev = d.dev;
    rino = d.ino;

    for (;;) {
        stat(ddot, &d);
        if (d.ino == rino && d.dev == rdev) {
            if (off < 0)
                off = 0;
            name[off] = '\0';
            cwd[0] = '/';
            for (int i = 0; i <= off; i++)
                cwd[i + 1] = name[i];
            return cwd;
        }

        if ((file = open(dotdot, 0)) < 0) {
            return 0; // error
        }

        stat(file, &dd);
        chdir(dotdot);

        if (d.dev == dd.dev) {
            if (d.ino == dd.ino) {
                if (off < 0)
                    off = 0;
                name[off] = '\0';
                cwd[0] = '/';
                for (int i = 0; i <= off; i++)
                    cwd[i + 1] = name[i];
                close(file);
                return cwd;
            }
            do {
                if (read(file, (char *)&dir, sizeof(dir)) < sizeof(dir)) {
                    close(file);
                    return 0;
                }
            } while (dir.inum != d.ino);
        } else {
            do {
                if (read(file, (char *)&dir, sizeof(dir)) < sizeof(dir)) {
                    close(file);
                    return 0;
                }
                stat(dir.name, &dd);
            } while (dd.ino != d.ino || dd.dev != d.dev);
        }

        close(file);
        int i = -1;
        while (dir.name[++i] != 0);
        if ((off + i + 2) > 511) {
            return 0; // path too long
        }
        for (int j = off + 1; j >= 0; --j)
            name[j + i + 1] = name[j];
        off = i + off + 1;
        name[i] = '/';
        for (--i; i >= 0; --i)
            name[i] = dir.name[i];
    }
}

char *
rindex(p, ch)
	const char *p;
	int ch;
{
	union {
		const char *cp;
		char *p;
	} u;
	char *save;

	u.cp = p;
	for (save = NULL;; ++u.p) {
		if (*u.p == ch)
			save = u.p;
		if (*u.p == '\0')
			return(save);
	}
	/* NOTREACHED */
}

int	errno = 0;
int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg = NULL;

enum {
    no_argument = 0,
    required_argument = 1,
    optional_argument = 2
};

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

static void ERR(const char *str, int c) {
    fprintf(stderr, "%s%c\n", str, c);
}

// To support GNU utils.
int getopt_long(int argc, char **argv, const char *opts, const struct option *longopts, int *longindex) {
    static int sp = 1;
    register int c;
    register char *cp;

    if (sp == 1) {
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }
        if (argv[optind][0] == '-' && argv[optind][1] == '-') {
            char *arg = argv[optind] + 2; // Skip "--"
            if (*arg == '\0') { // Handle "--" alone
                optind++;
                return -1;
            }
            char *equals = strchr(arg, '=');
            char *name = arg;
            if (equals) {
                *equals = '\0'; // Split into name and value
            }
            const struct option *lo;
            int match_index = 0;
            for (lo = longopts; lo->name != NULL; lo++) {
                if (strcmp(lo->name, name) == 0) {
                    break;
                }
                match_index++;
            }
            if (lo->name == NULL) {
                fprintf(stderr, "%s: unrecognized option '--%s'\n", argv[0], name);
                optind++;
                return '?';
            }
            if (lo->has_arg == no_argument) {
                if (equals) {
                    fprintf(stderr, "%s: option '--%s' doesn't allow an argument\n", argv[0], name);
                    optind++;
                    return '?';
                }
                optarg = NULL;
            } else if (lo->has_arg == 1) {
                if (equals) {
                    optarg = equals + 1;
                } else {
                    if (optind + 1 < argc) {
                        optarg = argv[optind + 1];
                        optind++;
                    } else {
                        fprintf(stderr, "%s: option '--%s' requires an argument\n", argv[0], name);
                        optind++;
                        return '?';
                    }
                }
            } else if (lo->has_arg == 1) {
                if (equals) {
                    optarg = equals + 1;
                } else {
                    optarg = NULL;
                }
            }
            optind++;
            if (longindex) {
                *longindex = match_index;
            }
            if (lo->flag) {
                *lo->flag = lo->val;
                return 0;
            } else {
                return lo->val;
            }
        }
    }
    optopt = c = argv[optind][sp];
    if (c == ':' || (cp = strchr(opts, c)) == NULL) {
        ERR(": illegal option -- ", c);
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return '?';
    }
    if (*++cp == ':') {
        if (argv[optind][sp+1] != '\0') {
            optarg = &argv[optind++][sp+1];
        } else if (++optind >= argc) {
            ERR(": option requires an argument -- ", c);
            sp = 1;
            return '?';
        } else {
            optarg = argv[optind++];
        }
        sp = 1;
    } else {
        if (argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }
    return c;
}

void error(int status, int errnum, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vprintf(stderr, fmt, args);

//    if (errnum != 0) {
        fprintf(stderr, ": %s", strerror(errnum));
  //  }

    fprintf(stderr, "\n");
    va_end(args);

    if (status != 0) {
        exit(status);
    }
}

int parse_long_options(int argc, char **argv,
                       const char *progname,
                       const char *version,
                       void (*usage)(void))
{
    int i;

    for (i = optind; i < argc; i++) {
        char *arg = argv[i];

        if (arg[0] != '-' || arg[1] != '-') {
            /* Not a long option */
            break;
        }

        if (strcmp(arg, "--") == 0) {
            /* End of options */
            optind = i + 1;
            return 0;
        }

        /* Strip leading "--" */
        arg += 2;

        if (strcmp(arg, "help") == 0) {
            usage();
            return -1;
        } else if (strcmp(arg, "version") == 0) {
            printf("%s\n", version);
            exit(0);
        } else {
            /* Handle --option=value */
            char *eq = strchr(arg, '=');
            if (eq) {
                *eq = '\0';
                optarg = eq + 1;
            } else {
                optarg = NULL;
            }

            /* Return the first letter of the option as "code" */
            optind = i + 1;
            return arg[0]; 
        }
    }

    return 0;  /* No more long options */
}

void
strip_trailing_slashes (path)
     char *path;
{
  int last;

  last = strlen (path) - 1;
  while (last > 0 && path[last] == '/')
    path[last--] = '\0';
}

char	*
basename(input)
char	*input;
{
	char	*c_ptr;

	if ((c_ptr = strrchr(input + 1, '/')) == NULL)
		c_ptr = *input == '/' ? input + 1 : input;
	else
		c_ptr++;

	return(c_ptr);
}

int iscntrl(int c)
{
    unsigned int uc = (unsigned char)c;
    return (uc < 0x20) || (uc == 0x7F);
}

int isprint(int c)
{
    unsigned int uc = (unsigned char)c;
    return (uc - 0x20) < 0x5F;
}

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

struct passwd *
getpwnam(name)
char *name;
{
	register struct passwd *p;
	struct passwd *getpwent();

	setpwent();
	while( (p = getpwent()) && strcmp(name,p->pw_name) );
	endpwent();
	return(p);
}

#define ERR(s, c)	if(opterr){\
	(void) puts(argv[0]);\
	(void) puts(s);\
	(void) putchar(c);\
	(void) putchar('\n');}

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
