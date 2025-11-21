#include <types.h>
#include <defs.h>
#include <x86.h>
#include <stdarg.h>
#include <tty.h>

int panicked = 0;
struct cons console;

void
panic(char *fmt, ...)
{
        va_list ap;

        cli();
        console.locking = 0;        // Disable console locking during panic
        cprintf("panic[%d]: ", lapicid());

        va_start(ap, fmt);
        vcprintf(fmt, ap);
        va_end(ap);

        panicked=1;             // Freeze other CPUs
        for(;;);
}

