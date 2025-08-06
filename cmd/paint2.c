#include "../include/errno.h"
#include "../include/user.h"
#include "../include/stat.h"
#include "../include/fcntl.h"
#include "../include/font8x16.h"
#include "../include/graphics.h"

int current_color = BLACK;
int current_radius = 3;
int old_dx, old_dy;

int
main()
{
    initgraphics("Paint", WHITE);
    int color_1 = putbutton(25, 25, 25, 25, "1", BLACK, WHITE);
    int color_2 = putbutton(0, 25, 25, 25, "2", BLUE, WHITE);
    int color_3 = putbutton(25, 50, 25, 25, "3", GREEN, WHITE);
    int color_4	= putbutton(0, 50, 25, 25, "4", CYAN, WHITE);
    int color_5	= putbutton(25, 75, 25, 25, "5", RED, WHITE);
    int color_6	= putbutton(0, 75, 25, 25, "6", MAGENTA, WHITE);
    int color_7	= putbutton(25, 100, 25, 25, "7", BROWN, WHITE);
    int color_8	= putbutton(0, 100, 25, 25, "8", GREY, WHITE);
    int color_9	= putbutton(25, 125, 25, 25, "9", DARK_GREY, WHITE);
    int color_10 = putbutton(0, 125, 25, 25, "10", BRIGHT_BLUE, WHITE);
    int color_11 = putbutton(25, 150, 25, 25, "11", BRIGHT_GREEN, BLACK);
    int color_12 = putbutton(0, 150, 25, 25, "12", BRIGHT_CYAN, BLACK);
    int color_13 = putbutton(25, 175, 25, 25, "13", BRIGHT_RED, BLACK);
    int color_14 = putbutton(0, 175, 25, 25, "14", BRIGHT_MAGENTA, BLACK);
    int color_15 = putbutton(25, 200, 25, 25, "15", YELLOW, BLACK);
    int color_16 = putbutton(0, 200, 25, 25, "16", WHITE, BLACK);
    putbutton(0, 250, 50, 250, "", GREY, GREY);
    int minus = putbutton(0, 225, 25, 25, "-", GREY, BLACK);
    int plus = putbutton(25, 225, 25, 25, "+", GREY, BLACK);
    flush_background();

    while (1) {
	mouser();
	if (getbuttonclick(color_1))
		current_color = 0;
        if (getbuttonclick(color_2))
                current_color =	1;
        if (getbuttonclick(color_3))
                current_color =	2;
        if (getbuttonclick(color_4))
                current_color =	3;
        if (getbuttonclick(color_5))
                current_color =	4;
        if (getbuttonclick(color_6))
                current_color =	5;
        if (getbuttonclick(color_7))
                current_color =	6;
        if (getbuttonclick(color_8))
                current_color =	7;
        if (getbuttonclick(color_9))
                current_color =	8;
        if (getbuttonclick(color_10))
                current_color =	9;
        if (getbuttonclick(color_11))
                current_color =	10;
        if (getbuttonclick(color_12))
                current_color =	11;
        if (getbuttonclick(color_13))
                current_color =	12;
        if (getbuttonclick(color_14))
                current_color =	13;
        if (getbuttonclick(color_15))
                current_color =	14;
        if (getbuttonclick(color_16))
                current_color =	15;
	if (getbuttonclick(plus))
		current_radius++;
	if (getbuttonclick(minus))
		current_radius--;
	if (leftclick && dy > 25+current_radius+1 && dx > 50+current_radius+1){
		if (old_dy < 25+current_radius+1) old_dy = 25+current_radius+1;
		if (old_dx < 50+current_radius+1) old_dx = 50+current_radius+1;
		if (current_radius <= 0) {
			dputline(old_dx, old_dy, dx, dy, current_color);
		} else {
			dputline_thick(old_dx, old_dy, dx, dy, current_radius, current_color);
		}
	}
	old_dx = dx;
	old_dy = dy;
        usleep(5000);
    }

    return 0;
}
