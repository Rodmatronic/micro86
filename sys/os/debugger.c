#include <types.h>
#include <defs.h>
#include <x86.h>

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

int getcmd(char *cmd) {
    if (strncmp(cmd, "help", sizeof(cmd)) == 0) {
        cprintf("commands: help, procs, reboot, skip, halt\n");
    } else if (strncmp(cmd, "procs", sizeof(cmd)) == 0) {
        procdump();
    } else if (strncmp(cmd, "reboot", sizeof(cmd)) == 0) {
        outb(0x64, 0xFE);
    } else if (strncmp(cmd, "halt", sizeof(cmd)) == 0) {
        for (;;);
    } else if (strncmp(cmd, "skip", sizeof(cmd)) == 0) {
	cprintf("skipping this panic.\n");
	exit(1);
    } else if (strncmp(cmd, "", sizeof(cmd)) == 0) {
    } else {
        cprintf("Unknown command: %s\n", cmd);
    }
    return 0;
}

int debugger(int status) {
    cprintf("press any key to enter the kernel debugger...\n");
    kgetchar();
    kerndcl = 0;
    while(1){
        cprintf("> ");
        kgets(command, sizeof(command));
        getcmd(command);
    }
    return 0;
}
