#include <types.h>
#include <stdio.h>
#include <fcntl.h>

#define LINES_PER_PAGE 29

void
wait_prompt() {
  char c;
  write(1, "--More--", 8);

  int console_fd = open("/dev/console", O_RDONLY);
  if (console_fd < 0) {
    exit(1);
  }

  if (read(console_fd, &c, 1) != 1) {
    close(console_fd);
    exit(0);
  }
  if (c == 'q') {
    close(console_fd);
    exit(0);
  }
  close(console_fd);
}

void
more(int fd)
{
  char ch;
  int lines = 0;

  while (read(fd, &ch, 1) == 1) {
    write(1, &ch, 1);
    if (ch == '\n') {
      lines++;
      if (lines >= LINES_PER_PAGE) {
        wait_prompt();
        lines = 0;
      }
    }
  }
  exit(0);
}

int
main(int argc, char *argv[]) {
  if (argc <= 1) {
    more(0); // Read from stdin
  } else {
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Cannot open file\n");
      exit(1);
    }
    more(fd);
    close(fd);
  }
  exit(0);
}
