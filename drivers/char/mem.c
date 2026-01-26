/*
 * Handle MEM_MAJOR read/write
 */

#include <types.h>
#include <defs.h>
#include <param.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>
#include <errno.h>
#include <memlayout.h>

extern int entropy(char * dst, int n);

int memread(int minor, struct inode *ip, char *dst, int n, uint32_t off){
	switch (minor){
		case 1: // mem
			if (off + n > PHYSTOP)
				n = PHYSTOP - off;
			if (n <= 0)
				return 0;
			memmove(dst, P2V(off), n);
			return n;
		case 2: // kmem
			if (off < KERNBASE || off + n > KERNBASE + PHYSTOP)
				return -EFAULT;
			memmove(dst, (char*)off, n);
			return n;
		case 3: // null
			return 0;
		case 4: // port
			return -ENXIO;
		case 5: // zero
			memset(dst, 0, n);
			return n;
		case 6: // core (unused)
			return -ENXIO;
		case 7: // full
			memset(dst, 0, n);
			return n;
		case 8: // random
		case 9: // urandom
			return entropy(dst, n);
		case 10: // AIO
			return -ENXIO;
		case 11: // kmesg
			return -ENXIO;

		default:
			return -ENXIO;
	}
}

int memwrite(int minor, struct inode *ip, char *src, int n, uint32_t off){
	switch (minor){
		case 1: // mem
			if (off + n > PHYSTOP)
				n = PHYSTOP - off;
			if (n <= 0)
				return 0;
			memmove(P2V(off), src, n);
			return n;
		case 2: // kmem
			return -ENXIO;
		case 3: // null
			return n;
		case 4: // port
			return -ENXIO;
		case 5: // zero
			return n;
		case 6: // core (unused)
			return -ENXIO;
		case 7: // full
			return -ENOSPC;
		case 8: // random
		case 9: // urandom
			return n; // this should enhance entropy but whatever, we dont have a pool
		case 10: // AIO
			return -ENXIO;
		case 11: // kmesg
			return -ENXIO;
		default:
			return -ENXIO;
	}
}

