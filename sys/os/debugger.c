#include <types.h>
#include <defs.h>
char command[512];

int kgets(char *buf, int maxlen) {
    int i = 0;
    int c;

    while (i < maxlen - 1) {
        c = kgetchar();
        if (c == '\r' || c == '\n') break;
        if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                cprintf("%c", 0x100);
            }
            continue;
        }
        buf[i++] = c;
        cprintf("%c", c);
    }
    buf[i] = 0;
    cprintf("\n");
    return i;
}

int getcmd(char *prompt){
    cprintf("Unknown command\n");
    return 0;
}

void debugger(int status) {
    cprintf("welcome to the kernel debugger.\n");
    int kerndcl = 0;
    while(1){
        cprintf("> ");
        kgets(command, sizeof(command));
        if (getcmd(command))
		cprintf("test\n");
    }
    return 0;
}
