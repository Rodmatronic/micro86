#include <errno.h>
#include <stdio.h>
#include <stat.h>
#include <fcntl.h>

void
draw_rect(x1, y1, x2, y2, c)
{
    int fd = open("/dev/scr", O_WRONLY);
    char buf[10];
    buf[0] = 0x03;
    buf[1] = x1 & 0xFF; buf[2] = (x1 >> 8) & 0xFF;
    buf[3] = y1 & 0xFF; buf[4] = (y1 >> 8) & 0xFF;
    buf[5] = x2 & 0xFF; buf[6] = (x2 >> 8) & 0xFF;
    buf[7] = y2 & 0xFF; buf[8] = (y2 >> 8) & 0xFF;
    buf[9] = c;
    write(fd, buf, sizeof(buf));
    close(fd);
}

void 
draw_line(x1, y1, x2, y2, c)
{
    int fd0 = open("/dev/scr", O_WRONLY);
    char buf[10];
    buf[0] = 0x01;
    buf[1] = x1 & 0xFF; buf[2] = (x1 >> 8) & 0xFF;
    buf[3] = y1 & 0xFF; buf[4] = (y1 >> 8) & 0xFF;
    buf[5] = x2 & 0xFF; buf[6] = (x2 >> 8) & 0xFF;
    buf[7] = y2 & 0xFF; buf[8] = (y2 >> 8) & 0xFF;
    buf[9] = c;
    write(fd0, buf, sizeof(buf));
    close(fd0);
}

void
draw_circle(x1, y1, r, c)
{
    int fd0 = open("/dev/scr", O_WRONLY);
    char buf[10];
    buf[0] = 0x01;
    buf[1] = x1 & 0xFF; buf[2] = (x1 >> 8) & 0xFF;
    buf[3] = y1 & 0xFF; buf[4] = (y1 >> 8) & 0xFF;
    buf[5] = r & 0xFF; buf[6] = (r >> 8) & 0xFF;
    buf[7] = 0; buf[8] = 0;
    buf[9] = c;
    write(fd0, buf, sizeof(buf));
    close(fd0);
}

void
clear_screen(uint8_t c)
{
    int fd0 = open("/dev/scr", O_WRONLY);
    char pixel1[10];
    pixel1[0] = 5;
    pixel1[1] = 0;
    pixel1[2] = 0;
    pixel1[3] = 0;
    pixel1[4] = 0;
    pixel1[5] = 0;
    pixel1[6] = 0;
    pixel1[7] = 0;
    pixel1[8] = 0;
    pixel1[9] = 0;

    write(fd0, pixel1, sizeof(pixel1));
    close(fd0);
}

int
main()
{
  clear_screen(0x03);
  draw_rect(0, 460, 640, 480, 0x7);
  draw_line(0, 461, 640, 461, 0xF);
  //draw_rect(90, 100, 180, 200, 0x9);
  for(int i = 0; i < 16; i++){
	draw_line(0, i, 100, i, i);
  }
  return 0;
}
