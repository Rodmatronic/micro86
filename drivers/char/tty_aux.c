/*
 * Alternative TTY devices
 */

#include <types.h>
#include <defs.h>
#include <param.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>
#include <errno.h>

extern int consoleread(int minor, struct inode *ip, char *dst, int n, uint32_t off);
extern int consolewrite(int minor, struct inode *ip, char *dst, int n, uint32_t off);

int ttyauxwrite(int minor, struct inode *ip, char *src, int n, uint32_t off){
	switch (minor){
		case 0: // current TTY device
			return -ENXIO;
		case 1: // console
			return consolewrite(minor, ip, src, n, off);
		case 2: // PTY master multiplex
			return -ENXIO;
		default:
			return -ENXIO;
	}
}

int ttyauxread(int minor, struct inode *ip, char *src, int n, uint32_t off){
	switch (minor){
		case 0: // current TTY device
			return -ENXIO;
		case 1: // console
			return consoleread(minor, ip, src, n, off);
		case 2: // PTY master multiplex
			return -ENXIO;
		default:
			return -ENXIO;
	}
}

