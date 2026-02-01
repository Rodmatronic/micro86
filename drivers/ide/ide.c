// Simple PIO-based (non-DMA) IDE driver code.

#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <traps.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <fs.h>
#include <stat.h>
#include <file.h>
#include <buf.h>

#define SECTOR_SIZE	512
#define IDE_BSY		0x80
#define IDE_DRDY	0x40
#define IDE_DF		0x20
#define IDE_ERR		0x01

#define IDE_CMD_READ	0x20
#define IDE_CMD_WRITE	0x30
#define IDE_CMD_RDMUL	0xc4
#define IDE_CMD_WRMUL	0xc5

#define min(a, b) ((a) < (b) ? (a) : (b))

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;
static int havedisk1;
static void ide_start(struct buf*);

// Wait for IDE disk to become ready.
static int idewait(int checkerr){
	int r;

	while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY);

	if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
		return -1;
	return 0;
}

void ide_init(void){
	int i;

	initlock(&idelock, "ide");
	pic_enable(IRQ_IDE);

	printk("waiting on root device...\n");

	// Explicitly select drive 0
	outb(0x1f6, 0xe0 | (0<<4));	// Select drive 0, LBA mode
	idewait(0);	// Wait for drive 0 to be ready

	// Check if disk 1 is present
	outb(0x1f6, 0xe0 | (1<<4));	// Select drive 1
	for(i = 0; i < 1000; i++){
		if(inb(0x1f7) != 0xff) {	// 0xFF = floating bus (no drive)
			havedisk1 = 1;
			break;
		}
	}

	// Switch back to disk 0
	outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b. Caller must hold idelock.
static void ide_start(struct buf *b){
	if(b == 0)
		panic("failed to start ide (b = 0)");
	if(b->blockno >= FSSIZE)
		panic("fatal ide error: incorrect number of blocks");
	int sector_per_block =	BSIZE/SECTOR_SIZE;
	int sector = b->blockno * sector_per_block;
	int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :	IDE_CMD_RDMUL;
	int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

	if (sector_per_block > 7) panic("fatal ide error: incorrect sector-per-block amount (%d)", sector_per_block);

	idewait(0);
	outb(0x3f6, 0);	// generate interrupt
	outb(0x1f2, sector_per_block);	// number of sectors
	outb(0x1f3, sector & 0xff);
	outb(0x1f4, (sector >> 8) & 0xff);
	outb(0x1f5, (sector >> 16) & 0xff);
	outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
	if(b->flags & B_DIRTY){
		outb(0x1f7, write_cmd);
		outsl(0x1f0, b->data, BSIZE/4);
	} else {
		outb(0x1f7, read_cmd);
	}
}

// Interrupt handler.
void ide_interrupt(void){
	struct buf *b;

	// First queued buffer is the active request.
	acquire(&idelock);

	if((b = idequeue) == 0){
		release(&idelock);
		return;
	}
	idequeue = b->qnext;

	// Read data if needed.
	if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
		insl(0x1f0, b->data, BSIZE/4);

	// Wake process waiting for this buf.
	b->flags |= B_VALID;
	b->flags &= ~B_DIRTY;
	wakeup(b);

	// Start disk on next buf in queue.
	if(idequeue != 0)
		ide_start(idequeue);

	release(&idelock);
}

// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void ide_dirty_write(struct buf *b){
	struct buf **pp;

	if(!holdingsleep(&b->lock))
		panic("iderw: buf not locked");
	if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
		panic("iderw: nothing to do");
	if(b->dev != 0 && !havedisk1)
		panic("iderw: ide disk 1 not present");

	acquire(&idelock);	//DOC:acquire-lock

	// Append b to idequeue.
	b->qnext = 0;
	for(pp=&idequeue; *pp; pp=&(*pp)->qnext); //DOC:insert-queue
	*pp = b;

	// Start disk if necessary.
	if(idequeue == b)
		ide_start(b);

	// Wait for request to finish.
	while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
		sleep(b, &idelock);
	}

	release(&idelock);
}

int ide0read(int minor, struct inode *ip, char *dst, int n, uint32_t off){
	struct buf *bp;
	int bytes_read = 0;

	while(n > 0) {
		uint32_t sector = off / BSIZE;
		uint32_t sector_off = off % BSIZE;

		bp = buffer_read(ip->dev, sector);
		int count = min(n, BSIZE - sector_off);
		memmove(dst, bp->data + sector_off, count);
		buffer_release(bp);
		dst += count;
		bytes_read += count;
		n -= count;
		off += count;
	}

	return bytes_read;
}

int ide0write(int minor, struct inode *ip, char *src, int n, uint32_t off){
	struct buf *bp;
	int tot = 0;

	if((ip->mode & S_IFMT) == S_IFBLK){
		uint32_t base = ip->minor;

		while(n > 0){
			uint32_t sector = base + (off / BSIZE);
			uint32_t boff = off % BSIZE;

			bp = buffer_read(ip->dev, sector);

			int count = BSIZE - boff;
			if(count > n)
				count = n;

			memmove(bp->data + boff, src, count);

			buffer_write(bp);
			buffer_release(bp);

			off += count;
			src += count;
			n -= count;
			tot += count;
		}
		return tot;
	}

	while(n > 0){
		uint32_t lbn = off / BSIZE;
		uint32_t boff = off % BSIZE;
		uint32_t sector = bmap(ip, lbn);

		bp = buffer_read(ip->dev, sector);

		int count = BSIZE - boff;
		if(count > n)
			count = n;

		memmove(bp->data + boff, src, count);

		log_write(bp);
		buffer_release(bp);

		off += count;
		src += count;
		n -= count;
		tot += count;
	}

	if(off > ip->size){
		ip->size = off;
		iupdate(ip);
	}

	return tot;
}

