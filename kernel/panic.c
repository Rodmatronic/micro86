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

struct stackframe {
	struct stackframe* ebp;
	uint32_t eip;
};

void traceback(unsigned int MaxFrames){
	struct stackframe *stk;
	asm ("movl %%ebp,%0" : "=r"(stk) ::);
	printk("---- stack trace ----\n");

	for(unsigned int frame = 0; stk && frame < MaxFrames; ++frame){
		/* Unwind to previous stack frame */
		printk("[0x%016x]\n", stk->eip);
		stk = stk->ebp;
	}
	printk("-------- end --------");
}

void panic(char *fmt, ...){
        va_list ap;

        cli();
	uart_debug=0;
        console.locking = 0;	// Disable console locking during panic
        printk("Kernel panic: ");
	color_change('9', '1');

        va_start(ap, fmt);
        vkprintf(fmt, ap);
        va_end(ap);

	console_putc('\n');
	printk(banner);
	traceback(5);
        panicked=1;	// Freeze other CPUs
        for(;;);
}

