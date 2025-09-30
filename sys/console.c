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
#include "../include/font8x8.h"

static void consputc(int);
int color = 0x0;
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
      } else if(c == 'u') {
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

int
vsprintf(char *buf, const char *fmt, va_list ap)
{
    char *start = buf;
    vprintf(1, fmt, ap);
    *buf = '\0';
    return buf - start;
}

int
sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vsprintf(buf, fmt, ap);
    va_end(ap);
    return rc;
}

void
cprintf(char *fmt, ...)
{
  va_list ap;
  int locking;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  char * kernmsg = "[system]: ";
  for (int i = 0; i < 10; i++){
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
  cons.locking = 0;   // Disable console locking during panic
  cprintf("panic: ");

  va_start(ap, fmt);
  vcprintf(fmt, ap);
  va_end(ap);

  consputc('\n');
  panicked = 1;       // Freeze other CPUs

  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
//static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

#define CONSOLE_COLS 128
#define CONSOLE_ROWS 96
static char console_buffer[CONSOLE_COLS * CONSOLE_ROWS];
static char old_console_buffer[CONSOLE_COLS * CONSOLE_ROWS];
static int buffer_index = 0;

void vbe_initdraw(void) {
	for (int y = 0; y < CONSOLE_ROWS; y++) {
		for (int x = 0; x < CONSOLE_COLS; x++) {
			char c = console_buffer[y * CONSOLE_COLS + x];
			if (c != ' ') {
				graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, c, rgb(255, 255, 255));
			}
		}
    	}
}

void vga_scroll(void) {
	memcpy(old_console_buffer, console_buffer, CONSOLE_ROWS * CONSOLE_COLS);
	memmove(console_buffer, console_buffer + CONSOLE_COLS, (CONSOLE_ROWS - 1) * CONSOLE_COLS);

	memset(console_buffer + (CONSOLE_ROWS - 1) * CONSOLE_COLS, ' ', CONSOLE_COLS);
	buffer_index -= CONSOLE_COLS;
	if (buffer_index < 0) buffer_index = 0;

	for (int y = 0; y < CONSOLE_ROWS; y++) {
		for (int x = 0; x < CONSOLE_COLS; x++) {
			char c = console_buffer[y * CONSOLE_COLS + x];
			char old_c = old_console_buffer[y * CONSOLE_COLS + x];
			if (old_c != ' ') {
				graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, old_c, rgb(0, 0, 0));
			}
			if (c != ' ') {
				graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, c, rgb(255, 255, 255));
			}
		}
    	}
}

void console_buffer_init(void) {
    for (int i = 0; i < CONSOLE_COLS * CONSOLE_ROWS; i++) {
        console_buffer[i] = ' ';
    }
}

//static int cursor_visible = 1;
static int cursor_position = 0;

void drawcursor(int x, int y, int visible) {
    if (visible) {
        const uint8_t* glyph = &fontdata_8x8[0xDB * FONT_HEIGHT];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (line & (1 << (7 - col))) {
                    putpixel(x + col, y + row, rgb(255, 255, 255));
                }
            }
        }
    } else {
        char c = console_buffer[cursor_position];
        if (c != ' ' && c != 0) {
            graphical_putc(x, y, c, rgb(255, 255, 255));
        } else {
            for (int row = 0; row < FONT_HEIGHT; row++) {
                for (int col = 0; col < FONT_WIDTH; col++) {
                    putpixel(x + col, y + row, rgb(0, 0, 0));
                }
            }
        }
    }
}

void cgaputc(int c) {
    int cursor_x = (cursor_position % CONSOLE_COLS) * FONT_WIDTH;
    int cursor_y = (cursor_position / CONSOLE_COLS) * FONT_HEIGHT;

    oldcgaputc(c);

    if (postvbe)
    	drawcursor(cursor_x, cursor_y, 0);

    if (c == '\n') {
        int current_row = buffer_index / CONSOLE_COLS;
        buffer_index = (current_row + 1) * CONSOLE_COLS;
    } else if (c == '\r') {
        buffer_index = (buffer_index / CONSOLE_COLS) * CONSOLE_COLS;
    } else if(c == '\t'){
    	for(int i = 0; i < 8; i++){
        	console_buffer[buffer_index++] = ' ';
	        int x = (buffer_index - 1) % CONSOLE_COLS;
	        int y = (buffer_index - 1) / CONSOLE_COLS;
		    if (!postvbe)
			    return;

	        graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, ' ', rgb(255, 255, 255));
	        if(buffer_index >= CONSOLE_COLS * CONSOLE_ROWS)
	            vga_scroll();
	    }
    } else if (c == BACKSPACE) {
        if (buffer_index > 0) {
            buffer_index--;
            console_buffer[buffer_index] = ' '; // Clear in buffer
            
            int x_pos = (buffer_index % CONSOLE_COLS) * FONT_WIDTH;
            int y_pos = (buffer_index / CONSOLE_COLS) * FONT_HEIGHT;
	        if (!postvbe)
		    return;

            for (int row = 0; row < FONT_HEIGHT; row++) {
                for (int col = 0; col < FONT_WIDTH; col++) {
                    putpixel(x_pos + col, y_pos + row, 0x00);
                }
            }
        }
    } else {
        console_buffer[buffer_index] = c;
        buffer_index++;
    }

    if (buffer_index >= CONSOLE_COLS * CONSOLE_ROWS) {
        vga_scroll();
    }

    if (c > 0 && c != '\n' && c != '\r' && c != BACKSPACE && c != '\t') {
        int x = (buffer_index - 1) % CONSOLE_COLS;
       	int y = (buffer_index - 1) / CONSOLE_COLS;
	    if (!postvbe)
		    return;

	    color=rgb(255, 255, 255);
        graphical_putc(x * FONT_WIDTH, y * FONT_HEIGHT, c, color);
    }
    cursor_position = buffer_index;
    cursor_x = (cursor_position % CONSOLE_COLS) * FONT_WIDTH;
    cursor_y = (cursor_position / CONSOLE_COLS) * FONT_HEIGHT;
    if (!postvbe)
	    return;
    drawcursor(cursor_x, cursor_y, 1);
}

enum ansi_state {
  ANSI_NORMAL,
  ANSI_ESCAPE,
  ANSI_BRACKET,
  ANSI_PARAM
};

static enum ansi_state ansi_state = ANSI_NORMAL;
static int ansi_params[4];
static int ansi_param_count = 0;
static int current_colour = 0x0700;
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

void
oldcgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else if(c == '\t') {
    int spaces = 8 - (pos % 8);
    for(int i = 0; i < spaces; i++) {
      crt[pos++] = ' ' | current_colour;
      if((pos/80) >= 25){
        memmove(crt, crt+80, sizeof(crt[0])*24*80);
        pos -= 80;
        memset(crt+pos, 0, sizeof(crt[0])*(25*80 - pos));
      }
    }
  } else
    crt[pos++] = (c&0xff) | current_colour;  // black on white

  if(pos < 0 || pos > 26*80)
    panic("pos under/overflow");

  if((pos/80) >= 25){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*24*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(25*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void handle_ansi_sgr(int param);

void
handle_ansi_sgr_sequence(int params[], int count)
{
  for(int i = 0; i < count; i++) {
    handle_ansi_sgr(params[i]);
  }
}

void
handle_ansi_sgr(int param)
{
  switch(param) {
    case 0:  // reset
      current_colour = 0x0700; // white on black
      break;
    case 1:  // bold
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

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == '\t'){
      for(int i = 0; i < 8; i++)
          uartputc(' ');
  } else if(c == BACKSPACE){
      uartputc('\b'); uartputc(' '); uartputc('\b');
  } else {
      uartputc(c);
  }

  switch(ansi_state) {
    case ANSI_NORMAL:
      if(c == 0x1B) { // ESC
        ansi_state = ANSI_ESCAPE;
        ansi_param_count = 0;
	return;
      } else {
        cgaputc(c);  // noraml
        return;
      }
      break;
      
    case ANSI_ESCAPE:
      if(c == '[') {
        ansi_state = ANSI_BRACKET;
      } else {
        ansi_state = ANSI_NORMAL;
      }
      return;
      
    case ANSI_BRACKET:
      if(c >= '0' && c <= '9') {
        ansi_params[ansi_param_count] = c - '0';
        ansi_state = ANSI_PARAM;
      } else if(c == 'm') {
        handle_ansi_sgr(0);
        ansi_state = ANSI_NORMAL;
      } else {
        ansi_state = ANSI_NORMAL; // invalid
      }
      return;
      
    case ANSI_PARAM:
      if(c >= '0' && c <= '9') {
        ansi_params[ansi_param_count] = ansi_params[ansi_param_count] * 10 + (c - '0');
      } else if(c == ';') {
        ansi_param_count++;
        if(ansi_param_count >= 4) {
          ansi_state = ANSI_NORMAL; // too short
        }
      } else if(c == 'm') {
        ansi_param_count++; // end of seq
        handle_ansi_sgr_sequence(ansi_params, ansi_param_count);
        ansi_state = ANSI_NORMAL;
      } else {
        ansi_state = ANSI_NORMAL; // not valid
      }
      return;
  }

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
case '\t':  // Tab
    if(input.e-input.r < INPUT_BUF - 8){  // ensure space for 8 spaces
        for(int i = 0; i < 8; i++){
            input.buf[input.e++ % INPUT_BUF] = ' ';
            if(ttyb.tflags & ECHO)
                consputc(' ');
        }
        if(input.e == input.r+INPUT_BUF){
            input.w = input.e;
            wakeup(&input.r);
        }
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

