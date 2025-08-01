#include "../include/errno.h"
#include "../include/user.h"
#include "../include/stat.h"
#include "../include/fcntl.h"
#include "../include/font8x16.h"
#include "../include/graphics.h"

int helloworldbutton;

int
main()
{
    initgraphics("Desktop", CYAN);
    int helloworldbutton = putbutton(0, 25, 64, 64, "Hellogui", GREY, WHITE);

    flush_background();

    while (1) {
	mouser();
	if (getbuttonclick(helloworldbutton))
		openprogram("/opt/bin/hellogui");

        usleep(5000);
    }

    return 0;
}
