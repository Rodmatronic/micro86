#ifndef STDIO_H
#define STDIO_H

struct stat;
struct rtcdate;

#include "types.h"
#include "../include/utsname.h"
#include "../include/tty.h"
#include "../include/stdarg.h"
#include "../include/fs.h"

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct uproc {
  uint sz;                     // Size of process memory (bytes)
  enum procstate state;        // Process state
  int p_pid;                     // Process ID
  int killed;                  // If non-zero, have been killed
  char name[16];               // Process name (debugging)
  int p_uid;                     // User ID
  int p_gid;                   // Group ID
  int exitstatus;              // Exit status number
  int ttyflags;                // TTY flags
};

#define	dysize(y) \
	(((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0) ? 366 : 365)

#define	CTL_HW		6		/* generic CPU/io */
#define	HW_MACHINE_ARCH	10		/* string: machine architecture */
#define NSIG 64

# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4

#define MB_CUR_MAX 1
#define FMT_SCALED_STRSIZE 7

#define no_argument 0
#define required_argument 1
#define optional_argument 2

#define major(dev) ((int)(((unsigned int)(dev) >> 8) & 0xFF))
#define minor(dev) ((int)((dev) & 0xFF))
#define ST_NBLOCKS(stat) (((stat).size + 511) / 512)
#define alloca(size) malloc(size)
#define SAFE_LSTAT(path, statbuf) stat((path), (statbuf))
#define SAFE_STAT(path, statbuf) stat((path), (statbuf))
#define PATH_MAX		 255

extern int optind;

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define LC_ALL "C"
extern char * __progname;

#define nullptr ((void*)0)

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

extern char* optarg;

typedef struct {
    int fd;
    struct dirent de;
} DIR;

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

#define makedev(major, minor) (((major) << 8) | (minor))
#define stdout 1
#define stderr 2
#define stdin 0

#define EOF (-1)
#define NULL ((void*)0)

#define _(String) (String)
#define proper_name(String) (String)

#define BUFSIZ 256
extern char *month[];
extern int days_in_month[];
extern int days_in_month_leap[];

struct tm {
  int tm_sec;   // seconds (0-59)
  int tm_min;   // minutes (0-59)
  int tm_hour;  // hours (0-23)
  int tm_yday;
  int tm_mday;  // day of month (1-31)
  int tm_mon;   // month (0-11)
  int tm_year;  // years since 1970
  int tm_wday;  // day of week (0-6, Sunday=0)
  int tm_isdst;
};

// system calls
int fork(void);
int exit(int status) __attribute__((noreturn));
int wait(int *status);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int, int);
int exec(char*, char**);
int open(const char*, int, ...);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(int, mode_t);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int getuid(void);
int getgid(void);
int setuid(int);
int setgid(int);
int usleep(unsigned long);
int time(unsigned long);
int uname(struct utsname *);
int stty(struct ttyb *);
int gtty(struct ttyb *);
int sync(void);
int lseek(int, int, int);
int devctl(int dev, int sig, int data);
int stime(unsigned long);
int utime(const char *path);
int sethostname(const char *, size_t len);
int setenv(const char *name, const char *value, int);
int environ(char *buf, int buflen);
int chmod(const char *path, mode_t mode);
int reboot(int);
int chown(const char *pathname, uid_t owner, gid_t group);
int getproc(pid_t, struct uproc *up);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
int getc(int fd);
int getchar();
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
char* strerror(int);
int putchar(char);
int puts(const char *fmt, ...);
int isdigit(int);
int fclose(int);
//char* month(int);
char* strrchr (register const char *s, int c);
int toupper(int);
int islower(int);
int abs (int);
char* strdup (const char *);
char* strtok(char *s, const char *delim);
char* strtok_r(char *s, const char *delim, char **last);
int getopt(int argc, char **argv, char *opts);
char *ctime(const struct tm *tm);
int sprintf(char *buf, const char *fmt, ...);
char*(strcat)(char *s1, const char *s2);
int sscanf(const char*, const char*, ...);
int seek(int fd, int offset, int whence);
int execl(char *path, const char *arg0, ...);
char* fgets(char *buf, int max, int fd);
void vprintf(int fd, const char *fmt, va_list ap);
void error(int status, int errnum, const char *fmt, ...);

// ucrypt.c
char* crypt(char *pw, char *salt);

// udate.c
void epoch_to_tm(unsigned long epoch, struct tm *tm);
unsigned long mktime(struct tm * tm);
int isleapyear(int);

// setmode.c
void *setmode(const char *);

// reallocarray.c
void * reallocarray(void *optr, size_t nmemb, size_t size);

#endif
