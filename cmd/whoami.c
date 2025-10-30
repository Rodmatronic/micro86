/*
 * print effective userid
 */

#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>

int
main(int argc, char *argv[])
{
  struct passwd *pw;
  uid_t uid;

  uid = geteuid();
  pw = getpwuid(uid);
  if (pw) {
    printf("%s\n", pw->pw_name);
    exit(0);
  } else {
    printf("%d\n", uid);
    exit(0);
  }
  exit (1);
}
