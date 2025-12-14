// On-disk file system format.
// Both the kernel and user programs use this header file.

#ifndef FS_H
#define FS_H

#define ROOTINO 1  // root i-number
#define BSIZE 2048  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  unsigned int size;         // Size of file system image (blocks)
  unsigned int nblocks;      // Number of data blocks
  unsigned int ninodes;      // Number of inodes.
  unsigned int nlog;         // Number of log blocks
  unsigned int logstart;     // Block number of first log block
  unsigned int inodestart;   // Block number of first inode block
  unsigned int bmapstart;    // Block number of first free map block
};

#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(unsigned int))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short mode;           // File type
  unsigned int lmtime;
  unsigned int ctime;
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  unsigned int size;            // Size of file (bytes)
  unsigned int addrs[NDIRECT+1];   // Data block addresses
  unsigned int uid;
  unsigned int gid;
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))
// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

#define _DIRENT_HAVE_D_RECLEN
#define _DIRENT_HAVE_D_OFF
#define _DIRENT_HAVE_D_TYPE

struct dirent {
    ushort d_ino;
    ushort d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[DIRSIZ];
    char _pad[32 - (2 + 2 + 2 + 1 + DIRSIZ)];
};
#endif
