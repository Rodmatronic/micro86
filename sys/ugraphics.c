#include "../include/user.h"
#include "../include/font8x16.h"
#include "../include/graphics.h"
#include "../include/fcntl.h"

typedef struct {
    int rel_x, rel_y;
    int abs_x, abs_y;
    int width, height;
    int pressed, clicked;
    char* text;
    int fg, bg;
} Button;

#define MAX_ENTRIES 50

struct WindowEntry {
    int pid;
    int x, y, width, height;
};

struct Window win;

static Button buttons[MAX_BUTTONS];
static int button_count = 0;
static int pressed_button_id = -1;

#define cursor_width 11
#define cursor_height 16
static unsigned char cursor_bits[] = {
   0xff, 0x01, 0xff, 0x02, 0x7f, 0x03, 0xbf, 0x03, 0xdf, 0x03, 0xef, 0x03,
   0xf7, 0x03, 0xfb, 0x03, 0xfd, 0x03, 0xe1, 0x03, 0x6f, 0x03, 0xb7, 0x02,
   0xb7, 0x01, 0xdb, 0x07, 0xdb, 0x07, 0xe7, 0x07};

void graphicsexit(int r);

int leftclick, old_leftclick, leftdown;
int rightclick, old_rightclick, rightdown;
int middleclick, old_middleclick, middledown;

static uint8_t *background = NULL;
static uint8_t *saved_bg = NULL;
static uint8_t *window_buffer = NULL;

void update_button_positions();
void redraw_buttons();

int result;
int dx, dy;
int last_dx = -1;
int last_dy = -1;
int mousebuttons;

pack_pixel(int x, int y, int color)
{
	return (x << 22) | (y << 12) | (color & 0xFF);
}

void
putpixel(int x, int y, int color)
{
    if (x < win.x || x >= win.x + win.width || y < win.y || y >= win.y + win.height) {
        return;
    }
    devctl(1, 1, pack_pixel(x, y, color));
}

void
dputpixel(int x, int y, int color)
{
    devctl(1, 1, pack_pixel(x, y, color));
}

void save_background(int x, int y) {
    for (int row = 0; row < cursor_height; row++) {
        for (int col = 0; col < cursor_width; col++) {
                saved_bg[row * cursor_width + col] =
                    background[(y + row) * VGA_MAX_WIDTH + (x + col)];
        }
    }
}

void restore_background(int x, int y) {
    for (int row = 0; row < cursor_height; row++) {
        for (int col = 0; col < cursor_width; col++) {
                background[(y + row) * VGA_MAX_WIDTH + (x + col)] = saved_bg[row * cursor_width + col];
                putpixel(x + col, y + row, saved_bg[row * cursor_width + col]);
        }
    }
}

static char cursormask_bits[] = {
  0xFF, 0x07, 0xFF, 0x05, 0xFF, 0x04, 0x7F, 0x04, 0x3F, 0x04, 0x1F, 0x04,
  0x0F, 0x04, 0x07, 0x04, 0x03, 0x04, 0x1F, 0x04, 0x9F, 0x04, 0xCF, 0x05,
  0xCF, 0x07, 0xE7, 0x07, 0xE7, 0x07, 0xFF, 0x07
};

void init_window_buffer() {
    if (window_buffer) free(window_buffer);
    window_buffer = malloc(win.width * win.height);
}

void save_to_window_buffer() {
    for (int y = 0; y < win.height; y++) {
        for (int x = 0; x < win.width; x++) {
            int screen_x = win.x + x;
            int screen_y = win.y + y;
            if (screen_x >= 0 && screen_x < VGA_MAX_WIDTH &&
                screen_y >= 0 && screen_y < VGA_MAX_HEIGHT) {
                window_buffer[y * win.width + x] =
                    background[screen_y * VGA_MAX_WIDTH + screen_x];
            }
        }
    }
}

void move_window(int new_x, int new_y) {
    save_to_window_buffer();

    putrectf(win.x, win.y, win.width, win.height, 0x8);
    flush_background();

    win.x = new_x;
    win.y = new_y;

    update_button_positions();
    for (int y = 0; y < win.height; y++) {
        for (int x = 0; x < win.width; x++) {
            int screen_x = new_x + x;
            int screen_y = new_y + y;
            if (screen_x >= 0 && screen_x < VGA_MAX_WIDTH &&
                screen_y >= 0 && screen_y < VGA_MAX_HEIGHT) {
                background[screen_y * VGA_MAX_WIDTH + screen_x] =
                    window_buffer[y * win.width + x];
            }
        }
    }
    redraw_buttons();
    flush_background();
}

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

void putpixel_bg(int x, int y, int color) {
    // Clip to window bounds
    if (x < win.x || x >= win.x + win.width || y < win.y || y >= win.y + win.height) {
        return;
    }
    background[y * VGA_MAX_WIDTH + x] = color;
}

struct vga_update {
    int x, y;
    int width, height;
    uint8_t pixels[];
};

void flush_background() {
    int start_x = win.x < 0 ? 0 : win.x;
    int start_y = win.y < 0 ? 0 : win.y;
    int end_x = win.x + win.width;
    if (end_x > VGA_MAX_WIDTH) end_x = VGA_MAX_WIDTH;
    int end_y = win.y + win.height;
    if (end_y > VGA_MAX_HEIGHT) end_y = VGA_MAX_HEIGHT;
    
    int visible_width = end_x - start_x;
    int visible_height = end_y - start_y;
    
    if (visible_width <= 0 || visible_height <= 0) {
        // Entire window is offscreen, nothing to draw
        return;
    }

    size_t buf_size = visible_width * visible_height;
    struct vga_update* update = malloc(sizeof(struct vga_update) + buf_size);

    update->x = start_x;
    update->y = start_y;
    update->width = visible_width;
    update->height = visible_height;

    for (int j = 0; j < visible_height; j++) {
        int src_y = start_y + j;
        uint8_t *src = background + src_y * VGA_MAX_WIDTH + start_x;
        uint8_t *dst = update->pixels + j * visible_width;
        memmove(dst, src, visible_width);
    }

    devctl(1, 3, update);
    free(update);
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

void dputline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        putpixel_bg(x1, y1, color);
	dputpixel(x1, y1, color);
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

void dputrect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color) {
    dputline(x, y, x, y + height, color);
    dputline(x, y, x + width, y, color);
    dputline(x + width, y, x + width, y + height, color);
    dputline(x, y + height, x + width, y + height, color);
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
    putpixel_bg(xc + x, yc + y, color);
    putpixel_bg(xc - x, yc + y, color);
    putpixel_bg(xc + x, yc - y, color);
    putpixel_bg(xc - x, yc - y, color);
    putpixel_bg(xc + y, yc + x, color);
    putpixel_bg(xc - y, yc + x, color);
    putpixel_bg(xc + y, yc - x, color);
    putpixel_bg(xc - y, yc - x, color);
}

void dbres_circle(int xc, int yc, int x, int y, uint8_t color) {
    putpixel_bg(xc + x, yc + y, color);
    putpixel_bg(xc - x, yc + y, color);
    putpixel_bg(xc + x, yc - y, color);
    putpixel_bg(xc - x, yc - y, color);
    putpixel_bg(xc + y, yc + x, color);
    putpixel_bg(xc - y, yc + x, color);
    putpixel_bg(xc + y, yc - x, color);
    putpixel_bg(xc - y, yc - x, color);
    putpixel(xc + x, yc + y, color);
    putpixel(xc - x, yc + y, color);
    putpixel(xc + x, yc - y, color);
    putpixel(xc - x, yc - y, color);
    putpixel(xc + y, yc + x, color);
    putpixel(xc - y, yc + x, color);
    putpixel(xc + y, yc - x, color);
    putpixel(xc - y, yc - x, color);
}

void dputcircle(uint16_t x, uint16_t y, uint16_t radius, uint8_t color) {
    int x2 = 0, y2 = radius;
    int d = 3 - 2 * radius;
    dbres_circle(x, y, x2, y2, color);
    while (y2 >= x2) {
        x2++;
        if (d > 0) {
            y2--;
            d = d + 4 * (x2 - y2) + 10;
        } else {
            d = d + 4 * x2 + 6;
        }
        dbres_circle(x, y, x2, y2, color);
    }
}

void dputline_thick(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint8_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    for (int i = 0; i < r+1; i++) dputcircle(x1, y1, i, color);
    while (1) {
        for (int i = r+1; i > -r-1; i--) {
            putpixel_bg(x1+i, y1, color);
            putpixel(x1+i, y1, color);
            putpixel_bg(x1, y1+i, color);
            putpixel(x1, y1+i, color);
        }

        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
    for (int i = 0; i <	r+1; i++) dputcircle(x1, y1, i, color);
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
        graphical_putc(win.x+current_x, win.y+y, *str, color);
        current_x += FONT_WIDTH;
        str++;
    }
}

void bputs(int x, int y, const char* str, uint8_t color) {
    int current_x = x;

    while (*str != '\0') {
        graphical_putc(current_x, y, *str, color);
        current_x += FONT_WIDTH;
        str++;
    }
}

int putbutton(int x, int y, int width, int height, char* text, int color, int fg) {
    if (button_count >= MAX_BUTTONS) return -1;

    Button *b = &buttons[button_count];
    b->rel_x = x;
    b->rel_y = y;
    b->abs_x = win.x + x;
    b->abs_y = win.y + y;
    b->width = width;
    b->height = height;
    b->pressed = 0;
    b->clicked = 0;
    b->bg = color;
    b->fg = fg;
    b->text = text;

    putrectf(b->abs_x+1, b->abs_y+1, width-2, height-2, color);
    putline(b->abs_x, b->abs_y, b->abs_x+width, b->abs_y, 0xF);
    putline(b->abs_x, b->abs_y, b->abs_x, b->abs_y+height, 0xF);
    putline(b->abs_x+width, b->abs_y, b->abs_x+width, b->abs_y+height, 0x8);
    putline(b->abs_x, b->abs_y+height, b->abs_x+width, b->abs_y+height, 0x8);
    bputs(b->abs_x + 3, b->abs_y + height/2 - 7, text, fg);

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
            if (x >= b->abs_x && x < b->abs_x + b->width &&
                y >= b->abs_y && y < b->abs_y + b->height) {
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
        if (x >= b->abs_x && x < b->abs_x + b->width &&
            y >= b->abs_y && y < b->abs_y + b->height) {
            b->clicked = 1;
        }
        pressed_button_id = -1;
    }
}

void redraw_buttons() {
    for (int i = 0; i < button_count; i++) {
        Button *b = &buttons[i];
        putrectf(b->abs_x+1, b->abs_y+1, b->width-2, b->height-2, b->bg);

        putline(b->abs_x, b->abs_y, b->abs_x+b->width, b->abs_y, 0xF);
        putline(b->abs_x, b->abs_y, b->abs_x, b->abs_y+b->height, 0xF);
        putline(b->abs_x+b->width, b->abs_y, b->abs_x+b->width, b->abs_y+b->height, 0x8);
        putline(b->abs_x, b->abs_y+b->height, b->abs_x+b->width, b->abs_y+b->height, 0x8);
        bputs(b->abs_x + 3, b->abs_y + b->height/2 - 7, b->text, b->fg);
    }
}

void update_button_positions() {
    for (int i = 0; i < button_count; i++) {
        Button *b = &buttons[i];
        b->abs_x = win.x + b->rel_x;
        b->abs_y = win.y + b->rel_y;
    }
}

int getbuttonclick(int id) {
    if (id < 0 || id >= button_count) return 0;
    int clicked = buttons[id].clicked;
    buttons[id].clicked = 0;
    return clicked;
}

int exitbutton;

static void lock_win_file() {
    mkdir("/run");
    int fd = open("/run/lockfile", O_CREATE | O_RDWR);
    if (fd >= 0) {
        close(fd);
    }
    while (link("/run/lockfile", "/run/win.lock") < 0) {
        sleep(10);
    }
}

static void unlock_win_file() {
    unlink("/run/win.lock");
}

static int rect_intersect(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    return (x1 < x2 + w2 && x1 + w1 > x2 &&
            y1 < y2 + h2 && y1 + h1 > y2);
}

static int parse_int(char **p) {
    int num = 0;
    char *start = *p;
    while (**p >= '0' && **p <= '9') {
        num = num * 10 + (**p - '0');
        (*p)++;
    }
    return (*p > start) ? num : -1;
}

static int read_window_entries(struct WindowEntry *entries, int max) {
    int fd = open("/run/win", O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    char buf[1024];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n < 0) {
        return 0;
    }
    buf[n] = '\0';

    int count = 0;
    char *p = buf;
//    char *line;

    while (count < max && *p) {
        if (*p == '\n') {
            p++;
            continue;
        }

        int pid = parse_int(&p);
        if (pid < 0 || *p != '-') break;
        p++;

        int x = parse_int(&p);
        if (x < 0 || *p != '-') break;
        p++;

        int y = parse_int(&p);
        if (y < 0 || *p != '-') break;
        p++;

        int w = parse_int(&p);
        if (w < 0 || *p != '-') break;
        p++;

        int h = parse_int(&p);
        if (h < 0) break;

        // Skip to end of line or next entry
        while (*p != '\n' && *p != '\0') p++;
        if (*p == '\n') p++;

        entries[count].pid = pid;
        entries[count].x = x;
        entries[count].y = y;
        entries[count].width = w;
        entries[count].height = h;
        count++;
    }

    return count;
}

static void append_window_entry(int pid, int x, int y, int w, int h) {
    int fd = open("/run/win", O_WRONLY | O_CREATE);
    lseek(fd, 0, SEEK_END);
    if (fd < 0) {
        return;
    }
    char buf[100];
    int len = sprintf(buf, "%d-%d-%d-%d-%d\n", pid, x, y, w, h);
    write(fd, buf, len);
    close(fd);
}

static void remove_window_entry(int pid) {
    struct WindowEntry entries[MAX_ENTRIES];
    int count = read_window_entries(entries, MAX_ENTRIES);

    int fd = open("/run/win.tmp", O_WRONLY | O_CREATE);
    if (fd < 0) {
        return;
    }

    // Write all entries except the one with the matching PID
    for (int i = 0; i < count; i++) {
        if (entries[i].pid == pid) continue;
        char buf[100];
        int len = sprintf(buf, "%d-%d-%d-%d-%d\n",
                         entries[i].pid,
                         entries[i].x,
                         entries[i].y,
                         entries[i].width,
                         entries[i].height);
        write(fd, buf, len);
    }
    close(fd);

    // Replace the original file with the temporary one
    unlink("/run/win");
    link("/run/win.tmp", "/run/win");
    unlink("/run/win.tmp");
}

void
initgraphics(int x, int y, int width, int height, char * s, int c)
{
    int pid = getpid();

    lock_win_file();

    struct WindowEntry entries[MAX_ENTRIES];
    int count = read_window_entries(entries, MAX_ENTRIES);

    int new_x = x;
    int new_y = y;
    int found = 0;

    while (!found) {
        found = 1;
        for (int i = 0; i < count; i++) {
            if (rect_intersect(new_x, new_y, width, height, 
                               entries[i].x, entries[i].y, entries[i].width, entries[i].height)) {
                found = 0;
                break;
            }
        }

        if (!found) {
            new_x += 10;
            if (new_x + width > VGA_MAX_WIDTH+width-10) {
                new_x = 0;
                new_y += 10;
                if (new_y + height > VGA_MAX_HEIGHT+height-10) {
                    new_y = 0;
                }
            }
        }
    }

    win.x = new_x;
    win.y = new_y;
    win.width = width;
    win.height = height;
    win.name = s;

    append_window_entry(pid, new_x, new_y, width, height);

    unlock_win_file();

    if (background) free(background);
    background = malloc(VGA_MAX_WIDTH * VGA_MAX_HEIGHT * sizeof(uint8_t));

    if (saved_bg) free(saved_bg);
    saved_bg = malloc(cursor_width * cursor_height);
    init_window_buffer();

    putrectf(win.x+0, win.y+0, win.width, 24, 0x7);
    putline(win.x+0, win.y+0, win.x+win.width, win.y+0, 0xF);
    putline(win.x+0, win.y+0, win.x+0, win.y+24, 0xF);
    putbutton(win.width-22, 3, 18, 18, "", 0x07, BLACK);
    putline(win.x+win.width-24, win.y+3, win.x+win.width-24, win.y+21, 0x8);
    putline(win.x+win.width-25, win.y+3, win.x+win.width-25, win.y+21, 0xF);
    graphical_puts(6, 6, s, 0x00);
    putrectf(win.x+0, win.y+25, win.width, win.height, c);
    putrect(win.x+0, win.y+0, win.width-1, win.height-1, 0xF);
    putrect(win.x+1, win.y+24, win.width-3, win.height-26, 0x08);
    save_to_window_buffer();
}

// 0xFF if no key.
char 
getkey()
{
	return (char)devctl(2, 0, 0);
}

void
checkbar(int x, int y)
{
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
	int pid;
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
//	while((wpid=wait(0)) >= 0 && wpid != pid){}
	flush_background();
	return 0;
}



//!!

// UPDATE ME TO USE UNIVERSAL GRAPHICS COLOUR

//!!
void
graphicsexit(int r)
{
    int pid = getpid();

    lock_win_file();
    remove_window_entry(pid);
    unlock_win_file();

    putrectf(win.x, win.y, win.width, win.height, 0x8);
    flush_background();
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

void
putimage(x, y, width, height, bits, c)
char bits[];
{
  int bytes_per_row = (width + 7) / 8;

  for(int row = 0; row < height; row++) {
    for(int byte_in_row = 0; byte_in_row < bytes_per_row; byte_in_row++) {
      unsigned char byte = bits[row * bytes_per_row + byte_in_row];
      for(int bit = 0; bit < 8; bit++) {
        x = byte_in_row * 8 + bit;
        if(x >= width) {
          break; // Skip padding bits at end of row
        }

        if(!(byte & (1 << bit))) {
          putpixel_bg(x, y, c);
        }
      }
    }
    y++;
    x = 0;
  }
}

void
clearscreen()
{
	devctl(1, 2, 0x8);
}
