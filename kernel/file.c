//
// File descriptors
//

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

struct devsw devsw[NDEV];
struct {
	struct spinlock lock;
	struct file file[NFILE];
} ftable;

void
fileinit(void)
{
	initlock(&ftable.lock, "ftable");
}

int badwrite(struct inode *ip, char *buf, int nm, uint32_t off) { return -1; }
int badread(struct inode *ip, char *buf, int n, uint32_t off) { return -1; }
int nullread(struct inode *ip, char *buf, int n, uint32_t off) { return 0; }

// Devnodes devices
uint32_t
xorshift32(uint32_t *state)
{
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

static uint32_t rng_state;

// very simple entropy
int
rndread(struct inode *ip, char *dst, int n, uint32_t off)
{
	uint32_t lo, hi;

	// Generate entropy using rdtsc and mix into PRNG state
	asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
	uint32_t entropy = lo ^ hi;
	rng_state ^= entropy; // Mix entropy into existing state

	uint32_t *buf32 = (uint32_t*)dst;
	int num_words = n / sizeof(uint32_t);
	int remainder = n % sizeof(uint32_t);

	for (int i = 0; i < num_words; i++) {
		buf32[i] = xorshift32(&rng_state);
	}

	if (remainder > 0) {
		uint32_t val = xorshift32(&rng_state);
		char *tail = (char*)(buf32 + num_words);
		for (int i = 0; i < remainder; i++) {
			tail[i] = (val >> (i * 8)) & 0xFF;
		}
	}

	return n;
}

int
keyread(struct inode *ip, char *dst, int n, uint32_t off)
{
	return n;
}

int mouseread(struct inode *ip, char *dst, int n, uint32_t off) {
	return n;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

int diskread(struct inode *ip, char *dst, int n, uint32_t off){
	struct buf *bp;
	int bytes_read = 0;

	while(n > 0) {
		uint32_t sector = off / BSIZE;
		uint32_t sector_off = off % BSIZE;

		bp = bread(ip->dev, sector);
		int count = min(n, BSIZE - sector_off);
		memmove(dst, bp->data + sector_off, count);
		brelse(bp);
		dst += count;
		bytes_read += count;
		n -= count;
		off += count;
	}

	return bytes_read;
}

int diskwrite(struct inode *ip, char *src, int n, uint32_t off){
	struct buf *bp;
	uint32_t sector = ip->minor;
	int bytes_written = 0;

	while(n > 0) {
		bp = bread(0, sector);
		int count = min(n, BSIZE);
		memmove(bp->data, src, count);
		bwrite(bp);
		brelse(bp);
		src += count;
		bytes_written += count;
		n -= count;
		sector++;
	}

	return bytes_written;
}

// Devnodes devices (not /dev/console)
void
devinit()
{
	// Character devices
	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	devsw[NULLDEV].read = nullread;
	devsw[NULLDEV].write = badwrite;
	devsw[RANDOM].read = rndread;
	devsw[RANDOM].write = badwrite;
	devsw[KEYDEV].read = keyread;
	devsw[KEYDEV].write = badwrite;
	devsw[MOUSDEV].read = mouseread;
	devsw[MOUSDEV].write = badwrite;

	// Disk block device
	devsw[DISKDEV].read = diskread;
	devsw[DISKDEV].write = diskwrite;

}

// Allocate a file structure.
struct file*
filealloc(void)
{
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
struct file*
filedup(struct file *f)
{
	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("filedup");
	f->ref++;
	release(&ftable.lock);
	return f;
}

// Close file f.	(Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
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
int
filestat(struct file *f, struct stat *st)
{
	if(f->type == FD_INODE){
		ilock(f->ip);
		stati(f->ip, st);
		iunlock(f->ip);
		return 0;
	}
	return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
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
int
filewrite(struct file *f, char *addr, int n)
{
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

