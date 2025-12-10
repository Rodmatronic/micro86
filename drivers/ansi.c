#include <types.h>
#include <defs.h>

enum ansi_state {
	ANSI_NORMAL,
	ANSI_ESCAPE,
	ANSI_BRACKET,
	ANSI_PARAM
};

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static enum ansi_state ansi_state = ANSI_NORMAL;
static int ansi_params[4];
static int ansi_param_count = 0;

void handle_ansi_sgr(int param);

void
setcursor(int x, int y)
{
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

void
handle_ansi_sgr_sequence(int params[], int count)
{
	for(int i = 0; i < count; i++) {
		handle_ansi_sgr(params[i]);
	}
}

void
handle_ansi_clear(int param)
{
	if(param == 2 || param == 0) { // 2J = clear entire screen, 0J = clear from cursor
		setcursor(0, 0);
		for (int i = 0; i < 80*24; i++) {
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

void
handle_ansi_sgr(int param)
{
	switch(param) {
		case 0:	// reset
			current_colour = 0x0700; // white on black
			break;
		case 1:	// bold
			current_colour = (current_colour & 0x0F00) | 0x0800;
			break;
		case 30: // black foreground
			current_colour = (current_colour & 0xF0FF) | 0x0000;
			break;
		case 31: // red foreground
			current_colour = (current_colour & 0xF0FF) | 0x0400;
			break;
		case 32: // green foreground
			current_colour = (current_colour & 0xF0FF) | 0x0200;
			break;
		case 33: // yellow foreground
			current_colour = (current_colour & 0xF0FF) | 0x0600;
			break;
		case 34: // blue foreground
			current_colour = (current_colour & 0xF0FF) | 0x0100;
			break;
		case 35: // magenta foreground
			current_colour = (current_colour & 0xF0FF) | 0x0500;
			break;
		case 36: // cyan foreground
			current_colour = (current_colour & 0xF0FF) | 0x0300;
			break;
		case 37: // white foreground
			current_colour = (current_colour & 0xF0FF) | 0x0700;
			break;

		case 40: // black background
			current_colour = (current_colour & 0x0FFF) | 0x0000;
			break;
		case 41: // red background
			current_colour = (current_colour & 0x0FFF) | 0x4000;
			break;
		case 42: // green background
			current_colour = (current_colour & 0x0FFF) | 0x2000;
			break;
		case 43: // yellow background
			current_colour = (current_colour & 0x0FFF) | 0x6000;
			break;
		case 44: // blue background
			current_colour = (current_colour & 0x0FFF) | 0x1000;
			break;
		case 45: // magenta background
			current_colour = (current_colour & 0x0FFF) | 0x5000;
			break;
		case 46: // cyan background
			current_colour = (current_colour & 0x0FFF) | 0x3000;
			break;
		case 47: // white background
			current_colour = (current_colour & 0x0FFF) | 0x7000;
			break;

		case 90: // hi black foreground
			current_colour = (current_colour & 0xF0FF) | 0x0800;
			break;
		case 91: // hi red foreground
			current_colour = (current_colour & 0xF0FF) | 0x0C00;
			break;
		case 92: // hi green foreground
			current_colour = (current_colour & 0xF0FF) | 0x0A00;
			break;
		case 93: // hi yellow foreground
			current_colour = (current_colour & 0xF0FF) | 0x0E00;
			break;
		case 94: // hi blue foreground
			current_colour = (current_colour & 0xF0FF) | 0x0900;
			break;
		case 95: // hi magenta foreground
			current_colour = (current_colour & 0xF0FF) | 0x0D00;
			break;
		case 96: // hi cyan foreground
			current_colour = (current_colour & 0xF0FF) | 0x0B00;
			break;
		case 97: // hi white foreground
			current_colour = (current_colour & 0xF0FF) | 0x0F00;
			break;

		case 100: // hi black background
			current_colour = (current_colour & 0x0FFF) | 0x0000;
			break;
		case 101: // hi red background
			current_colour = (current_colour & 0x0FFF) | 0xC000;
			break;
		case 102: // hi green background
			current_colour = (current_colour & 0x0FFF) | 0xA000;
			break;
		case 103: // hi yellow background
			current_colour = (current_colour & 0x0FFF) | 0xE000;
			break;
		case 104: // hi blue background
			current_colour = (current_colour & 0x0FFF) | 0x9000;
			break;
		case 105: // hi magenta background
			current_colour = (current_colour & 0x0FFF) | 0xD000;
			break;
		case 106: // hi cyan background
			current_colour = (current_colour & 0x0FFF) | 0xB000;
			break;
		case 107: // hi white background
			current_colour = (current_colour & 0x0FFF) | 0xF000;
			break;
	}
}

