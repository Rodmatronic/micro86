#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <fs.h>
#include <fcntl.h>

char buf[512];

long
du(char *path)
{
  int fd;
  struct stat st;
  struct dirent de;
  char subpath[512];
  long blocks = 0;

  if (stat(path, &st) < 0) {
    printf("du: cannot stat %s\n", path);
    return 0;
  }

  blocks = (st.st_size + BSIZE-1) / BSIZE;

  if ((st.st_mode & S_IFMT) != S_IFDIR) {
    printf("%ld\t%s\n", blocks, path);
    return blocks;
  }

  fd = open(path, 0);
  if (fd < 0) {
    printf("du: cannot open %s\n", path);
    return blocks;
  }

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    if (!strcmp(de.name, ".") || !strcmp(de.name, "..")) continue;

    // build subpath
    strcpy(subpath, path);
    char *p = subpath + strlen(subpath);
    *p++ = '/';
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    // trim trailing nulls
    for (char *q = p + DIRSIZ - 1; q >= p && *q == 0; q--) *q = 0;

    blocks += du(subpath);
  }
  close(fd);

  printf("%ld\t%s\n", blocks, path);
  return blocks;
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    // Use /bin/pwd to get the current directory
    int p[2];
    pipe(p);

    if (fork() == 0) {
      close(p[0]);
      dup(p[1]);
      close(p[1]);
      char *args[] = {"pwd", 0};
      exec("/bin/pwd", args);
      exit(0);
    }

    close(p[1]);
    int n = read(p[0], buf, sizeof(buf)-1);
    close(p[0]);
    wait(0);

    if (n > 0) {
      buf[n] = 0;
      // Remove trailing newline if present
      if (buf[n-1] == '\n') buf[n-1] = 0;
      du(buf);
    } else {
      du(".");
    }
  } else {
    for (int i = 1; i < argc; i++)
      du(argv[i]);
  }
  exit(0);
}
