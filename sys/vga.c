#define VGA_GRAPHICS_WIDTH 640
#define VGA_GRAPHICS_HEIGHT 480
#define VGA_GRAPHICS_BYTES_PER_LINE (VGA_GRAPHICS_WIDTH / 8)
#define VGA_GRAPHICS_SIZE (VGA_GRAPHICS_BYTES_PER_LINE * VGA_GRAPHICS_HEIGHT)

#include "../include/types.h"
#include "../include/x86.h"
#include "../include/defs.h"
#include "../include/memlayout.h"
#include "../include/font8x16.h"
#include "../include/version.h"

//#define VGA_ADDRESS 0xA0000
#define VGA_MAX ( (VGA_MAX_WIDTH / 8) * VGA_MAX_HEIGHT )

enum vga_color {
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

int bg_color = 0x00;
uint8_t *g_vga_buffer = (uint8_t*)VGA_ADDRESS;

static const uint8_t vga_palette[16][3] = {
    {0x00, 0x00, 0x00}, // 0: BLACK
    {0x05, 0x05, 0x05}, // 1: BLUE
    {0x0B, 0x0B, 0x0B}, // 2: GREEN
    {0x11, 0x11, 0x11}, // 3: CYAN
    {0x16, 0x16, 0x16}, // 4: RED
    {0x1a, 0x1a, 0x1a}, // 5: MAGENTA
    {0x1e, 0x1e, 0x1e}, // 6: BROWN 2A1500 (og) #FFB000
    {0x22, 0x22, 0x22}, // 7: GREY
    {0x27, 0x27, 0x27}, // 8: DARK GREY
    {0x2a, 0x2a, 0x2a}, // 9: BRIGHT BLUE
    {0x2d, 0x2d, 0x2d}, // 10: BRIGHT GREEN
    {0x31, 0x31, 0x31}, // 11: BRIGHT CYAN
    {0x34, 0x34, 0x34}, // 12: BRIGHT RED
    {0x39, 0x39, 0x39}, // 13: BRIGHT MAGENTA
    {0x3c, 0x3c, 0x3c}, // 14: YELLOW
    {0x3F, 0x3F, 0x3F}, // 15: WHITE
};

/*enum corrected {
	BROWN,
	BLUE,

};*/

typedef enum vga_color VGA_COLOR_TYPE;

#define VGA_TEXT_ADDRESS       0xB8000
#define VGA_TEXT_TOTAL_ITEMS   2200

#define VGA_TEXT_WIDTH    80
#define VGA_TEXT_HEIGHT   24

// Miscellaneous Output
#define VGA_MISC_READ  0x3CC
#define VGA_MISC_WRITE 0x3C2

// Graphics Controller Registers
#define VGA_GC_INDEX  0x3CE
#define VGA_GC_DATA   0x3CF

// Attribute Controller Registers
#define VGA_AC_INDEX  0x3C0
#define VGA_AC_READ   0x3C1
#define VGA_AC_WRITE  0x3C0

// CRT Controller Registers
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5

// VGA Color Palette Registers
#define VGA_DAC_READ_INDEX   0x3C7
#define VGA_DAC_WRITE_INDEX  0x3C8
#define VGA_DAC_DATA   0x3C9

// General Control and Status Registers
#define VGA_INSTAT_READ   0x3DA

#define VGA_DAC_WRITE_INDEX  0x3C8
#define VGA_DAC_DATA         0x3C9

void vga_load_palette() {
    outb(VGA_DAC_WRITE_INDEX, 0);  // Start at DAC entry 0
    for (int i = 0; i < 16; i++) {
        outb(VGA_DAC_DATA, vga_palette[i][0]); // Red
        outb(VGA_DAC_DATA, vga_palette[i][1]); // Green
        outb(VGA_DAC_DATA, vga_palette[i][2]); // Blue
    }
}

void set_miscellaneous_registers() {
    outb(VGA_MISC_WRITE, 0xE3);  // Mode 12h setting
}

void set_sequencer_registers() {
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

void set_graphics_controller_registers() {
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

void
set_attribute_controller_registers()
{
    uint8_t ac_data[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };

    for (uint8_t index = 0; index < 20; index++) {
        inb(0x3DA);                // Reset AC flip-flop
        outb(VGA_AC_INDEX, index);  // Set index
        outb(VGA_AC_WRITE, ac_data[index]); // Write data
    }
    inb(0x3DA);
    outb(VGA_AC_INDEX, 0x20 | 20);
    outb(VGA_AC_WRITE, ac_data[20]);
}

void set_crt_controller_registers() {
    static const uint8_t crtc_data[25] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E,
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3, 0xFF
    };

    for (uint8_t i = 0; i < 25; i++) {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, crtc_data[i]);
    }
}
	
void graphical_putc(uint16_t x, uint16_t y, char c, uint8_t color) {
	const uint8_t* glyph = &fontdata_8x16[(uint8_t)c * FONT_HEIGHT];
	for (int row = 0; row < FONT_HEIGHT; row++) {
		uint8_t line = glyph[row];
		for (int col = 0; col < FONT_WIDTH; col++) {
			if (line & (1 << (7 - col))) {
	                	putpixel(x + col, y + row, color);
			}
		}
	}
}

void vga_clear_screen(uint8_t);

void
vgainit(void)
{
    set_miscellaneous_registers();
    set_sequencer_registers();
    set_crt_controller_registers();
    set_graphics_controller_registers();
    set_attribute_controller_registers();
    vga_load_palette();

    g_vga_buffer = (uint8_t*)VGA_ADDRESS;
    vga_clear_screen(0x00);
    cprintf("%s %s\n", sys_name, sys_version);
    cprintf("vgainit: %c%c Graphical vga on 0x%x\n", 0x01, 0x02, VGA_ADDRESS);
}

void putrect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color) {
    putline(x, y, x, y + height, color);
    putline(x, y, x + width, y, color);
    putline(x + width, y, x + width, y + height, color);
    putline(x, y + height, x + width, y + height, color);
}

void putrectf(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color) {
    putline(x, y, x, y + height, color);
    putline(x, y, x + width, y, color);
    putline(x + width, y, x + width, y + height, color);
    putline(x, y + height, x + width, y + height, color);
    for (int i = 0; i < height; i++) {
        putline(x, y + i, x + width, y + i, color);
    }
}

void bres_circle(int xc, int yc, int x, int y, uint8_t color) {
    putpixel(xc + x, yc + y, color);
    putpixel(xc - x, yc + y, color);
    putpixel(xc + x, yc - y, color);
    putpixel(xc - x, yc - y, color);
    putpixel(xc + y, yc + x, color);
    putpixel(xc - y, yc + x, color);
    putpixel(xc + y, yc - x, color);
    putpixel(xc - y, yc - x, color);
}

void putcircle(uint16_t x, uint16_t y, uint16_t radius, uint8_t color) {
    int x2 = 0, y2 = radius;
    int d = 3 - 2 * radius;
    bres_circle(x, y, x2, y2, color);
    while (y2 >= x2) {
        x2++;
        if (d > 0) {
            y2--;
            d = d + 4 * (x2 - y2) + 10;
        } else {
            d = d + 4 * x2 + 6;
        }
        bres_circle(x, y, x2, y2, color);
    }
}

void putline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    if (y1 == y2) {
        for (uint16_t i = x1; i <= x2; i++)
            putpixel(i, y1, color);
        return;
    }

    if (x1 == x2) {
        for (uint16_t i = y1; i <= y2; i++) {
            putpixel(x1, i, color);
        }
        return;
    }
}
	
void
vga_clear_screen(uint8_t colour)
{
    int x, y;
    for(y = 0; y < 480; y++) {
	    for(x = 0; x < 640; x++)
	    {
		   putpixel(x, y, colour);
	    }
    }
}

void
putpixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (x >= VGA_GRAPHICS_WIDTH || y >= VGA_GRAPHICS_HEIGHT) return;
    
    uint32_t offset = y * VGA_GRAPHICS_BYTES_PER_LINE + (x / 8);
    uint8_t bit_mask = 1 << (7 - (x % 8));
    
    // Set bit mask
    outb(VGA_GC_INDEX, 0x08);
    outb(VGA_GC_DATA, bit_mask);
    
    volatile uint8_t dummy = g_vga_buffer[offset];
    (void)dummy;
    
    // Write color (uses write mode 2)
    g_vga_buffer[offset] = color & 0x0F;
}

uint8_t
getpixel(uint16_t x, uint16_t y)
{
    if (x >= VGA_MAX_WIDTH || y >= VGA_MAX_HEIGHT)
        return 0;

    uint32_t offset = y * VGA_GRAPHICS_BYTES_PER_LINE + (x / 8);
    uint8_t bit_mask = 1 << (7 - (x % 8));
    uint8_t color = 0;

    for (int plane = 0; plane < 4; plane++) {
        outb(VGA_GC_INDEX, 0x04);   // Set index to Read Map Select Register
        outb(VGA_GC_DATA, plane);   // Select plane to read
        uint8_t plane_byte = g_vga_buffer[offset];
        if (plane_byte & bit_mask) {
            color |= (1 << plane);
        }
    }
    return color;
}
