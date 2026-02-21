#include <types.h>
#include <defs.h>
#include <param.h>
#include <traps.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <fs.h>
#include <file.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <tty.h>
#include <stdarg.h>
#include <signal.h>
#include <config.h>
#include <termios.h>

extern struct cons cons;

/*
 * NOTE:
 * `printf` is for printing raw text to the display while being all fancily-formatted.
 * 'printk' is for text that should have time, colours, callers, and eventually syslog.
 */

/* Format a string and print it on the screen, just like the libc
 * function printf. */
void printf(const char *format, ...){
        char **arg = (char **) &format;
        int c;
        char buf[20];

        arg++;
        while ((c = *format++) != 0){
                if (c != '%')
                        console_putc(c);
                else {
                        char *p, *p2;
                        int pad0 = 0, pad = 0;

                        c = *format++;
                        if (c == '0'){
                                pad0 = 1;
                                c = *format++;
                        }

                        if (c >= '0' && c <= '9'){
                                pad = c - '0';
                                c = *format++;
                        }

                        switch (c){
                                case 'd':
                                case 'u':
                                case 'x':
                                        itoa (buf, c, *((int *) arg++));
                                        p = buf;
                                        goto string;
                                        break;

                                case 's':
                                        p = *arg++;
                                        if (! p)
                                                p = "(null)";

                                string:
                                        for (p2 = p; *p2; p2++);
                                        for (; p2 < p + pad; p2++)
                                                console_putc(pad0 ? '0' : ' ');
                                        while (*p)
                                                console_putc(*p++);
                                        break;

                                default:
                                        console_putc(*((int *) arg++));
                                        break;
                        }
                }
        }
}

/* This is the same as normal printf, but uses va_args. */
void vprintf(const char *format, va_list args){
        int c;
        char buf[20];

        while ((c = *format++) != 0){
                if (c != '%')
                        console_putc(c);
                else {
                        char *p, *p2;
                        int pad0 = 0, pad = 0;

                        c = *format++;
                        if (c == '0'){
                                pad0 = 1;
                                c = *format++;
                        }

                        if (c >= '0' && c <= '9'){
                                pad = c - '0';
                                c = *format++;
                        }

                        switch (c){
                                case 'd':
                                case 'u':
                                case 'x':
                                        itoa(buf, c, va_arg(args, int));
                                        p = buf;
                                        goto string;

                                case 's':
                                        p = va_arg(args, char*);
                                        if (!p)
                                                p = "(null)";

                                string:
                                        for (p2 = p; *p2; p2++);
                                        for (; p2 < p + pad; p2++)
                                                console_putc(pad0 ? '0' : ' ');
                                        while (*p)
                                                console_putc(*p++);
                                        break;

                                default:
                                        console_putc(va_arg(args, int));
                                        break;
                        }
                }
        }
}

/* Used by printk to print to console */
void _printk(const char *func, const char *fmt, ...){
        int locking;
        uint32_t us;
        va_list args;

        locking = cons.locking; // Aquire console lock
        if (locking)
                acquire(&cons.lock);

        /* Colour changes are to differenciate between the time, kernel function, and
         * message. The colours used are copied from what is seen in Linux. */
        tty_sgr(&ttys[active_tty], 0x0200);
        printf("[ ");

        us = tsc_calibrated ? 0 : tsc_to_us(rdtsc());
        printf("%4u.%06u", us / 1000000, us % 1000000);

        printf("] ");
        tty_sgr(&ttys[active_tty], 0x0600);
        printf("%s: ", func);
        tty_sgr(&ttys[active_tty], 0x0F00);

        va_start(args, fmt);
        vprintf(fmt, args); // print the message
        va_end(args);

        tty_sgr(&ttys[active_tty], 0x0700);

        if (locking)
                release(&cons.lock);
}

