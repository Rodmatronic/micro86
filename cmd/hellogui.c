#include <errno.h>
#include <stdio.h>
#include <stat.h>
#include <fcntl.h>
#include <font8x16.h>
#include <graphics.h>

int
main()
{
    initgraphics(0, 0, 200, 150, "Hello World!", WHITE);
    graphical_puts(20, 45, "Hello World!", BLACK);
    flush_background();

    while (1) {
	mouser();
        usleep(5000);
    }

    return 0;
}
