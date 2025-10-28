#include <errno.h>
#include <stdio.h>
#include <stat.h>
#include <fcntl.h>

int
main(int argc, char ** argv)
{
  for (int i = 0; i < 8; i++){
	printf("\033[3%dm#", i);
  }
  printf("\n");
  for (int i = 0; i < 8; i++){
	printf("\033[4%dm#", i);
  }
  printf("\033[0m\n");
  for (int i = 0; i < 8; i++){
	printf("\033[9%dm#", i);
  }
  printf("\n");
  for (int i = 0; i < 8; i++){
	printf("\033[10%dm#", i);
  }
  printf("\033[0m\n");

  printf("%d\n", devctl(3, 0, 0));
  struct uproc info;
  if (getproc(24, &info) == 0)
    printf("pid=%d name=%s state=%d\n", info.p_pid, info.name, info.state);
  else
    printf("process not found\n");

  printf("will now segfault\n");
  printf("%s", argv[99]);
  printf("huh?\n");

  exit(1);
}
