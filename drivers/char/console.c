// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include <types.h>
#include <defs.h>
#include <param.h>
#include <traps.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <fs.h>
#include <file.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <tty.h>
#include <stdarg.h>
#include <signal.h>
#include <config.h>
#include <termios.h>

struct {
	char buf[INPUT_BUF];
	uint32_t r;	// Read index
	uint32_t w;	// Write index
	uint32_t e;	// Edit index
} input;

int current_color = 0x0700;
struct cons cons;
int x, y = 0;
unsigned short *crt = (ushort*)P2V(0xb8000);	// CGA memory
int pos = 0;

/* Format a string and print it on the screen, just like the libc
 * function printf. */
void printf(const char *format, ...){
	char **arg = (char **) &format;
	int c;
	char buf[20];

	arg++;
	while ((c = *format++) != 0){
		if (c != '%')
			console_putc(c);
		else {
			char *p, *p2;
			int pad0 = 0, pad = 0;

			c = *format++;
			if (c == '0'){
				pad0 = 1;
				c = *format++;
			}

			if (c >= '0' && c <= '9'){
				pad = c - '0';
				c = *format++;
			}

			switch (c){
				case 'd':
				case 'u':
				case 'x':
					itoa (buf, c, *((int *) arg++));
					p = buf;
					goto string;
					break;

				case 's':
					p = *arg++;
					if (! p)
						p = "(null)";

				string:
					for (p2 = p; *p2; p2++);
					for (; p2 < p + pad; p2++)
						console_putc(pad0 ? '0' : ' ');
					while (*p)
						console_putc(*p++);
					break;

				default:
					console_putc(*((int *) arg++));
					break;
			}
		}
	}
}

/* This is the same as normal printf, but uses va_args. */
void vprintf(const char *format, va_list args){
	int c;
	char buf[20];

	while ((c = *format++) != 0){
		if (c != '%')
			console_putc(c);
		else {
			char *p, *p2;
			int pad0 = 0, pad = 0;

			c = *format++;
			if (c == '0'){
				pad0 = 1;
				c = *format++;
			}

			if (c >= '0' && c <= '9'){
				pad = c - '0';
				c = *format++;
			}

			switch (c){
				case 'd':
				case 'u':
				case 'x':
					itoa(buf, c, va_arg(args, int));
					p = buf;
					goto string;

				case 's':
					p = va_arg(args, char*);
					if (!p)
						p = "(null)";

				string:
					for (p2 = p; *p2; p2++);
					for (; p2 < p + pad; p2++)
						console_putc(pad0 ? '0' : ' ');
					while (*p)
						console_putc(*p++);
					break;

				default:
					console_putc(va_arg(args, int));
					break;
			}
		}
	}
}

/* Used by printk to print to console */
void _printk(const char *func, const char *fmt, ...){
	int locking;
	uint32_t us;
	va_list args;

	locking = cons.locking; // Aquire console lock
	if (locking)
		acquire(&cons.lock);

	/* Colour changes are to differenciate between the time, kernel function, and
	 * message. The colours used are copied from what is seen in Linux. */
	tty_sgr(&ttys[active_tty], 0x0200);
	printf("[ ");

	us = tsc_calibrated ? 0 : tsc_to_us(rdtsc());
	printf("%4u.%06u", us / 1000000, us % 1000000);

	printf("] ");
	tty_sgr(&ttys[active_tty], 0x0600);
	printf("%s: ", func);
	tty_sgr(&ttys[active_tty], 0x0F00);

	va_start(args, fmt);
	vprintf(fmt, args); // print the message
	va_end(args);

	tty_sgr(&ttys[active_tty], 0x0700);

	if (locking)
		release(&cons.lock);
}

void cgaputc(int c){
	int rflag = 0;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	int spaces = 8 - (pos % 8);

	switch(c) {
		case('\n'):
			pos += 80 - pos%80;
			break;

		case('\r'):
			pos -= pos % 80;
			rflag = 1;
			break;

		case(BACKSPACE):
			if (pos > 0) -- pos;
			break;

	case('\t'):
		for (int i = 0; i < spaces; i++){
			crt[pos++] = ' ' | current_color;
			if ((pos/80) >= 25){
				memmove(crt, crt+80, sizeof(crt[0])*24*80);
				pos -= 80;
				memset(crt+pos, 0, sizeof(crt[0])*(25*80 - pos));
			}
		}
		break;

	default:
		if ((c == 0x00))
			break;
		if ((c & 0xff) < 0x20){
			crt[pos++] = '^' | current_color;
			crt[pos++] = ((c & 0xff) + '@') | current_color;
		}else{
			crt[pos++] = (c & 0xff) | current_color;
		}
		break;
	}

	if ((pos/80) >= 25){	// Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*24*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(25*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	if (!rflag)
		crt[pos] = ' ' | 0x0700;
}

void console_putc(int c){
	if (panicked){
		cli();
		for (;;);
	}
	if (c == '\t'){
		for (int i = 0; i < 8; i++)
			uart_putc(' ');
	} else if (c == BACKSPACE){
		uart_putc('\b'); uart_putc(' '); uart_putc('\b');
	} else {
		uart_putc(c);
	}

	if (uart_debug)	// this is for the serial debug line, don't print to vga
		return;

	tty_putc(&ttys[active_tty], c);
}

/*
 * This interrupt is for typing on the dumb console, we don't do ANSI emulation, don't worry about
 * termios, and simply get text onto the VGA/CGA card.
 */
void console_interrupt(int (*getc)(void)){
	int c;
	acquire(&cons.lock);
	if (active_tty == -1){
		while ((c = getc()) >= 0){
			switch(c){
			case C('U'):	// Kill line.
				while (input.e != input.w && input.buf[(input.e-1) % INPUT_BUF] != '\n'){
					input.e--;
					cgaputc(BACKSPACE);
				}
				break;
			case C('H'): case '\x7f':	// Backspace
				if (input.e != input.w){
					input.e--;
						if ((input.buf[input.e % INPUT_BUF] & 0xff) < 0x20){
						cgaputc(BACKSPACE);
						cgaputc(BACKSPACE);
					} else {
						cgaputc(BACKSPACE);
					}
				}
				break;
			case '\t':	// Tab
				if (input.e-input.r < INPUT_BUF - 8){	// ensure space for 8 spaces
					for (int i = 0; i < 8; i++){
						input.buf[input.e++ % INPUT_BUF] = ' ';
						cgaputc(' ');
					}
					if (input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
			break;
			default:
				if (c != 0 && input.e-input.r < INPUT_BUF){
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					cgaputc(c);
					if (c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
				break;
			}
		}
	} else {
		release(&cons.lock);
		return;
	}
	release(&cons.lock);
}

/*
 * When reading from dumb terminal, we check to see if we are said terminal, then just take the
 * keypresses.
 */
int consoleread(int minor, struct inode *ip, char *dst, int n, uint32_t off){
	uint32_t target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while (n > 0){
		if (active_tty == -1){
		while (input.r == input.w){
			if (myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if (c == C('D')){	// EOF
			if (n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if (c == '\n')
			break;
		}
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

/*
 * If something writes to the dumb terminal, simply echo it right to VGA if we can.
 */
int consolewrite(int minor, struct inode *ip, char *buf, int n, uint32_t off){
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for (i = 0; i < n; i++) {
		if (active_tty == -1){
			cgaputc(buf[i] & 0xff);
		}
	}
	release(&cons.lock);
	ilock(ip);

	return n;
}

void console_init(void){
	initlock(&cons.lock, "console");
	cons.locking = 1;

	// 14-line tall cursor for visibility
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | 0x0E);
}

