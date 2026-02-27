// ../include/tty.h
#ifndef _TTY_H_
#define _TTY_H_

#include <spinlock.h>
#include <termios.h>
#include <param.h>

#define INPUT_BUF 128
#define C(x)	((x)-'@')	// Control-x
#define BACKSPACE 0x100
#define CRTPORT 0x3d4

#define TTY_ROWS 25
#define TTY_COLS 80

extern unsigned short *crt;
extern int pos;

struct cons {
	struct spinlock lock;
	int locking;
};

struct tty {
        int num;	// TTY minor number
	int pgrp;	// TTY program group
	int attached_console;	// VGA or PTY (unused for now)
	struct termios termios;	// TTY specific termios struct
	struct spinlock lock;	// TTY lock
	uint16_t screen[TTY_ROWS * TTY_COLS];	// Sreen-sized buffer for text
	int pos;	// CRT position

	enum {
		ANSI_NORMAL,
		ANSI_ESCAPE,
		ANSI_BRACKET,
		ANSI_PARAM
	} ansi_state;

	int ansi_params[8];
	int ansi_param_count;
	int ansi_private;
	int ansi_sgr;	// ANSI foreground colour
	char input_buf[INPUT_BUF];
	uint32_t input_r;	// Read index
	uint32_t input_w;	// Write index
	uint32_t input_e;	// Edit index
};

extern int active_tty;	// active TTY getting all the attention (keyboard)
extern struct tty ttys[NTTYS];	// TTY index

#endif
