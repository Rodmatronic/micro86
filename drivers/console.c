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

#define INPUT_BUF 128
#define C(x)	((x)-'@')	// Control-x
#define BACKSPACE 0x100
#define CRTPORT 0x3d4

struct ttyb ttyb = {
	.speeds = 0,	// Initial speeds
	.erase = '\b',	// Backspace
	.kill = '\025',	// Ctrl+U
	.tflags = ECHO	// Enable echo by default
};

enum ansi_state {
	ANSI_NORMAL,
	ANSI_ESCAPE,
	ANSI_BRACKET,
	ANSI_PARAM
};

struct {
	char buf[INPUT_BUF];
	unsigned int r;	// Read index
	unsigned int w;	// Write index
	unsigned int e;	// Edit index
} input;

static enum ansi_state ansi_state = ANSI_NORMAL;
static int ansi_params[4];
static int ansi_param_count = 0;
int current_color = 0x0700;
struct cons cons;
int x, y = 0;
static ushort *crt = (ushort*)P2V(0xb8000);	// CGA memory

static void printint(unsigned int xx, int base, int sgn, int width, int zero_pad){
	static char digits[] = "0123456789ABCDEF";
	char buf[16];
	int i, neg;
	unsigned int x;

	neg = 0;
	if(sgn) {
		if ((int)xx < 0) {
			neg = 1;
			x = (unsigned int)(-(int)xx);
		} else {
			x = xx;
		}
	} else {
		x = xx;
	}

	i = 0;
	do {
		buf[i++] = digits[x % base];
	} while((x /= base) != 0);

	int total_digits = i;
	int total_chars = total_digits + (neg ? 1 : 0);
	int pad = width > total_chars ? width - total_chars : 0;

	if (zero_pad) {
		if (neg) consputc('-');
		for (int j = 0; j < pad; j++) consputc('0');
		while (--i >= 0) consputc(buf[i]);
	}
	else {
		for (int j = 0; j < pad; j++) consputc(' ');
		if (neg) consputc('-');
		while (--i >= 0) consputc(buf[i]);
	}
}

static void vprintf(int fd, const char *fmt, va_list ap){
	char *s;
	int c, i, state;
	int width, zero_pad;

	state = 0;
	for(i = 0; fmt[i]; i++) {
		c = fmt[i] & 0xff;
		if(state == 0) {
			if(c == '%') {
				state = '%';
				width = 0;
				zero_pad = 0;
			} else {
				consputc(c);
			}
		} else if(state == '%') {
			if(c == '0') {
				zero_pad = 1;
				i++;
				c = fmt[i] & 0xff;
			}

			while(c >= '0' && c <= '9') {
				width = width * 10 + (c - '0');
				i++;
				c = fmt[i] & 0xff;
			}
			if(c == 'l') {
				i++;
				c = fmt[i] & 0xff;
			}
			if(c == 'd') {
				printint((unsigned int)va_arg(ap, int), 10, 1, width, zero_pad);
			} else if(c == 'u') {
				printint(va_arg(ap, unsigned int), 10, 0, width, zero_pad);
			} else if(c == 'x' || c == 'p') {
				printint(va_arg(ap, unsigned int), 16, 0, width, zero_pad);
			} else if(c == 's') {
				s = va_arg(ap, char*);
				if(s == 0) s = "(null)";
				while(*s != 0) {
					consputc(*s);
					s++;
				}
			} else if(c == 'c') {
				consputc(va_arg(ap, int));
			} else if(c == '%') {
				consputc(c);
			} else {
				consputc('%');
				consputc(c);
			}
			state = 0;
		}
	}
}

void vkprintf(const char *fmt, va_list ap){
	vprintf(1, fmt, ap);
}

int vsprintf(char *buf, const char *fmt, va_list ap){
	char *start = buf;
	vprintf(1, fmt, ap);
	*buf = '\0';
	return buf - start;
}

int sprintf(char *buf, const char *fmt, ...){
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsprintf(buf, fmt, ap);
	va_end(ap);
	return rc;
}

void _printf(char *func, char *fmt, ...){
	va_list ap;
	int locking;
	int old_color;

	old_color = current_color;
#ifdef CONFIG_PRINTK_TIME
	unsigned int us, s, rem;
#endif

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

#ifdef CONFIG_PRINTK_TIME
#ifdef CONFIG_COLORFUL_KMESG
	current_color = 0x0200;
#endif

	consputc('[');
	consputc(' ');

	if(tsc_calibrated) {
		us = 0;
	} else {
		us = tsc_to_us(rdtsc());
	}

	s = us / 1000000;
	rem = us % 1000000;

	printint(s, 10, 0, 4, 0);
	consputc('.');
	printint(rem, 10, 0, 6, 1);

	consputc(']');
	consputc(' ');

#ifdef CONFIG_COLORFUL_KMESG
	current_color = 0x600;
#endif

	vkprintf(func, 0);
	consputc(':');
	consputc(' ');
#endif
	current_color = 0x0F00;
	va_start(ap, fmt);
	vkprintf(fmt, ap);
	va_end(ap);

	current_color = old_color;

	if(locking)
		release(&cons.lock);
}

void cgaputc(int c){
	int pos;

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

		case(BACKSPACE):
			if(pos > 0) -- pos;
			break;

	case('\t'):
		for(int i = 0; i < spaces; i++){
			crt[pos++] = ' ' | current_color;
			if((pos/80) >= 25){
				memmove(crt, crt+80, sizeof(crt[0])*24*80);
				pos -= 80;
				memset(crt+pos, 0, sizeof(crt[0])*(25*80 - pos));
			}
		}
		break;

	default:
		if((c == 0x00))
			break;
		if((c & 0xff) < 0x20){
			crt[pos++] = '^' | current_color;
			crt[pos++] = ((c & 0xff) + '@') | current_color;
		}else{
			crt[pos++] = (c & 0xff) | current_color;
		}
		break;
	}

	if((pos/80) >= 25){	// Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*24*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(25*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void handle_ansi_sgr(int param);

void setcursor(int x, int y){
	if(x < 0) x = 0;
	if(x > 79) x = 79;
	if(y < 0) y = 0;
	if(y > 24) y = 24;
	int pos = y * 80 + x;
	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos >> 8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos & 0xFF);
}

void handle_ansi_sgr_sequence(int params[], int count){
	for(int i = 0; i < count; i++) {
		handle_ansi_sgr(params[i]);
	}
}

void handle_ansi_clear(int param){
	if(param == 2 || param == 0) { // 2J = clear entire screen, 0J = clear from cursor
		setcursor(0, 0);
		for (int i = 0; i < 80*25; i++) {
			crt[i] = ' ' | 0x0700;
		}
	} else if(param == 1) { // clear from top to cursor
		outb(CRTPORT, 14);
		int pos = inb(CRTPORT+1) << 8;
		outb(CRTPORT, 15);
		pos |= inb(CRTPORT+1);
		for(int i = 0; i <= pos; i++)
			crt[i] = ' ' | 0x0700;
	}
}

void handle_ansi_sgr(int param){
	switch(param) {
		case 0:	// reset
			current_color = 0x0700; // white on black
			break;
		case 1:	// bold
			current_color = (current_color & 0x0F00) | 0x0800;
			break;
		case 30: // black foreground
			current_color = (current_color & 0xF0FF) | 0x0000;
			break;
		case 31: // red foreground
			current_color = (current_color & 0xF0FF) | 0x0400;
			break;
		case 32: // green foreground
			current_color = (current_color & 0xF0FF) | 0x0200;
			break;
		case 33: // yellow foreground
			current_color = (current_color & 0xF0FF) | 0x0600;
			break;
		case 34: // blue foreground
			current_color = (current_color & 0xF0FF) | 0x0100;
			break;
		case 35: // magenta foreground
			current_color = (current_color & 0xF0FF) | 0x0500;
			break;
		case 36: // cyan foreground
			current_color = (current_color & 0xF0FF) | 0x0300;
			break;
		case 37: // white foreground
			current_color = (current_color & 0xF0FF) | 0x0700;
			break;

		case 40: // black background
			current_color = (current_color & 0x0FFF) | 0x0000;
			break;
		case 41: // red background
			current_color = (current_color & 0x0FFF) | 0x4000;
			break;
		case 42: // green background
			current_color = (current_color & 0x0FFF) | 0x2000;
			break;
		case 43: // yellow background
			current_color = (current_color & 0x0FFF) | 0x6000;
			break;
		case 44: // blue background
			current_color = (current_color & 0x0FFF) | 0x1000;
			break;
		case 45: // magenta background
			current_color = (current_color & 0x0FFF) | 0x5000;
			break;
		case 46: // cyan background
			current_color = (current_color & 0x0FFF) | 0x3000;
			break;
		case 47: // white background
			current_color = (current_color & 0x0FFF) | 0x7000;
			break;

		case 90: // hi black foreground
			current_color = (current_color & 0xF0FF) | 0x0800;
			break;
		case 91: // hi red foreground
			current_color = (current_color & 0xF0FF) | 0x0C00;
			break;
		case 92: // hi green foreground
			current_color = (current_color & 0xF0FF) | 0x0A00;
			break;
		case 93: // hi yellow foreground
			current_color = (current_color & 0xF0FF) | 0x0E00;
			break;
		case 94: // hi blue foreground
			current_color = (current_color & 0xF0FF) | 0x0900;
			break;
		case 95: // hi magenta foreground
			current_color = (current_color & 0xF0FF) | 0x0D00;
			break;
		case 96: // hi cyan foreground
			current_color = (current_color & 0xF0FF) | 0x0B00;
			break;
		case 97: // hi white foreground
			current_color = (current_color & 0xF0FF) | 0x0F00;
			break;

		case 100: // hi black background
			current_color = (current_color & 0x0FFF) | 0x8000;
			break;
		case 101: // hi red background
			current_color = (current_color & 0x0FFF) | 0xC000;
			break;
		case 102: // hi green background
			current_color = (current_color & 0x0FFF) | 0xA000;
			break;
		case 103: // hi yellow background
			current_color = (current_color & 0x0FFF) | 0xE000;
			break;
		case 104: // hi blue background
			current_color = (current_color & 0x0FFF) | 0x9000;
			break;
		case 105: // hi magenta background
			current_color = (current_color & 0x0FFF) | 0xD000;
			break;
		case 106: // hi cyan background
			current_color = (current_color & 0x0FFF) | 0xB000;
			break;
		case 107: // hi white background
			current_color = (current_color & 0x0FFF) | 0xF000;
			break;
	}
}

void consputc(int c){
	if(panicked){
		cli();
		for(;;);
	}
	if(c == '\t'){
		for(int i = 0; i < 8; i++)
			uartputc(' ');
	} else if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else {
		uartputc(c);
	}

	if (uart_debug)	// this is for the serial debug line, don't print to vga
		return;

	switch(ansi_state) {
		case ANSI_NORMAL:
			if(c == 0x1B) { // ESC
				ansi_state = ANSI_ESCAPE;
				ansi_param_count = 0;
				return;
			} else {
				cgaputc(c);	// normal
				return;
			}
			break;
			
		case ANSI_ESCAPE:
			if(c == '[') {
				ansi_state = ANSI_BRACKET;
			} else {
				ansi_state = ANSI_NORMAL;
			}
			return;

		case ANSI_BRACKET:
			ansi_param_count = 0;
			memset(ansi_params, 0, sizeof(ansi_params));
			if (c >= '0' && c <= '9') {
				ansi_params[0] = c - '0';
				ansi_param_count = 1;
				ansi_state = ANSI_PARAM;
			} else if (c == 'm') {
				handle_ansi_sgr(0);
				ansi_state = ANSI_NORMAL;
			} else if (c == 'H') {
				setcursor(1, 1);
				ansi_state = ANSI_NORMAL;
			} else {
				ansi_state = ANSI_NORMAL; // invalid
			}
			return;

		case ANSI_PARAM:
			if (c >= '0' && c <= '9') {
				ansi_params[ansi_param_count - 1] =
					ansi_params[ansi_param_count - 1] * 10 + (c - '0');
				return;
			} else if (c == ';') {
				if (ansi_param_count < 4)
					ansi_param_count++;
				return;
			} else if(c == 'm') {
				handle_ansi_sgr_sequence(ansi_params, ansi_param_count);
				ansi_state = ANSI_NORMAL;
				return;
			} else if (c == 'J') {
				int param = ansi_param_count > 0 ? ansi_params[0] : 0;
				handle_ansi_clear(param);
				ansi_state = ANSI_NORMAL;
				return;
			} else if (c == 'f') {
				int row = (ansi_param_count >= 1 ? ansi_params[0] : 1) - 1;
				int col = (ansi_param_count >= 2 ? ansi_params[1] : 1) - 1;
				setcursor(col, row);
				ansi_state = ANSI_NORMAL;
				return;
			} else {
				ansi_state = ANSI_NORMAL; // not valid
				return;
			}
		return;
	}

	cgaputc(c);
}

void consoleintr(int (*getc)(void)){
	int c;
	acquire(&cons.lock);
	while((c = getc()) >= 0){
		if (c >= 0xE100 && c <= 0xE103) {
			char seq[3];
			seq[0] = 0x1B;
			seq[1] = '[';
			switch (c) {
				case 0xE100: seq[2] = 'A'; break; // up
				case 0xE101: seq[2] = 'B'; break; // down
				case 0xE102: seq[2] = 'D'; break; // left
				case 0xE103: seq[2] = 'C'; break; // right
			}
			for (int i = 0; i < 3; i++) {
				if (input.e - input.r < INPUT_BUF) {
					input.buf[input.e++ % INPUT_BUF] = seq[i];
					if (ttyb.tflags & ECHO)
						consputc(seq[i]);
				}
			}
			continue;
		}
		switch(c){
		case C('U'):	// Kill line.
			while(input.e != input.w && input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('C'):	// Send interrupt signal
			break;
		case C('H'): case '\x7f':	// Backspace
			if(input.e != input.w){
				input.e--;
				if(ttyb.tflags & ECHO){
					if((input.buf[input.e % INPUT_BUF] & 0xff) < 0x20){
						consputc(BACKSPACE);
						consputc(BACKSPACE);
					}else{
						consputc(BACKSPACE);
					}
				}
			}
			break;
		case '\t':	// Tab
			if(input.e-input.r < INPUT_BUF - 8){	// ensure space for 8 spaces
				for(int i = 0; i < 8; i++){
					input.buf[input.e++ % INPUT_BUF] = ' ';
					if(ttyb.tflags & ECHO)
						consputc(' ');
				}
				if(input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
		break;

		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				if(ttyb.tflags & ECHO)
					consputc(c);
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
}

int consoleread(struct inode *ip, char *dst, int n, uint32_t off){
	unsigned int target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){	// EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n, uint32_t off){
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void consoleinit(void){
	initlock(&cons.lock, "console");

	cons.locking = 1;
	ttyb.tflags = ECHO;

	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | 0x0E);

	ioapicenable(IRQ_KBD, 0);
}

