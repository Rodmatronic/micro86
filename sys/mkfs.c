#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <../include/fcntl.h>
#include <assert.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "../include/fs.h"
#include "../include/param.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE/(BSIZE*8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
ushort
xshort(ushort x)
{
  ushort y;
  u_char *a = (u_char*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint
xint(uint x)
{
  uint y;
  u_char *a = (u_char*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

// Add "." and ".." entries to a directory inode
void
add_dot_entries(uint dir_ino, uint parent_ino)
{
  struct dirent de;
  bzero(&de, sizeof(de));
  de.inum = xshort(dir_ino);
  strcpy(de.name, ".");
  iappend(dir_ino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(parent_ino);
  strcpy(de.name, "..");
  iappend(dir_ino, &de, sizeof(de));
}

uint create_directory(uint parent_ino, const char *name) {
  // Allocate inode for the new directory
  uint new_ino = ialloc(S_IFDIR);
  if (new_ino == 0) return 0; // Allocation failure

  struct dirent de;

  // Add entry to parent directory
  bzero(&de, sizeof(de));
  de.inum = xshort(new_ino);
  strncpy(de.name, name, DIRSIZ);
  de.name[DIRSIZ-1] = '\0'; // Ensure null-termination if truncated
  iappend(parent_ino, &de, sizeof(de));

  add_dot_entries(new_ino, parent_ino);

  return new_ino;
}

int
exists_in_list(char *name, char **list)
{
    for (int i = 0; list[i] != NULL; i++) {
        if (strcmp(name, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int
main(int argc, char *argv[])
{
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;


  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

// assert((BSIZE % sizeof(struct dinode)) == 0);
 assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  // 1 fs block = 1 disk sector
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = FSSIZE - nmeta;

  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2+nlog);
  sb.bmapstart = xint(2+nlog+ninodeblocks);

  printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
         nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

  freeblock = nmeta;     // the first free block that we can allocate

  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(S_IFDIR);
  assert(rootino == ROOTINO);

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  uint binino = create_directory(rootino, "bin");
  create_directory(rootino, "dev");
  uint etcino = create_directory(rootino, "etc");
  create_directory(rootino, "root");
  create_directory(rootino, "tmp");
  uint usrino = create_directory(rootino, "usr");
  create_directory(usrino, "adm");
  uint usrmanino = create_directory(usrino, "man");
  uint manman1 = create_directory(usrmanino, "man1");
  uint usrhomeino = create_directory(usrino, "home");
  create_directory(usrhomeino, "pou");
  uint usrlibino = create_directory(usrino, "lib");
  uint optino = create_directory(rootino, "opt");
  uint optbinino = create_directory(optino, "bin");

for (i = 2; i < argc; i++) {
    if ((fd = open(argv[i], 0)) < 0) {
        perror(argv[i]);
        exit(1);
    }

    char *path = argv[i];
    char *base = strrchr(path, '/');
    if (base) base++;
    else base = path;

    // Handle leading '_'
    char *name = base;
    if (name[0] == '_')
        name++;

    inum = ialloc(S_IFREG);

    // Create directory entry
    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, name, DIRSIZ);

    char * bin_files[] = {
	"banner",
	"basename",
	"cat",
	"cmp",
	"cp",
	"date",
	"dd",
	"debugger",
	"dirname",
	"du",
	"echo",
	"ed",
	"find",
	"grep",
	"hexdump",
	"kill",
	"line",
	"ln",
	"ls",
	"login",
	"man",
	"mkdir",
	"mknod",
	"more",
	"mv",
	"passwd",
	"pwd",
	"rm",
	"rmdir",
	"sleep",
	"sh",
	"su",
	"uname",
	"touch",
	"wc",
	"yes",
	NULL
    };

    char * etc_files[] = {
	"cron",
	"group",
	"init",
	"motd",
	"passwd.1",
	"rc",
	"rc.local",
	"stressfs",
	"usertests",
	"unlink",
	"getty",
	NULL
    };

    char * usrlib_files[] = {
	"crontab",
	NULL
    };

    char * manman1_files[] = {
	"cd.1",
	NULL
    };

    char * optbin_files[] = {
	"desktop",
	"hellogui",
	"paint2",
	NULL
    };

    // append files to disk
    if (exists_in_list(name, etc_files)) {
      if (strcmp(name, "passwd.1") == 0) { // prevent interfering
        strncpy(de.name, "passwd", DIRSIZ);
      }
      iappend(etcino, &de, sizeof(de));
    } else if (exists_in_list(name, bin_files)) {
      iappend(binino, &de, sizeof(de));
    } else if (exists_in_list(name, usrlib_files)) {
      iappend(usrlibino, &de, sizeof(de));
    } else if (exists_in_list(name, manman1_files)) {
	    iappend(manman1, &de, sizeof(de));
    } else if (exists_in_list(name, optbin_files)) {
	    iappend(optbinino, &de, sizeof(de));
    } else {
      iappend(rootino, &de, sizeof(de));
    }

    // Append file contents
    while ((cc = read(fd, buf, sizeof(buf))) > 0)
        iappend(inum, buf, cc);

    close(fd);
}

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  balloc(freeblock);

  exit(0);
}


void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, BSIZE) != BSIZE){
    perror("write");
    exit(1);
  }
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    perror("read");
    exit(1);
  }
}

uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.mode = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void
balloc(int used)
{
  u_char buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < BSIZE*8);
  bzero(buf, BSIZE);
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while(n > 0){
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if(fbn < NDIRECT){
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if(xint(din.addrs[NDIRECT]) == 0){
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn-NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
