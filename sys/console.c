// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "../include/types.h"
#include "../include/defs.h"
#include "../include/param.h"
#include "../include/traps.h"
#include "../include/spinlock.h"
#include "../include/sleeplock.h"
#include "../include/fs.h"
#include "../include/file.h"
#include "../include/memlayout.h"
#include "../include/mmu.h"
#include "../include/proc.h"
#include "../include/x86.h"
#include "../include/stdarg.h"
#include "../include/tty.h"
#include "../include/font8x16.h"

static void consputc(int);
int color = 0x6;
static int panicked = 0;
int draw_blacks = 0;

struct ttyb ttyb = {
    .speeds = 0,       // Initial speeds
    .erase = '\b',     // Backspace
    .kill = '\025',    // Ctrl+U
    .tflags = ECHO     // Enable echo by default
};

int posx;
int posy;

struct cons cons;

static void
printint(uint xx, int base, int sgn, int width, int zero_pad)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn) {
    if ((int)xx < 0) {
      neg = 1;
      x = (uint)(-(int)xx);
    } else {
      x = xx;
    }
  } else {
    x = xx;
  }

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  int total_digits = i;
  int total_chars = total_digits + (neg ? 1 : 0);
  int pad = width > total_chars ? width - total_chars : 0;

  if (zero_pad) {
    if (neg) consputc('-');
    for (int j = 0; j < pad; j++) consputc('0');
    while (--i >= 0) consputc(buf[i]);
  }
  else {
    for (int j = 0; j < pad; j++) consputc(' ');
    if (neg) consputc('-');
    while (--i >= 0) consputc(buf[i]);
  }
}

static void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c, i, state;
  int width, zero_pad;

  state = 0;
  for(i = 0; fmt[i]; i++) {
    c = fmt[i] & 0xff;
    if(state == 0) {
      if(c == '%') {
        state = '%';
        width = 0;
        zero_pad = 0;
      } else {
        consputc(c);
      }
    } else if(state == '%') {
      if(c == '0') {
        zero_pad = 1;
        i++;
        c = fmt[i] & 0xff;
      }

      while(c >= '0' && c <= '9') {
        width = width * 10 + (c - '0');
        i++;
        c = fmt[i] & 0xff;
      }
      if(c == 'l') {
        i++;
        c = fmt[i] & 0xff;
      }
      if(c == 'd') {
        printint((uint)va_arg(ap, int), 10, 1, width, zero_pad);
      } else if(c == 'u') {  // NEW: Handle %u
        printint(va_arg(ap, uint), 10, 0, width, zero_pad);
      } else if(c == 'x' || c == 'p') {
        printint(va_arg(ap, uint), 16, 0, width, zero_pad);
      } else if(c == 's') {
        s = va_arg(ap, char*);
        if(s == 0) s = "(null)";
        while(*s != 0) {
          consputc(*s);
          s++;
        }
      } else if(c == 'c') {
        consputc(va_arg(ap, int));
      } else if(c == '%') {
        consputc(c);
      } else {
        consputc('%');
        consputc(c);
      }
      state = 0;
    }
  }
}

void
vcprintf(const char *fmt, va_list ap)
{
  vprintf(1, fmt, ap);
}

void
cprintf(char *fmt, ...)
{
  va_list ap;
  int locking;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  char * kernmsg = "[kern]: ";
  for (int i = 0; i < 8; i++){
	  consputc(kernmsg[i]);
  }

  va_start(ap, fmt);
  vcprintf(fmt, ap);
  va_end(ap);

  if(locking)
    release(&cons.lock);
}

void
panic(char *fmt, ...)
{
  va_list ap;

  cli();
/*  int x1 = 0;
  int y1 = 0;
  int x2 = VGA_MAX_WIDTH;
  int y2 = VGA_MAX_HEIGHT;
    if (x1 > x2) { int tmp = x1; x1 = x2; x2 = tmp; }
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if ((x + y) % 2 == 0) {
                putpixel(x, y, 0xF);
            }
        }
    }*/

  cons.locking = 0;   // Disable console locking during panic
  cprintf("panic: ");

  va_start(ap, fmt);
  vcprintf(fmt, ap);
  va_end(ap);

  consputc('\n');
  cprintf("Please report this panic to https://github.com/Rodmatronic/Exnix/issues");

  panicked = 1;       // Freeze other CPUs

  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
//static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

#define CONSOLE_COLS 80
//#define CONSOLE_ROWS 60 8x16
#define CONSOLE_ROWS 30
static char console_buffer[CONSOLE_COLS * CONSOLE_ROWS];
static char old_console_buffer[CONSOLE_COLS * CONSOLE_ROWS];
static int buffer_index = 0;

// Initialize buffer with spaces
void console_buffer_init(void) {
    for (int i = 0; i < CONSOLE_COLS * CONSOLE_ROWS; i++) {
        console_buffer[i] = ' ';
    }
}

// Scroll buffer and redraw screen
void vga_scroll(void) {
	memcpy(old_console_buffer, console_buffer, CONSOLE_ROWS * CONSOLE_COLS);
	// Shift buffer up by one row
    for (int i = 0; i < CONSOLE_COLS * (CONSOLE_ROWS - 1); i++) {
        console_buffer[i] = console_buffer[i + CONSOLE_COLS];
    }

    // Clear last row
    for (int i = CONSOLE_COLS * (CONSOLE_ROWS - 1); i < CONSOLE_COLS * CONSOLE_ROWS; i++) {
        console_buffer[i] = ' ';
    }

/*    for (int y = 0; y < CONSOLE_ROWS + 1; y++) {
        for (int x = 0; x < CONSOLE_COLS; x++) {
            char c = old_console_buffer[y * CONSOLE_COLS + x];
	    if (c != ' ') {
            	graphical_putc(x * 8, y * 16, c, bg_color);
	    }
        }
    }
*/
    buffer_index -= CONSOLE_COLS;
    if (buffer_index < 0) buffer_index = 0;

    // Redraw entire buffer
    for (int y = 0; y < CONSOLE_ROWS; y++) {
        for (int x = 0; x < CONSOLE_COLS; x++) {
            char c = console_buffer[y * CONSOLE_COLS + x];
	    char old_c = old_console_buffer[y * CONSOLE_COLS + x];
	    if (old_c != ' ') {
	            graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, old_c, bg_color);
	    }
	    if (c != ' ') {
		    graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, c, color);
	    }
        }
    }
}

//static int cursor_visible = 1;
static int cursor_position = 0;

// Function to draw/erase cursor
void drawcursor(int x, int y, int visible) {
    if (visible) {
        // Draw solid white block (0xDB) for cursor
        const uint8_t* glyph = &fontdata_8x16[0xDB * FONT_HEIGHT];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (line & (1 << (7 - col))) {
                    putpixel(x + col, y + row, color); // White
                }
            }
        }
    } else {
        // Erase cursor by drawing underlying character
        char c = console_buffer[cursor_position];
        if (c != ' ' && c != 0) {
            graphical_putc(x, y, c, 0x0F); // Redraw character
        } else {
            // Clear area if space or null
            for (int row = 0; row < FONT_HEIGHT; row++) {
                for (int col = 0; col < FONT_WIDTH; col++) {
                    putpixel(x + col, y + row, bg_color); // Black
                }
            }
        }
    }
}

// Modified cgaputc
static void cgaputc(int c) {
    int cursor_x = (cursor_position % CONSOLE_COLS) * FONT_WIDTH;
    int cursor_y = (cursor_position / CONSOLE_COLS) * FONT_HEIGHT;
    drawcursor(cursor_x, cursor_y, 0);

    if (c == '\n') {
        // Handle newline: move to next line start
        int current_row = buffer_index / CONSOLE_COLS;
        buffer_index = (current_row + 1) * CONSOLE_COLS;
    } else if (c == '\r') {
        // Handle carriage return: move to line start
        buffer_index = (buffer_index / CONSOLE_COLS) * CONSOLE_COLS;
    } else if (c == BACKSPACE) {
        if (buffer_index > 0) {
            buffer_index--;
            console_buffer[buffer_index] = ' '; // Clear in buffer
            
            // Erase entire character cell
            int x_pos = (buffer_index % CONSOLE_COLS) * FONT_WIDTH;
            int y_pos = (buffer_index / CONSOLE_COLS) * FONT_HEIGHT;
            for (int row = 0; row < FONT_HEIGHT; row++) {
                for (int col = 0; col < FONT_WIDTH; col++) {
                    putpixel(x_pos + col, y_pos + row, 0x00);
                }
            }
        }
    } else {
        // Store printable character
        console_buffer[buffer_index] = c;
        buffer_index++;
    }

    // Wrap buffer index
    if (buffer_index >= CONSOLE_COLS * CONSOLE_ROWS) {
        vga_scroll();
    }

    // Simple screen update (only handles printable chars)
    if (c > 0 && c != '\n' && c != '\r' && c != BACKSPACE) {
        int x = (buffer_index - 1) % CONSOLE_COLS;
        int y = (buffer_index - 1) / CONSOLE_COLS;
        graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, c, color);
    }
    // Update cursor position and draw
    cursor_position = buffer_index;
    cursor_x = (cursor_position % CONSOLE_COLS) * FONT_WIDTH;
    cursor_y = (cursor_position / CONSOLE_COLS) * FONT_HEIGHT;
    drawcursor(cursor_x, cursor_y, 1);
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
	if(ttyb.tflags & ECHO)
          consputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
	if(ttyb.tflags & ECHO)
          consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;
  ttyb.tflags = ECHO;

  ioapicenable(IRQ_KBD, 0);
}

