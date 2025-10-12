#include <errno.h>
#include <stdio.h>
#include <stat.h>
#include <fcntl.h>
#include <font8x16.h>
#include <graphics.h>


int
main()
{
    clearscreen(); // desktop background
    initgraphics(0, 0, 300, 200, "Desktop", CYAN);
    int helloworldbutton = putbutton(0, 25, 64, 64, "Hellogui", GREY, BLACK);
    int paint = putbutton(64, 25, 64, 64, "Paint", GREY, BLACK);
    flush_background();

    while (1) {
	mouser();
	if (getbuttonclick(helloworldbutton))
		openprogram("/opt/bin/hellogui");
	if (getbuttonclick(paint))
		openprogram("/opt/bin/paint2");
        usleep(5000);
    }

    return 0;
}
