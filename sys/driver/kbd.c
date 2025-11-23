#include <types.h>
#include <x86.h>
#include <defs.h>
#include <kbd.h>

int
kgetchar(void) {
	uchar stat, data;

	for (;;) {
		stat = inb(0x64);
		if ((stat & 0x01) != 0) {
			data = inb(0x60);  // Read scancode

			if (!(data & 0x80) && (data < 0x80)) {
				char c = normalmap[data];
				if (c != 0) return c;  // Valid character
			}
		}
		asm volatile("pause");
	}
}

int
is_mouse_data()
{
		return (inb(0x64) & 0x20) != 0;
}

int
kbdgetc(void)
{
	static uint shift;
	static uchar *charcode[4] = {
		normalmap, shiftmap, ctlmap, ctlmap
	};
	uint st, data, c;
	if (is_mouse_data()) {
		return -1;
	}

	st = inb(KBSTATP);
	if((st & KBS_DIB) == 0)
		return -1;
	data = inb(KBDATAP);

	if(data == 0xE0){
		shift |= E0ESC;
		return 0;
	} else if(data & 0x80){
		// Key released
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	} else if(shift & E0ESC){
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];
	c = charcode[shift & (CTL | SHIFT)][data];
	if(shift & CAPSLOCK){
		if('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}
	switch (c) {
		case KEY_UP: return 0xE100;
		case KEY_DN: return 0xE101;
		case KEY_LF: return 0xE102;
		case KEY_RT: return 0xE103;
	}
	return c;
}

void kbdintr(void) {
	consoleintr(kbdgetc);
}
