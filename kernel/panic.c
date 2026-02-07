/*
 * panic.c - bring the system down. 'panic' brings the system to a halt on serious error.
 */

#include <types.h>
#include <defs.h>
#include <x86.h>
#include <stdarg.h>
#include <tty.h>

int panicked = 0;
struct cons console;
extern char * banner;

void panic(char *fmt, ...){
	va_list ap;

        cli();
	uart_debug=0;
        console.locking = 0;	// Disable console locking during panic
	printk("Kernel panic: ");
	printf("\033[91m");

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

        panicked=1;	// Freeze other CPUs
        for(;;);
}

