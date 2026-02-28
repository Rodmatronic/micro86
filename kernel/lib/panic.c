/*
 * Bring the system down in case of a fatal error
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
	tty_sgr(&ttys[active_tty], 0x0C00);

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

        panicked=1;	// Freeze other CPUs
        for(;;);
}

