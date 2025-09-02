#ifndef GRAPHICS_H
#define GRAPHICS_H

extern int leftclick, old_leftclick, leftdown;
extern int rightclick, old_rightclick, rightdown;
extern int middleclick, old_middleclick, middledown;

#define VGA_MAX_WIDTH 640
#define VGA_MAX_HEIGHT 480
#define MAX_BUTTONS 256

struct Window {
	int x, y, width, height;
	char * name;
};

enum color {
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

extern int result;
extern int dx, dy;
extern int last_dx;
extern int last_dy;
extern int mousebuttons;

int pack_pixel(int x, int y, int color);
void putpixel(int x, int y, int color);
void save_background(int x, int y);
void restore_background(int x, int y);
void draw_cursor(int x, int y);
void putpixel_bg(int x, int y, int color);
void putrect_trans(int x1, int y1, int x2, int y2, int color);
void putline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);
void dputline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);
void putrect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void dputrect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void putrectf(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void bres_circle(int xc, int yc, int x, int y, uint8_t color);
void dputcircle(uint16_t x, uint16_t y, uint16_t radius, uint8_t color);
void dputline_thick(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint8_t color);
void graphical_putc(int x, int y, char c, uint8_t color);
void graphical_puts(int x, int y, const char* str, uint8_t color);
int putbutton(int x, int y, int width, int height, char* text, int color, int fg);
void update_buttons(int x, int y, int leftclick, int old_leftclick);
int getbuttonclick(int id);
void initgraphics(int x, int y, int width, int height, char * s, int c);
void checkbar(int x, int y);
void alert(char * msg);
int openprogram(char * name);
void flush_background();
void mouser();
void putimage(int x, int y, int width, int height, char bits[], int c);
void clearscreen();
void move_window(int new_x, int new_y);
char getkey();
#endif
