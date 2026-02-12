/*
 * TTY devices
 */

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
#include <errno.h>

int tty_write_output(struct tty *tty, char *src, int n);
void tty_putc(struct tty *tty, int c);
extern void cgaputc(int c);
void handle_ansi_sgr(struct tty *tty, int param);

int active_tty = 1;	// default tty of 1
struct tty ttys[NTTYS];

struct termios default_termios = {
	.c_iflag = ICRNL | IXON,
	.c_oflag = OPOST | ONLCR,
	.c_cflag = CS8 | CREAD | CLOCAL,
	.c_lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN,
	.c_line = 0,
	.c_cc = {
		[VINTR]	= 0x03,  // ^C
		[VQUIT]	= 0x1C,  // backslash
		[VERASE]   = 0x7F,  // DEL
		[VKILL]	= 0x15,  // ^U
		[VEOF]	 = 0x04,  // ^D
		[VTIME]	= 0,
		[VMIN]	 = 1,
		[VSTART]   = 0x11,  // ^Q
		[VSTOP]	= 0x13,  // ^S
		[VSUSP]	= 0x1A,  // ^Z
		[VEOL]	 = 0,
		[VREPRINT] = 0x12,  // ^R
		[VDISCARD] = 0x0F,  // ^O
		[VWERASE]  = 0x17,  // ^W
		[VLNEXT]   = 0x16,  // ^V
		[VEOL2]	= 0,
	},
	.__c_ispeed = B38400,
	.__c_ospeed = B38400,
};

void set_cursor(struct tty *tty, int x, int y){
	if (x < 0) x = 0;
	if (x > 79) x = 79;
	if (y < 0) y = 0;
	if (y > 24) y = 24;
	int pos = y * 80 + x;
	tty->pos = pos;
}

void handle_ansi_sgr_sequence(struct tty *tty, int params[], int count){
	for (int i = 0; i < count; i++)
		handle_ansi_sgr(tty, params[i]);
}

void handle_ansi_clear(struct tty *tty, int param){
	if (param == 2 || param == 0){ // 2J = clear entire screen, 0J = clear from cursor
		set_cursor(tty, 0, 0);
		for (int i = 0; i < 80*25; i++){
			tty->screen[i] = ' ' | 0x0700;
			if (tty->attached_console){
				if (tty->num == active_tty){
					crt[i] = ' ' | 0x0700;
				}
			}
		}
	} else if (param == 1){ // clear from top to cursor
		for (int i = 0; i <= pos; i++){
			tty->screen[i] = ' ' | 0x0700;
			if (tty->attached_console){
				if (tty->num == active_tty){
					crt[i] = ' ' | 0x0700;
				}
			}
		}
	}
}

void handle_ansi_sgr(struct tty *tty, int param){
	switch(param){
		case 0:	// reset
			tty->ansi_sgr = 0x0700; // white on black
			break;
		case 1:	// bold
			tty->ansi_sgr = (tty->ansi_sgr & 0x0F00) | 0x0800;
			break;
		case 30: // black foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0000;
			break;
		case 31: // red foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0400;
			break;
		case 32: // green foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0200;
			break;
		case 33: // yellow foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0600;
			break;
		case 34: // blue foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0100;
			break;
		case 35: // magenta foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0500;
			break;
		case 36: // cyan foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0300;
			break;
		case 37: // white foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0700;
			break;

		case 40: // black background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x0000;
			break;
		case 41: // red background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x4000;
			break;
		case 42: // green background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x2000;
			break;
		case 43: // yellow background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x6000;
			break;
		case 44: // blue background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x1000;
			break;
		case 45: // magenta background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x5000;
			break;
		case 46: // cyan background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x3000;
			break;
		case 47: // white background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x7000;
			break;

		case 90: // hi black foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0800;
			break;
		case 91: // hi red foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0C00;
			break;
		case 92: // hi green foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0A00;
			break;
		case 93: // hi yellow foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0E00;
			break;
		case 94: // hi blue foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0900;
			break;
		case 95: // hi magenta foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0D00;
			break;
		case 96: // hi cyan foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0B00;
			break;
		case 97: // hi white foreground
			tty->ansi_sgr = (tty->ansi_sgr & 0xF0FF) | 0x0F00;
			break;

		case 100: // hi black background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x8000;
			break;
		case 101: // hi red background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0xC000;
			break;
		case 102: // hi green background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0xA000;
			break;
		case 103: // hi yellow background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0xE000;
			break;
		case 104: // hi blue background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0x9000;
			break;
		case 105: // hi magenta background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0xD000;
			break;
		case 106: // hi cyan background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0xB000;
			break;
		case 107: // hi white background
			tty->ansi_sgr = (tty->ansi_sgr & 0x0FFF) | 0xF000;
			break;
	}
}

/*
 * Keypress on a tty
 */
void tty_interrupt(int (*getc)(void)){
	int c;
	struct tty *tty;
	int ttynum = active_tty;

	if (ttynum < 0 || ttynum >= NTTYS)
		return;

	tty = &ttys[ttynum];
	while ((c = getc()) >= 0){
		switch(c){
		case C('U'):	// Kill line.
			while (tty->input_e != tty->input_w && tty->input_buf[(tty->input_e-1) % INPUT_BUF] != '\n'){
				tty->input_e--;
				if (tty->attached_console)
					tty_putc(tty, BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':	// Backspace
			if (tty->input_e != tty->input_w){
				tty->input_e--;
				if (tty->termios.c_lflag & ECHO){
					if ((tty->input_buf[tty->input_e % INPUT_BUF] & 0xff) < 0x20){
						if (tty->attached_console){
							tty_putc(tty, BACKSPACE);
							tty_putc(tty, BACKSPACE);
						}
					} else {
						if (tty->attached_console)
							tty_putc(tty, BACKSPACE);
					}
				}
			}
			break;
		case '\t':	// Tab
			if (tty->input_e-tty->input_r < INPUT_BUF - 8){	// ensure space for 8 spaces
				for (int i = 0; i < 8; i++){
					tty->input_buf[tty->input_e++ % INPUT_BUF] = ' ';
					if (tty->termios.c_lflag & ECHO)
						if (tty->attached_console)
							tty_putc(tty, ' ');
				}
				if (tty->input_e == tty->input_r+INPUT_BUF){
					tty->input_w = tty->input_e;
					wakeup(&tty->input_r);
				}
			}
		break;

		default:
			if (c != 0 && tty->input_e-tty->input_r < INPUT_BUF){
				tty->input_buf[tty->input_e++ % INPUT_BUF] = c;
				if (tty->termios.c_lflag & ECHO)
					if (tty->attached_console)
						tty_putc(tty, c);
				if (c == '\n' || c == C('D') || tty->input_e == tty->input_r+INPUT_BUF){
					tty->input_w = tty->input_e;
					wakeup(&tty->input_r);
				}
			}
			break;
		}
	}
}

/*
 * Switch over the context to another virtual TTY
 */
int change_tty(int num){
	uint16_t *crt = (uint16_t*)P2V(0xB8000);
	struct tty *old, *new;

	if (num > NTTYS - 1)	// very important
		return -EINVAL;

	old = &ttys[active_tty];
	new = &ttys[num];

	for (int i = 0; i < TTY_ROWS * TTY_COLS; i++) {
		old->screen[i] = crt[i];
	}

	active_tty = num;

	for (int i = 0; i < TTY_ROWS * TTY_COLS; i++) {
		crt[i] = new->screen[i];
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, new->pos >> 8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, new->pos);
	return 0;
}

void dumb_putc(struct tty *tty, int c){
	crt[tty->pos - 1] = (c & 0xff) | tty->ansi_sgr;
}

void tty_sgr(struct tty *tty, int sgr){
	tty->ansi_sgr = (tty->ansi_sgr & ~0x0F00) | sgr;
}

/*
 * Output to the TTY or VGA console if available.
 */
void tty_putc(struct tty *tty, int c){
	int rflag = 0;
	int spaces = 8 - (tty->pos % 8);

	switch(c) {
		case('\n'):	// newline
			tty->pos += 80 - tty->pos%80;
			break;

		case('\r'):	// carriage return
			tty->pos -= tty->pos % 80;
			rflag = 1;
			break;

		case(BACKSPACE):
			if (tty->pos > 0) -- tty->pos;
			break;

	case('\t'):	// tab
		for (int i = 0; i < spaces; i++){
			tty->screen[tty->pos++] = ' ' | tty->ansi_sgr;
			if ((pos/80) >= 25){
				memmove(tty->screen, tty->screen+80, sizeof(tty->screen[0])*24*80);
				tty->pos -= 80;
				memset(tty->screen+tty->pos, 0, sizeof(tty->screen[0])*(25*80 - tty->pos));
				if (tty->attached_console){
					if (tty->num == active_tty){
						for (int i = 0; i < TTY_ROWS * TTY_COLS; i++) {
							crt[i] = tty->screen[i];
						}
					}
				}
			}
		}
		break;

	default:
		if ((c == 0x00))
			break;
		if ((c & 0xff) < 0x20){	// special characters
			tty->screen[tty->pos++] = '^' | tty->ansi_sgr;
			tty->screen[tty->pos++] = ((c & 0xff) + '@') | tty->ansi_sgr;
			if (tty->attached_console){
				if (tty->num == active_tty){
					crt[tty->pos-2] = '^' | tty->ansi_sgr;
					crt[tty->pos-1] = ((c & 0xff) + '@') | tty->ansi_sgr;
				}
			}
		} else {	// normal characters
			tty->screen[tty->pos++] = (c & 0xff) | tty->ansi_sgr;
			if (tty->attached_console){
				if (tty->num == active_tty){
					dumb_putc(tty, c);
				}
			}
		}
		break;
	}

	if ((tty->pos/80) >= 25){	// Scroll up.
		memmove(tty->screen, tty->screen+80, sizeof(tty->screen[0])*24*80);
		tty->pos -= 80;
		memset(tty->screen+tty->pos, 0, sizeof(tty->screen[0])*(25*80 - tty->pos));
		if (tty->attached_console){
			if (tty->num == active_tty){
				for (int i = 0; i < TTY_ROWS * TTY_COLS; i++) {
					crt[i] = tty->screen[i];
				}
			}
		}
	}

	if (!rflag) {	// don't do this when printing \r
		tty->screen[tty->pos] = ' ' | 0x0700;
		if (tty->attached_console){
			if (tty->num == active_tty){
				crt[tty->pos] = ' ' | 0x0700;
			}
		}
	}

	if (tty->attached_console){
		if (tty->num == active_tty){
			outb(CRTPORT, 14);
			outb(CRTPORT+1, tty->pos>>8);
			outb(CRTPORT, 15);
			outb(CRTPORT+1, tty->pos);
		}
	}

	// TODO: If this is a PTY, write to master side
	// TODO: If logging is enabled, write to log
}

/*
 * Pretty things up before putting characters down to the screen or serial
 */
void termio_putc(struct tty *tty, char c){
	// If output processing is disabled, just pass through
	if (!(tty->termios.c_oflag & OPOST)){
		tty_putc(tty, c);
		return;
	}

	switch (c){
	case '\n':
		if (tty->termios.c_oflag & ONLCR){	// Map NL to CR-NL
			tty_putc(tty, '\r');
			tty_putc(tty, '\n');
		} else {
			tty_putc(tty, '\n');
		}
		break;
		
	case '\r':
		if (tty->termios.c_oflag & OCRNL){	// Map CR to NL
			tty_putc(tty, '\r');
			tty_putc(tty, '\n');
		} else if (!(tty->termios.c_oflag & ONOCR)){ // Don't output CR at column 0 if ONOCR set
			tty_putc(tty, '\r');
		}
		break;

	default:

	switch(tty->ansi_state){
		case ANSI_NORMAL:
			if (c == 0x1B){ // ESC
				tty->ansi_state = ANSI_ESCAPE;
				tty->ansi_param_count = 0;
				return;
			} else {
				tty_putc(tty, c);	// normal
				return;
			}
			break;
			
		case ANSI_ESCAPE:
			if (c == '[')
				tty->ansi_state = ANSI_BRACKET;
			else
				tty->ansi_state = ANSI_NORMAL;
			return;

		case ANSI_BRACKET:
			tty-> ansi_param_count = 0;
			memset(tty->ansi_params, 0, sizeof(tty->ansi_params));
			if (c >= '0' && c <= '9'){
				tty->ansi_params[0] = c - '0';
				tty->ansi_param_count = 1;
				tty->ansi_state = ANSI_PARAM;
			} else if (c == 'm'){
				handle_ansi_sgr(tty, 0);
				tty->ansi_state = ANSI_NORMAL;
			} else if (c == 'H'){
				set_cursor(tty, 0, 0);
				tty->ansi_state = ANSI_NORMAL;
			} else {
				tty->ansi_state = ANSI_NORMAL; // invalid
			}
			return;

			case ANSI_PARAM:
				if (c >= '0' && c <= '9'){
					tty->ansi_params[tty->ansi_param_count - 1] = tty->ansi_params[tty->ansi_param_count - 1] * 10 + (c - '0');
					return;
				} else if (c == ';'){
					if (tty->ansi_param_count < 4)
						tty->ansi_param_count++;
					return;
				} else if (c == 'm'){
					handle_ansi_sgr_sequence(tty, tty->ansi_params, tty->ansi_param_count);
					tty->ansi_state = ANSI_NORMAL;
					return;
				} else if (c == 'J'){
					int param = tty->ansi_param_count > 0 ? tty->ansi_params[0] : 0;
					handle_ansi_clear(tty, param);
					tty->ansi_state = ANSI_NORMAL;
					return;
				} else if (c == 'f'){
					int row = (tty->ansi_param_count >= 1 ? tty->ansi_params[0] : 1) - 1;
					int col = (tty->ansi_param_count >= 2 ? tty->ansi_params[1] : 1) - 1;
					set_cursor(tty, col, row);
					tty->ansi_state = ANSI_NORMAL;
					return;
				} else {
					tty->ansi_state = ANSI_NORMAL; // not valid
					return;
				}
			return;
		}

		tty_putc(tty, c);
		break;
	}
}

int ttywrite(int minor, struct inode *ip, char *src, int n, uint32_t off){
	struct tty *tty;
	int target;

	if (minor == 0){	// /dev/tty
		if (myproc()->tty < 0)
			return -ENXIO;
		target = myproc()->tty;
	} else if (minor >= 1 && minor < NTTYS -1){
		target = minor;
	} else {
		return -ENXIO;
	}

	tty = &ttys[target];
	iunlock(ip);

	for (int i = 0; i < n; i++)
		termio_putc(tty, src[i] & 0xff);

	ilock(ip);
	return n;
}

int ttyread(int minor, struct inode *ip, char *dst, int n, uint32_t off){
	struct tty *tty;
	int target;
	int c;
	
	if (minor == 0){ // /dev/tty
		if (myproc()->tty < 0)
			return -ENXIO;
		target = myproc()->tty;
	} else if (minor >= 1 && minor < NTTYS - 1){
		target = minor;
	} else {
		return -ENXIO;
	}
	
	tty = &ttys[target];
	
	target = n;
	acquire(&tty->lock);
	while (n > 0){
		while (tty->input_r == tty->input_w){
			if (myproc()->killed){
				ilock(ip);
				release(&tty->lock);
				return -1;
			}
			sleep(&tty->input_r, &tty->lock);
		}
		c = tty->input_buf[tty->input_r++ % INPUT_BUF];
		if (c == C('D')){	// EOF
			if (n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				tty->input_r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if (c == '\n')
			break;
	}
	release(&tty->lock);
	return target - n;
}

/*
 * initialize the index of virtual terminals
 */
void tty_init(void){
	uint16_t *crt = (uint16_t*)P2V(0xB8000);

	for (int i = 0; i < NTTYS; i++){
		ttys[i].num = i;
		ttys[i].pgrp = 0;
		ttys[i].input_r = ttys[i].input_w = ttys[i].input_e = 0;
		ttys[i].attached_console = 1;
		ttys[i].pos = 0;
		for (int j = 0; j < TTY_ROWS * TTY_COLS; j++){
			ttys[i].screen[j] = 0x0720;	// black with gray text
		}
		ttys[i].ansi_sgr = 0x0700;
		ttys[i].ansi_param_count = 0;
		ttys[i].termios = default_termios;
	}

	for (int i = 0; i < TTY_ROWS * TTY_COLS; i++){
		ttys[1].screen[i] = crt[i];
	}
	ttys[1].pos = pos;
}
