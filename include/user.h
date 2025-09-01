#ifndef USER_H
#define USER_H

struct stat;
struct rtcdate;

#include "types.h"
#include "../include/utsname.h"
#include "../include/tty.h"

#define stdout 1
#define stderr 2
#define stdin 0

#define EOF (-1)
#define NULL ((void*)0)

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

int stty(struct ttyb *);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

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
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
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
int sync(void);
int lseek(int, int, int);
int devctl(int dev, int sig, int data);
int stime(unsigned long);

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
void puts(const char *fmt, ...);
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
int execl(const char *path, const char *arg0, ...);
char* fgets(char *buf, int max, int fd);
// ucrypt.c
char* crypt(char *pw, char *salt);

// udate.c
void epoch_to_tm(unsigned long epoch, struct tm *tm);
unsigned long mktime(struct tm * tm);
int isleapyear(int);

#endif
