/*
 * File descriptors
 */

#include <types.h>
#include <defs.h>
#include <param.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>
#include <mmu.h>
#include <proc.h>
#include <stat.h>
#include <errno.h>
#include <buf.h>
#include <major.h>

struct {
	struct spinlock lock;
	struct file file[NFILE];
} ftable;

void file_init(void){
	initlock(&ftable.lock, "ftable");
}

int badwrite(int minor, struct inode *ip, char *buf, int nm, uint32_t off) { return -1; }
int badread(int minor, struct inode *ip, char *buf, int n, uint32_t off) { return -1; }

extern int memread(int minor, struct inode *ip, char *dst, int n, uint32_t off);
extern int memwrite(int minor, struct inode *ip, char *dst, int n, uint32_t off);
extern int ide0read(int minor, struct inode *ip, char *dst, int n, uint32_t off);
extern int ide0write(int minor, struct inode *ip, char *src, int n, uint32_t off);
extern int ttyauxread(int minor, struct inode *ip, char *src, int n, uint32_t off);
extern int ttyauxwrite(int minor, struct inode *ip, char *src, int n, uint32_t off);

struct devsw devsw[NDEV] = {
	[UNNAMED_MAJOR]	= {badread, badwrite},
	[MEM_MAJOR]	= {memread, memwrite},
	[FLOPPY_MAJOR]  = {badread, badwrite},
	[IDE0_MAJOR]	= {ide0read, ide0write},
	[HD_MAJOR]	= {ide0read, ide0write},
	[TTY_MAJOR]	= {badread, badwrite},
	[TTYAUX_MAJOR]	= {ttyauxread, ttyauxwrite},
};

// Allocate a file structure.
struct file* file_alloc(void){
	struct file *f;

	acquire(&ftable.lock);
	for(f = ftable.file; f < ftable.file + NFILE; f++){
		if(f->ref == 0){
			f->ref = 1;
			release(&ftable.lock);
			return f;
		}
	}
	release(&ftable.lock);
	return 0;
}

// Increment ref count for file f.
struct file* file_dup(struct file *f){
	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("filedup");
	f->ref++;
	release(&ftable.lock);
	return f;
}

// Close file f. (Decrement ref count, close when reaches 0.)
void file_close(struct file *f){
	struct file ff;

	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("fileclose");
	if(--f->ref > 0){
		release(&ftable.lock);
		return;
	}
	ff = *f;
	f->ref = 0;
	f->type = FD_NONE;
	release(&ftable.lock);

	if(ff.type == FD_PIPE)
		pipeclose(ff.pipe, ff.writable);
	else if(ff.type == FD_INODE){
		begin_op();
		iput(ff.ip);
		end_op();
	}
}

// Get metadata about file f.
int file_stat(struct file *f, struct stat *st){
	if(f->type == FD_INODE){
		ilock(f->ip);
		stati(f->ip, st);
		iunlock(f->ip);
		return 0;
	}
	return -1;
}

// Read from file f.
int file_read(struct file *f, char *addr, int n){
	int r;

	if(f->readable == 0)
		return -EACCES;
	if(f->type == FD_PIPE)
		return piperead(f->pipe, addr, n);
	if(f->type == FD_INODE){
		ilock(f->ip);
		if((r = readi(f->ip, addr, f->off, n)) > 0)
			f->off += r;
		iunlock(f->ip);
		return r;
	}
	panic("fileread");
}

// Write to file f.
int file_write(struct file *f, char *addr, int n){
	int r;

	if(f->writable == 0)
		return -EACCES;
	if(f->type == FD_PIPE)
		return pipewrite(f->pipe, addr, n);
	if(f->type == FD_INODE){
		// write a few blocks at a time to avoid exceeding
		// the maximum log transaction size, including
		// i-node, indirect block, allocation blocks,
		// and 2 blocks of slop for non-aligned writes.
		// this really belongs lower down, since writei()
		// might be writing a device like the console.
		int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
		int i = 0;
		while(i < n){
			int n1 = n - i;
			if(n1 > max)
				n1 = max;

			begin_op();
			ilock(f->ip);
			if (myproc()->uid != f->ip->uid &&
					((f->ip->mode & S_IFMT) != S_IFCHR) &&
					((f->ip->mode & S_IFMT) != S_IFBLK)) {
				iunlock(f->ip);
				end_op();
				return -1;
			}
			if ((r = writei(f->ip, addr + i, f->off, n1)) > 0){
				f->off += r;
				f->ip->lmtime = epoch_mktime();
				iupdate(f->ip);
			}
			iunlock(f->ip);
			end_op();

			if(r < 0)
				break;
			if(r != n1)
				panic("short filewrite");
			i += r;
		}
		return i == n ? n : -1;
	}
	panic("filewrite");
}

