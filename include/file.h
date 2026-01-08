struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  unsigned int off;
  unsigned int flags;
};

// in-memory copy of an inode
struct inode {
  unsigned int dev;           // Device number
  unsigned int inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short mode;         // copy of disk inode
  unsigned int lmtime;
  unsigned int ctime;
  short major;
  short minor;
  short nlink;
  unsigned int size;
  unsigned int addrs[NDIRECT+1];
  unsigned int uid;
  unsigned int gid;
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int, uint32_t);
  int (*write)(struct inode*, char*, int, uint32_t);
};

extern struct devsw devsw[];

#define CONSOLE 1
#define TTY     2
#define NULLDEV 3
#define RANDOM  4
#define SCRDEV  5
#define KEYDEV  6
#define MOUSDEV 7
#define ZERODEV 8
#define DISKDEV	9
