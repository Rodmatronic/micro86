#include <types.h>
#include <x86.h>
#include <defs.h>
#include <memlayout.h>
#include <font8x8.h>
#include <version.h>
#include <mmu.h>
#include <proc.h>
#include <memlayout.h>

int postvbe = 0;
void fbputpixel(int x, int y, uint32_t color);

void graphical_putc(uint16_t x, uint16_t y, char c, uint32_t color) {
	const uint8_t* glyph = &fontdata_8x8[(uint8_t)c * FONT_HEIGHT];
	for (int row = 0; row < FONT_HEIGHT; row++) {
		uint8_t line = glyph[row];
		for (int col = 0; col < FONT_WIDTH; col++) {
			if (line & (1 << (7 - col))) {
				extern pde_t *kpgdir;
//				pde_t *oldpgdir = myproc()->pgdir;  // or read current CR3 -> conversion
				lcr3(V2P(kpgdir));
	                	fbputpixel(x + col, y + row, color);
			}
		}
	}
}

void putpixel(int x, int y, uint32_t color) {
    // Only switch if we're not already in kernel context
    extern pde_t *kpgdir;
    if(myproc() && myproc()->pgdir != kpgdir) {
        pde_t *oldpgdir = myproc()->pgdir;
        lcr3(V2P(kpgdir));
        fbputpixel(x, y, color);
        lcr3(V2P(oldpgdir));
    } else {
        fbputpixel(x, y, color);
    }
}

#pragma pack(push, 1)
typedef struct {
    uint16_t attributes;
    uint8_t  winA, winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segA, segB;
    uint32_t realFctPtr;
    uint16_t pitch;              // BytesPerScanLine
    uint16_t Xres, Yres;
    uint8_t  XcharSize, YcharSize;
    uint8_t  planes;
    uint8_t  bpp;                // BitsPerPixel
    uint8_t  banks;
    uint8_t  memory_model;
    uint8_t  bank_size;
    uint8_t  image_pages;
    uint8_t  reserved1;

    uint8_t  red_mask_size;
    uint8_t  red_field_pos;
    uint8_t  green_mask_size;
    uint8_t  green_field_pos;
    uint8_t  blue_mask_size;
    uint8_t  blue_field_pos;
    uint8_t  rsv_mask_size;
    uint8_t  rsv_field_pos;
    uint8_t  dcm_info;

    uint32_t physbase;           // PhysBasePtr
    uint32_t reserved2;
    uint16_t reserved3;
} VbeModeInfo;
#pragma pack(pop)

#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  600
#define BYTES_PER_PIXEL 4
#define PITCH (SCREEN_WIDTH * BYTES_PER_PIXEL)

#define VBE_MODE_INFO_PA   0x00090000u   // physical addr we used in bootasm
#define VBE_MODE_INFO_VA   ((VbeModeInfo*)(KERNBASE + VBE_MODE_INFO_PA))

#define VBE_PHYS_START 0xE0000000
#define VBE_ADDRESS    ((volatile uint8_t*)P2V(VBE_PHYS_START))

#define FB_VA  (KERNBASE + 0x10000000)

extern pde_t *kpgdir;
static volatile uint8_t *fb;
static uint32_t fb_pitch;
static uint16_t fb_w, fb_h;
static uint8_t  fb_bpp;
static uint8_t  red_pos, grn_pos, blu_pos;

void* map_framebuffer(void) {
    volatile VbeModeInfo *mi = VBE_MODE_INFO_VA;

    uint32_t fb_pa   = mi->physbase;      // real physical LFB base
    uint32_t pitch   = mi->pitch;
//    uint32_t width   = mi->Xres;
    uint32_t height  = mi->Yres;
//    uint32_t bpp     = mi->bpp;           // expect 32
    uint32_t fb_size = pitch * height;

    // round up to pages
    uint32_t sz = (fb_size + PGSIZE - 1) & ~(PGSIZE - 1);
    uint perm = PTE_W | PTE_P;
    if (mappages(kpgdir, (char*)FB_VA, sz, fb_pa, perm) < 0)
        return 0;

    lcr3(V2P(kpgdir));
    return (void*)FB_VA;
}

uint32_t rgb(uint8_t red, uint8_t green, uint8_t blue);

//static VbeModeInfo vbe_modeinfo_copy;

void vbeinit(void) {
    volatile VbeModeInfo *mi = VBE_MODE_INFO_VA;

    fb = (volatile uint8_t*) map_framebuffer();
    if (!fb) {
        cprintf("vbeinit: map_framebuffer failed\n");
        return;
    }

    fb_pitch = mi->pitch;
    fb_w     = mi->Xres;
    fb_h     = mi->Yres;
    fb_bpp   = mi->bpp;        // store bits-per-pixel

    red_pos  = mi->red_field_pos;
    grn_pos  = mi->green_field_pos;
    blu_pos  = mi->blue_field_pos;

    cprintf("vbeinit: %ux%u bpp=%u pitch=%u phys=0x%x -> VA=0x%x\n",
        fb_w, fb_h, fb_bpp, fb_pitch, mi->physbase, FB_VA);
	vbe_initdraw();

    postvbe=1;
}

// set rgb values in 32 bit number
uint32_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    uint32_t color = red;
    color <<= 16;
    color |= (green << 8);
    color |= blue;
    return color;
}

void fbputpixel(int x, int y, uint32_t color) {
    if ((unsigned)x >= fb_w || (unsigned)y >= fb_h || !fb) return;

    uint32_t bytes_per_pixel = (fb_bpp + 7) / 8; // e.g., 32 -> 4
    uint32_t offset = y * fb_pitch + x * bytes_per_pixel;

    // prefer aligned 32-bit writes when possible (common case: 32 bpp)
    if (bytes_per_pixel == 4) {
        volatile uint32_t *p = (volatile uint32_t *)(fb + offset);
        *p = color;   // single 32-bit write
    } else if (bytes_per_pixel == 3) {
        // rare - write 3 bytes individually (B,G,R)
        fb[offset]     = color & 0xFF;
        fb[offset + 1] = (color >> 8) & 0xFF;
        fb[offset + 2] = (color >> 16) & 0xFF;
    } else if (bytes_per_pixel == 2) {
        volatile uint16_t *p = (volatile uint16_t *)(fb + offset);
        *p = (uint16_t)color; // simple 16-bit write (may need format handling)
    } else {
        for (uint32_t i = 0; i < bytes_per_pixel; ++i)
            fb[offset + i] = (color >> (8 * i)) & 0xFF;
    }
}
