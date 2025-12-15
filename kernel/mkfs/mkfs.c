#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <libgen.h>

#include <dirent.h>
#define dirent xv6_dirent

#define stat xv6_stat	// avoid clash with host struct stat
#include "../../include/fs.h"
#include "../../include/param.h"
#undef stat
#undef dirent

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE/(BSIZE*8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;	// Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;	// Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
unsigned int freeinode = 1;
unsigned int freeblock;

void balloc(int);
void wsect(unsigned int, void*);
void winode(unsigned int, struct dinode*);
void rinode(unsigned int inum, struct dinode *ip);
void rsect(unsigned int sec, void *buf);
unsigned int ialloc(ushort type);
void iappend(unsigned int inum, void *p, int n);

// convert to intel byte order
ushort
xshort(ushort x)
{
	ushort y;
	unsigned char *a = (unsigned char*)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}

unsigned int
xint(unsigned int x)
{
	unsigned int y;
	unsigned char *a = (unsigned char*)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

// Add "." and ".." entries to a directory inode
void
add_dot_entries(unsigned int dir_ino, unsigned int parent_ino)
{
	struct xv6_dirent de;
	memset(&de, 0, sizeof(de));
	de.d_ino = xshort(dir_ino);
	strcpy(de.d_name, ".");
	iappend(dir_ino, &de, sizeof(de));

	memset(&de, 0, sizeof(de));
	de.d_ino = xshort(parent_ino);
	strcpy(de.d_name, "..");
	iappend(dir_ino, &de, sizeof(de));
}

unsigned int create_directory(unsigned int parent_ino, ushort mode) {
	// Allocate inode for the new directory
	unsigned int new_ino = ialloc(mode);
	if (new_ino == 0) return 0; // Allocation failure

	add_dot_entries(new_ino, parent_ino);
	return new_ino;
}

// Convert host mode to XV6 filesystem mode
ushort convert_mode(mode_t host_mode) {
	ushort xv6_mode = 0;

	if (S_ISDIR(host_mode))
		xv6_mode |= S_IFDIR;
	else if (S_ISREG(host_mode))
		xv6_mode |= 0;

	if (host_mode & S_IRUSR) xv6_mode |= S_IRUSR;
	if (host_mode & S_IWUSR) xv6_mode |= S_IWUSR;
	if (host_mode & S_IXUSR) xv6_mode |= S_IXUSR;
	if (host_mode & S_IRGRP) xv6_mode |= S_IRGRP;
	if (host_mode & S_IXGRP) xv6_mode |= S_IXGRP;
	if (host_mode & S_IROTH) xv6_mode |= S_IROTH;
	if (host_mode & S_IXOTH) xv6_mode |= S_IXOTH;

	return xv6_mode;
}

int
scan_directory(const char *host_path, unsigned int xv6_ino)
{
	DIR *dir;
	struct dirent *entry;	// Host dirent
	struct stat st;
	char path[1024];
	int fd;
	char buf[BSIZE];
	ssize_t cc;

	dir = opendir(host_path);
	if (!dir) {
		perror(host_path);
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		// Build full path
		snprintf(path, sizeof(path), "%s/%s", host_path, entry->d_name);

		if (stat(path, &st) < 0) {
			perror(path);
			continue;
		}

		if (strlen(entry->d_name) >= DIRSIZ) {
			fprintf(stderr, "Warning: filename too long, truncating: %s\n", entry->d_name);
		}

		ushort mode = convert_mode(st.st_mode);
		unsigned int new_ino;

		if (S_ISDIR(st.st_mode)) {
			new_ino = create_directory(xv6_ino, mode);
			if (new_ino == 0) {
				fprintf(stderr, "Failed to create directory: %s\n", entry->d_name);
				continue;
			}

			// Add directory entry to parent
			struct xv6_dirent de;
			memset(&de, 0, sizeof(de));
			de.d_ino = xshort(new_ino);
			strncpy(de.d_name, entry->d_name, DIRSIZ);
			de.d_name[DIRSIZ-1] = '\0';
			iappend(xv6_ino, &de, sizeof(de));

			printf("	%s/\n", path);
			if (scan_directory(path, new_ino) < 0) {
				closedir(dir);
				return -1;
			}

		} else if (S_ISREG(st.st_mode)) {
			new_ino = ialloc(mode);

			struct xv6_dirent de;
			memset(&de, 0, sizeof(de));
			de.d_ino = xshort(new_ino);
			strncpy(de.d_name, entry->d_name, DIRSIZ);
			de.d_name[DIRSIZ-1] = '\0';
			iappend(xv6_ino, &de, sizeof(de));

			fd = open(path, O_RDONLY);
			if (fd < 0) {
				perror(path);
				continue;
			}

			printf("	%s (%ld bytes, mode %o)\n", path, st.st_size, mode);

			while ((cc = read(fd, buf, sizeof(buf))) > 0) {
				iappend(new_ino, buf, cc);
			}

			close(fd);
		} else {
			fprintf(stderr, "Warning: skipping special file: %s\n", path);
		}
	}

	closedir(dir);
	return 0;
}

int
main(int argc, char *argv[])
{
	int i;
	unsigned int rootino;
	struct dinode din;
	unsigned int off;
	char buf[BSIZE];

	static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

	if (argc != 3) {
		fprintf(stderr, "Usage: mkfs fs.img root_directory\n");
		fprintf(stderr, "	fs.img				 - Output filesystem image\n");
		fprintf(stderr, "	root_directory - Directory to use as filesystem root\n");
		exit(1);
	}

	assert((BSIZE % sizeof(struct xv6_dirent)) == 0);

	fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fsfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	nmeta = 2 + nlog + ninodeblocks + nbitmap;
	nblocks = FSSIZE - nmeta;

	sb.size = xint(FSSIZE);
	sb.nblocks = xint(nblocks);
	sb.ninodes = xint(NINODES);
	sb.nlog = xint(nlog);
	sb.logstart = xint(2);
	sb.inodestart = xint(2+nlog);
	sb.bmapstart = xint(2+nlog+ninodeblocks);

	printf("mkfs: creating filesystem image %s\n", argv[1]);
	printf("	meta blocks: %d (boot=1, super=1, log=%u, inode=%u, bitmap=%u)\n",
				 nmeta, nlog, ninodeblocks, nbitmap);
	printf("	data blocks: %d\n", nblocks);
	printf("	total blocks: %d\n", FSSIZE);

	freeblock = nmeta;

	for(i = 0; i < FSSIZE; i++)
		wsect(i, zeroes);

	memset(buf, 0, sizeof(buf));
	memmove(buf, &sb, sizeof(sb));
	wsect(1, buf);

	rootino = ialloc(S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	assert(rootino == ROOTINO);
	add_dot_entries(rootino, rootino);

	printf("\nroot directory: %s\n", argv[2]);
	if (scan_directory(argv[2], rootino) < 0) {
		fprintf(stderr, "error scanning directory tree\n");
		exit(1);
	}

	// fix size of root inode dir
	rinode(rootino, &din);
	off = xint(din.size);
	off = ((off/BSIZE) + 1) * BSIZE;
	din.size = xint(off);
	winode(rootino, &din);

	balloc(freeblock);

	printf("done!\n");
	printf("	%d blocks (%.1f%% of %d)\n",
				 freeblock, (float)freeblock * 100 / FSSIZE, FSSIZE);
	printf("	%d inodes (%.1f%% of %d)\n",
				 freeinode - 1, (float)(freeinode - 1) * 100 / NINODES, NINODES);

	close(fsfd);
	exit(0);
}

void
wsect(unsigned int sec, void *buf)
{
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (write(fsfd, buf, BSIZE) != BSIZE) {
		perror("write");
		exit(1);
	}
}

void
winode(unsigned int inum, struct dinode *ip)
{
	char buf[BSIZE];
	unsigned int bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode*)buf) + (inum % IPB);
	*dip = *ip;
	wsect(bn, buf);
}

void
rinode(unsigned int inum, struct dinode *ip)
{
	char buf[BSIZE];
	unsigned int bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode*)buf) + (inum % IPB);
	*ip = *dip;
}

void
rsect(unsigned int sec, void *buf)
{
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (read(fsfd, buf, BSIZE) != BSIZE) {
		perror("read");
		exit(1);
	}
}

unsigned int
ialloc(ushort type)
{
	unsigned int inum = freeinode++;
	struct dinode din;

	memset(&din, 0, sizeof(din));
	din.mode = xshort(type);
	din.nlink = xshort(1);
	din.size = xint(0);
	time_t now = time(NULL);
	din.ctime = xint((unsigned int)now);
	din.lmtime = xint((unsigned int)now);
	winode(inum, &din);
	return inum;
}

void
balloc(int used)
{
	unsigned char buf[BSIZE];
	int i;

	printf("balloc: allocated %d blocks\n", used);
	assert(used < BSIZE*8);
	memset(buf, 0, BSIZE);
	for (i = 0; i < used; i++) {
		buf[i/8] = buf[i/8] | (0x1 << (i%8));
	}
	wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(unsigned int inum, void *xp, int n)
{
	char *p = (char*)xp;
	unsigned int fbn, off, n1;
	struct dinode din;
	char buf[BSIZE];
	unsigned int indirect[NINDIRECT];
	unsigned int x;

	rinode(inum, &din);
	off = xint(din.size);

	while (n > 0) {
		fbn = off / BSIZE;
		assert(fbn < MAXFILE);

		if (fbn < NDIRECT) {
			if (xint(din.addrs[fbn]) == 0) {
				din.addrs[fbn] = xint(freeblock++);
			}
			x = xint(din.addrs[fbn]);
		} else {
			if (xint(din.addrs[NDIRECT]) == 0) {
				din.addrs[NDIRECT] = xint(freeblock++);
			}
			rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
			if (indirect[fbn - NDIRECT] == 0) {
				indirect[fbn - NDIRECT] = xint(freeblock++);
				wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
			}
			x = xint(indirect[fbn-NDIRECT]);
		}

		n1 = min(n, (fbn + 1) * BSIZE - off);
		rsect(x, buf);
		memcpy(buf + off - (fbn * BSIZE), p, n1);
		wsect(x, buf);
		n -= n1;
		off += n1;
		p += n1;
	}

	din.size = xint(off);
	winode(inum, &din);
}
