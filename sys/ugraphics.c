#include "../include/user.h"
#include "../include/font8x16.h"
#include "../include/graphics.h"

typedef struct {
    int x, y;
    int width, height;
    int pressed;
    int clicked;
} Button;

static Button buttons[MAX_BUTTONS];
static int button_count = 0;
static int pressed_button_id = -1;

#define cursor_width 11
#define cursor_height 16
static char cursor_bits[] = {
  0xFF, 0x01, 0xFF, 0x02, 0x7F, 0x03, 0xBF, 0x03, 0xDF, 0x03, 0xEF, 0x03,
  0xF7, 0x03, 0xFB, 0x03, 0xFD, 0x03, 0xE0, 0x03, 0x6F, 0x03, 0xB7, 0x02,
  0xB7, 0x01, 0xDB, 0x03, 0xDB, 0x07, 0xE3, 0x07,
};

int leftclick, old_leftclick, leftdown;
int rightclick, old_rightclick, rightdown;
int middleclick, old_middleclick, middledown;

// Background buffer (640x480 pixels)
static uint8_t background[VGA_MAX_HEIGHT][VGA_MAX_WIDTH];

// Saved cursor background
static uint8_t saved_bg[cursor_height][cursor_width];

int result;
int dx, dy;
int last_dx = -1;
int last_dy = -1;
int mousebuttons;

pack_pixel(int x, int y, int color)
{
    return (x << 22) | (y << 12) | (color & 0xFF);
}

putpixel(x, y, color)
{
  devctl(1, 1, pack_pixel(x, y, color));
}

void save_background(int x, int y) {
    for (int row = 0; row < cursor_height + 2; row++) {
        for (int col = 0; col < cursor_width + 2; col++) {
            if (y + row < VGA_MAX_HEIGHT && x + col < VGA_MAX_WIDTH) {
                saved_bg[row][col] = background[y + row][x + col];
            }
        }
    }
}

// Restore background under cursor position
void restore_background(int x, int y) {
    for (int row = 0; row < cursor_height; row++) {
        for (int col = 0; col < cursor_width; col++) {
            if (y + row < VGA_MAX_HEIGHT && x + col < VGA_MAX_WIDTH) {
                putpixel(x + col, y + row, background[y + row][x + col]);
            }
        }
    }
}

static char cursormask_bits[] = {
  0xFF, 0x07, 0xFF, 0x05, 0xFF, 0x04, 0x7F, 0x04, 0x3F, 0x04, 0x1F, 0x04,
  0x0F, 0x04, 0x07, 0x04, 0x03, 0x04, 0x1F, 0x04, 0x9F, 0x04, 0xCF, 0x05,
  0xCF, 0x07, 0xE7, 0x07, 0xE7, 0x07, 0xFF, 0x07
};

void draw_cursor(int x, int y) {
    save_background(x, y);

    for (int row = 0; row < cursor_height; row++) {
        uint16_t mask_bits = ((uint16_t *)cursormask_bits)[row];
        uint16_t cursor_bits_val = ((uint16_t *)cursor_bits)[row];

        for (int col = 0; col < cursor_width; col++) {
            int shift = cursor_width - 1 - col;
            int mask_bit = (mask_bits >> shift) & 1;
            int cursor_bit = (cursor_bits_val >> shift) & 1;

            if (y + row < VGA_MAX_HEIGHT && x + col < VGA_MAX_WIDTH) {
                if (!mask_bit) {
                    putpixel(x + col, y + row, 0x00);
                }

                if (!cursor_bit) {
                    putpixel(x + col, y + row, 0x0F);
                }
            }
        }
    }
}

void render_background() {
    for (int y = 0; y < VGA_MAX_HEIGHT; y++) {
        for (int x = 0; x < VGA_MAX_WIDTH; x++) {
            if(!background[y][x] == 0x00){
		    putpixel(x, y, background[y][x]);
	    }
        }
    }
}

void
putpixel_bg(int x, int y, int color)
{
    if (x < 0 || x >= VGA_MAX_WIDTH || y < 0 || y >= VGA_MAX_HEIGHT)
        return;

    background[y][x] = color;
//    putpixel(x, y, color);
}

void flush_background() {
    devctl(1, 3, background);
}

void putrect_trans(int x1, int y1, int x2, int y2, int color) {
    if (x1 > x2) { int tmp = x1; x1 = x2; x2 = tmp; }
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if ((x + y) % 2 == 0) {
                putpixel_bg(x, y, color);
            }
        }
    }
}

void putline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        putpixel_bg(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
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

void graphical_putc(int x, int y, char c, uint8_t color) {
    const uint8_t* glyph = &fontdata_8x16[(uint8_t)c * FONT_HEIGHT];
    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (line & (1 << (7 - col))) {
                putpixel_bg(x + col, y + row, color);
            }
        }
    }
}

void graphical_puts(int x, int y, const char* str, uint8_t color) {
    int current_x = x;

    while (*str != '\0') {
        graphical_putc(current_x, y, *str, color);
        current_x += FONT_WIDTH;
        str++;
    }
}

int putbutton(int x, int y, int width, int height, const char* text, int color, int fg) {
    if (button_count >= MAX_BUTTONS) return -1;

    Button *b = &buttons[button_count];
    b->x = x;
    b->y = y;
    b->width = width;
    b->height = height;
    b->pressed = 0;
    b->clicked = 0;

    // Draw the button
    putrectf(x+1, y+1, width-2, height-2, color);
    putline(x, y, x+width, y, 0xF);
    putline(x, y, x, y+height, 0xF);
    putline(x+width, y, x+width, y+height, 0x8);
    putline(x, y+height, x+width, y+height, 0x8);
    graphical_puts(x + 3, y + height/2 - 7, text, fg);

    return button_count++;
}

void update_buttons(int x, int y, int leftclick, int old_leftclick) {
    // Clear previous click states
    for (int i = 0; i < button_count; i++) {
        buttons[i].clicked = 0;
    }

    // Handle mouse press
    if (leftclick && !old_leftclick) {
        pressed_button_id = -1;
        for (int i = 0; i < button_count; i++) {
            Button *b = &buttons[i];
            if (x >= b->x && x < b->x + b->width &&
                y >= b->y && y < b->y + b->height) {
                pressed_button_id = i;
                b->pressed = 1;

                // Redraw pressed state
//                putrectf(b->x+1, b->y+1, b->width-2, b->height-2, 0x0F);
//                graphical_puts(b->x + 3, b->y + b->height/2 - 5, "", 0x00);
                break;
            }
        }
    }

    // Handle mouse release
    if (!leftclick && old_leftclick && pressed_button_id != -1) {
        Button *b = &buttons[pressed_button_id];
        b->pressed = 0;

        // Redraw normal state
//        putrectf(b->x+1, b->y+1, b->width-2, b->height-2, 0x07);
//        graphical_puts(b->x + 3, b->y + b->height/2 - 5, "", 0x00);

        // Check if still within button bounds
        if (x >= b->x && x < b->x + b->width &&
            y >= b->y && y < b->y + b->height) {
            b->clicked = 1;
        }
        pressed_button_id = -1;
    }
}

int getbuttonclick(int id) {
    if (id < 0 || id >= button_count) return 0;
    int clicked = buttons[id].clicked;
    buttons[id].clicked = 0;
    return clicked;
}

int exitbutton;

void
initgraphics(char * s, int c)
{
    putrectf(0, 0, VGA_MAX_WIDTH, 24, 0x7);
    putline(0, 0, VGA_MAX_WIDTH, 0, 0xF);
    putline(0, 0, 0, 24, 0xF);
    putbutton(VGA_MAX_WIDTH-80, 1, 79, 22, "Exit", 0x07, BLACK);
    graphical_puts(4, 6, s, 0x00);
    putline(0, 24, VGA_MAX_WIDTH, 24, 0x08);
    putrectf(0, 25, VGA_MAX_WIDTH, VGA_MAX_HEIGHT, c);
}

void
checkbar(x, y)
{
	int button_x = VGA_MAX_WIDTH-80;
	int button_y = 0;
	int button_height = 24;
	int button_width = 80;
        if (getbuttonclick(exitbutton)) {
            graphicsexit(0);
        }
}

void
alert(char * msg)
{
	putrectf(177, 160, 280, 150, 0x7);
	putrect(176, 159, 282, 152, 0xF);
	int x = 184;
	int y = 170;
	for(int i = 0; msg[i]; i++) {
		graphical_putc(x, y, msg[i], 0x00);
		x += 8;
	}
}

openprogram(char * name)
{
	int pid, wpid;
	char * argv[] = {name, 0};
	pid = fork();
	if(pid < 0){
		alert("Fork failed.");
		return 1;
	}
	if(pid == 0){
		exec(name, argv);
		return 1;
	}
	while((wpid=wait(0)) >= 0 && wpid != pid){}
//	render_background();
	flush_background();
	return 0;
}

void
graphicsexit(r)
{
//	devctl(1, 2, 0);
	exit(r);
}

void
mouser()
{
        result = devctl(0, 0, 0);  // Read mouse position
        dx = (result >> 12) & 0xFFF;
        dy = result & 0xFFF;
	mousebuttons = devctl(0, 2, 0);
	leftclick = (mousebuttons >> 8) & 0xF;
	rightclick = (mousebuttons >> 4) & 0xF;
	middleclick = mousebuttons & 0xF;

        if (dx != last_dx || dy != last_dy) {
            if (last_dx != -1 && last_dy != -1) {
                restore_background(last_dx, last_dy);
            }
	    draw_cursor(dx, dy);
            last_dx = dx;
            last_dy = dy;
        }

        update_buttons(dx, dy, leftclick, old_leftclick);
	old_leftclick = leftclick;
	checkbar(dx, dy);
}
