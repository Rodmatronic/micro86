#include <types.h>
#include <x86.h>
#include <defs.h>
#include <memlayout.h>
#include <font.h>

#define VGA_PHYS_START	0xA0000
#define VGA_ADDRESS	P2V(VGA_PHYS_START)
#define VGA_SEQ_INDEX	0x3C4
#define VGA_SEQ_DATA	0x3C5

#define VGA_MAX	( (VGA_MAX_WIDTH / 8) * VGA_MAX_HEIGHT )

// Miscellaneous Output
#define VGA_MISC_READ	0x3CC
#define VGA_MISC_WRITE	0x3C2

// Graphics Controller Registers
#define VGA_GC_INDEX	0x3CE
#define VGA_GC_DATA	0x3CF

// Attribute Controller Registers
#define VGA_AC_INDEX	0x3C0
#define VGA_AC_READ	0x3C1
#define VGA_AC_WRITE	0x3C0

// CRT Controller Registers
#define VGA_CRTC_INDEX	0x3D4
#define VGA_CRTC_DATA	0x3D5

// VGA Color Palette Registers
#define VGA_DAC_READ_INDEX	0x3C7
#define VGA_DAC_WRITE_INDEX	0x3C8
#define VGA_DAC_DATA	0x3C9

// General Control and Status Registers
#define VGA_INSTAT_READ	0x3DA
#define VGA_DAC_WRITE_INDEX 	0x3C8
#define VGA_DAC_DATA	0x3C9

#define VGA_BYTES_PER_LINE	80
#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define VGA_SIZE	(VGA_BYTES_PER_LINE * VGA_HEIGHT)

enum gvga_color {
	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	GREY,
	DARK_GREY,
	BRIGHT_BLUE,
	BRIGHT_GREEN,
	BRIGHT_CYAN,
	BRIGHT_RED,
	BRIGHT_MAGENTA,
	YELLOW,
	WHITE,
};

uint8_t *g_vga_buffer = (uint8_t*)VGA_ADDRESS;

static const uint8_t seq_text[5] = {
	0x03, 0x00, 0x03, 0x00, 0x02
};

static const uint8_t crtc_text[25] = {
	0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
	0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
	0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
	0xFF
};

static const uint8_t gc_text[9] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF
};

static const uint8_t ac_text[21] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
	0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x0C, 0x00, 0x0F, 0x08, 0x00
};

void graphical_putc(uint16_t x, uint16_t y, char c, uint8_t color){
	if((uint8_t)c == 0x00){
		for(int row = 0; row < FONT_HEIGHT; row++){
			for(int col = 0; col < FONT_WIDTH; col++){
				putpixel(x + col, y + row, 0);
			}
		}
		return;
	}

	const uint8_t* glyph = &fontdata_8x16[(uint8_t)c * FONT_HEIGHT];
	for(int row = 0; row < FONT_HEIGHT; row++){
		uint8_t line = glyph[row];
		for(int col = 0; col < FONT_WIDTH; col++){
			if(line & (1 << (7 - col))){
				putpixel(x + col, y + row, color);
			}
		}
	}
}

/*
 * Despite the effort, this function will get replaced soon
 */
void gvga_scroll(void){
	int plane;
	int row_bytes = 640 / 8;
	int scroll_rows = FONT_HEIGHT;
	int bytes_to_move = row_bytes * (480 - scroll_rows);

	outb(VGA_GC_INDEX, 0x05);
	outb(VGA_GC_DATA, 0x00);

	// Disable Set/Reset
	outb(VGA_GC_INDEX, 0x00);
	outb(VGA_GC_DATA, 0x00);

	outb(VGA_GC_INDEX, 0x01);
	outb(VGA_GC_DATA, 0x00);

	for(plane = 0; plane < 4; plane++) {
		outb(VGA_GC_INDEX, 0x04);  // Read Map Select
		outb(VGA_GC_DATA, plane);

		outb(VGA_SEQ_INDEX, 0x02);  // Map Mask
		outb(VGA_SEQ_DATA, 1 << plane);

		uint8_t *plane_base = g_vga_buffer;
		memmove(plane_base, plane_base + row_bytes * scroll_rows, bytes_to_move);
		memset(plane_base + bytes_to_move, 0, row_bytes * scroll_rows);
	}

	outb(VGA_GC_INDEX, 0x05);  // Graphics Mode register
	outb(VGA_GC_DATA, 0x02);   // Write mode 2, read mode 0

	outb(VGA_SEQ_INDEX, 0x02);
	outb(VGA_SEQ_DATA, 0x0F);

	outb(VGA_GC_INDEX, 0x03);  // Data Rotate/Function Select
	outb(VGA_GC_DATA, 0x00);   // No rotation, replace

	y -= scroll_rows;
}

void set_sequencer_registers(){
	outb(VGA_SEQ_INDEX, 0);
	outb(VGA_SEQ_DATA, 0x03);
	outb(VGA_SEQ_INDEX, 1);
	outb(VGA_SEQ_DATA, 0x01);
	outb(VGA_SEQ_INDEX, 2);
	outb(VGA_SEQ_DATA, 0x0F);  // Enable all planes
	outb(VGA_SEQ_INDEX, 3);
	outb(VGA_SEQ_DATA, 0x00);
	outb(VGA_SEQ_INDEX, 4);
	outb(VGA_SEQ_DATA, 0x06);  // Memory mode: 0x06
}

void set_graphics_controller_registers(){
	outb(VGA_GC_INDEX, 0);
	outb(VGA_GC_DATA, 0x00);
	outb(VGA_GC_INDEX, 1);
	outb(VGA_GC_DATA, 0x00);
	outb(VGA_GC_INDEX, 2);
	outb(VGA_GC_DATA, 0x00);
	outb(VGA_GC_INDEX, 3);
	outb(VGA_GC_DATA, 0x00);
	outb(VGA_GC_INDEX, 4);
	outb(VGA_GC_DATA, 0x00);
	outb(VGA_GC_INDEX, 5);
	outb(VGA_GC_DATA, 0x02);  // Graphics mode: 0x02
	outb(VGA_GC_INDEX, 6);
	outb(VGA_GC_DATA, 0x05);
	outb(VGA_GC_INDEX, 7);
	outb(VGA_GC_DATA, 0x0F);
	outb(VGA_GC_INDEX, 8);
	outb(VGA_GC_DATA, 0xFF);  // Default bitmask
}

void set_attribute_controller_registers(){
	uint8_t ac_data[21] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x41, 0x00, 0x0F, 0x00, 0x00
	};

	for (uint8_t index = 0; index < 20; index++) {
		inb(0x3DA);				// Reset AC flip-flop
		outb(VGA_AC_INDEX, index);		// Set index
		outb(VGA_AC_WRITE, ac_data[index]);	// Write data
	}
	inb(0x3DA);
	outb(VGA_AC_INDEX, 0x20 | 20);
	outb(VGA_AC_WRITE, ac_data[20]);
}

void set_crt_controller_registers(){
	static const uint8_t crtc_data[25] = {
		0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E,
		0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3, 0xFF
	};

	// unlock crtc registers 0-7 (for older VGA cards)
	outb(VGA_CRTC_INDEX, 0x11);
	outb(VGA_CRTC_DATA, crtc_data[0x11] & 0x7F);

	for (uint8_t i = 0; i < 25; i++) {
		outb(VGA_CRTC_INDEX, i);
		outb(VGA_CRTC_DATA, crtc_data[i]);
	}
}

void putpixel(uint16_t x, uint16_t y, uint8_t color){
	if (x >= VGA_WIDTH || y >= VGA_HEIGHT) return;

	uint32_t offset = y * VGA_BYTES_PER_LINE + (x / 8);
	uint8_t bit_mask = 1 << (7 - (x % 8));

	// Set bit mask
	outb(VGA_GC_INDEX, 0x08);
	outb(VGA_GC_DATA, bit_mask);

	volatile uint8_t dummy = g_vga_buffer[offset];
	(void)dummy;

	// Write color (uses write mode 2)
	g_vga_buffer[offset] = color & 0x0F;
}

void gvga_clear(){
	uint32_t i;

	/* write mode 2 */
	outb(VGA_GC_INDEX, 0x05);
	outb(VGA_GC_DATA, 0x02);

	/* enable all planes */
	outb(VGA_SEQ_INDEX, 0x02);
	outb(VGA_SEQ_DATA, 0x0F);

	/* bit mask all bits */
	outb(VGA_GC_INDEX, 0x08);
	outb(VGA_GC_DATA, 0xFF);

	for(i = 0; i < VGA_SIZE; i++){
		uint8_t dummy = g_vga_buffer[i];
		(void)dummy;
		g_vga_buffer[i] = 0x00;
	}
}


void gvga_init(void){
	outb(VGA_MISC_WRITE, 0xE3);  // Mode 12h setting
	set_sequencer_registers();
	set_crt_controller_registers();
	set_graphics_controller_registers();
	set_attribute_controller_registers();
	g_vga_buffer = (uint8_t*)VGA_ADDRESS;
	gvga_clear();
	printk("Graphical vga on 0x%x\n", VGA_ADDRESS);
}

void gvga_retcons(void){
	uint8_t i;

	outb(0x3C2, 0x67);

	outb(0x3C4, 0x00);
	outb(0x3C5, 0x01);

	for (i = 0; i < 5; i++) {
		outb(0x3C4, i);
		outb(0x3C5, seq_text[i]);
	}

	for (i = 0; i < 25; i++) {
		outb(0x3D4, i);
		outb(0x3D5, crtc_text[i]);
	}

	for (i = 0; i < 9; i++) {
		outb(0x3CE, i);
		outb(0x3CF, gc_text[i]);
	}

	for (i = 0; i < 21; i++) {
		inb(0x3DA);
		outb(0x3C0, i);
		outb(0x3C0, ac_text[i]);
	}

	inb(0x3DA);
	outb(0x3C0, 0x20);
}

