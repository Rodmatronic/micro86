#include "../include/types.h"
#include "../include/x86.h"
#include "../include/defs.h"
#include "../include/traps.h"
#include "../include/spinlock.h"

#define MOUSE_CMD_RESET     0xFF
#define MOUSE_CMD_RESEND     0xFE
#define MOUSE_CMD_SET_DEFAULTS     0xF6
#define MOUSE_CMD_DISABLE_PACKET_STREAMING     0xF5
#define MOUSE_CMD_ENABLE_PACKET_STREAMING     0xF4
#define MOUSE_CMD_SAMPLE_RATE     0xF3
#define MOUSE_CMD_MOUSE_ID     0xF2
#define MOUSE_CMD_REQUEST_SINGLE_PACKET     0xEB
#define MOUSE_CMD_STATUS     0xE9
#define MOUSE_CMD_RESOLUTION     0xE8
#define MOUSE_IRQ 44
#define MOUSE_ACKNOWLEDGE      0xFA

#define PS2_CMD_PORT      0x64    // read/write PS/2 command register
#define MOUSE_DATA_PORT     0x60    // PS/2 data port register see https://wiki.osdev.org/%228042%22_PS/2_Controller

typedef enum {
    FALSE,
    TRUE
} BOOL;

int g_mouse_x_pos = 0, g_mouse_y_pos = 0;
int left_button = 0;
int right_button = 0;
int middle_button = 0;

MOUSE_STATUS g_status;

int mouse_getx() {
    return g_mouse_x_pos;
}

int mouse_gety() {
    return g_mouse_y_pos;
}

void mouse_wait(BOOL type) {
    uint32_t time_out = 100000;
    if (type == FALSE) {
        // suspend until status is 1
        while (time_out--) {
            if ((inb(PS2_CMD_PORT) & 1) == 1) {
                return;
            }
        }
        return;
    } else {
        while (time_out--) {
            if ((inb(PS2_CMD_PORT) & 2) == 0) {
                return;
            }
        }
    }
}

void mouse_write(uint8_t data) {
    // sending write command
    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0xD4);
    mouse_wait(TRUE);
    // finally write data to port
    outb(MOUSE_DATA_PORT, data);
}

uint8_t mouse_read() {
    mouse_wait(FALSE);
    return inb(MOUSE_DATA_PORT);
}


void get_mouse_status(char status_byte, MOUSE_STATUS *status) {
    memset(status, 0, sizeof(MOUSE_STATUS));
    if (status_byte & 0x01)
        left_button = 1;
    else
	left_button = 0;
    if (status_byte & 0x02)
        right_button = 1;
    else
	right_button = 0;
    if (status_byte & 0x04)
        middle_button = 1;
    else
	middle_button = 0;
    if (status_byte & 0x08)
        status->always_1 = 1;
    if (status_byte & 0x10)
        status->x_sign = 1;
    if (status_byte & 0x20)
        status->y_sign = 1;
    if (status_byte & 0x40)
        status->x_overflow = 1;
    if (status_byte & 0x80)
        status->y_overflow = 1;
}

void print_mouse_info() {
    cprintf("X:%x Y:%x ", g_mouse_x_pos, g_mouse_y_pos);
        cprintf("%x ",g_status.left_button);
    cprintf("%x ", g_status.right_button);
    cprintf("%x\n", g_status.middle_button);
}
void mouse_handler() {
    static uint8_t mouse_cycle = 0;
    static char mouse_byte[3];

    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = mouse_read();
            get_mouse_status(mouse_byte[0], &g_status);
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = mouse_read();
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = mouse_read();
            g_mouse_x_pos = g_mouse_x_pos + mouse_byte[1];
            g_mouse_y_pos = g_mouse_y_pos - mouse_byte[2];

            if (g_mouse_x_pos < 0)
                g_mouse_x_pos = 0;
            if (g_mouse_y_pos < 0)
                g_mouse_y_pos = 0;
            if (g_mouse_x_pos > VGA_MAX_WIDTH)
                g_mouse_x_pos = VGA_MAX_WIDTH - 1;
            if (g_mouse_y_pos > VGA_MAX_HEIGHT)
                g_mouse_y_pos = VGA_MAX_HEIGHT - 1;

            mouse_cycle = 0;
            break;
    }
}


/**
 * available rates 10, 20, 40, 60, 80, 100, 200
 */
void set_mouse_rate(uint8_t rate) {
    uint8_t status;

    outb(MOUSE_DATA_PORT, MOUSE_CMD_SAMPLE_RATE);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        cprintf("error: failed to send mouse sample rate command\n");
        return;
    }
    outb(MOUSE_DATA_PORT, rate);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        cprintf("error: failed to send mouse sample rate data\n");
        return;
    }
}

/*void
mouseinit(void) {
    uint8_t status;

    g_mouse_x_pos = 5;
    g_mouse_y_pos = 2;

    cprintf("initializing mouse...\n");

    // enable mouse device
    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0xA8);

    // print mouse id
    outb(MOUSE_DATA_PORT, MOUSE_CMD_MOUSE_ID);
    status = mouse_read();
    cprintf("mouse id: 0x%x\n", status);

    set_mouse_rate(10);

//    outb(MOUSE_DATA_PORT, MOUSE_CMD_RESOLUTION);
//    outb(MOUSE_DATA_PORT, 0);

    // enable the interrupt
    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0x20);
    mouse_wait(FALSE);
    // get and set second bit
    status = (inb(MOUSE_DATA_PORT) | 2);
    // write status to port
    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0x60);
    mouse_wait(TRUE);
    outb(MOUSE_DATA_PORT, status);

    // set mouse to use default settings
    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        cprintf("error: failed to set default mouse settings\n");
        return;
    }

    // enable packet streaming to receive
    mouse_write(MOUSE_CMD_ENABLE_PACKET_STREAMING);
    status = mouse_read();
    if(status != MOUSE_ACKNOWLEDGE) {
        cprintf("error: failed to enable mouse packet streaming\n");
        return;
    }
  mouse_wait(TRUE);
  outb(PS2_CMD_PORT, 0x20);  // Read controller config
  mouse_wait(FALSE);
  status = inb(MOUSE_DATA_PORT) | 0x02; // Enable IRQ12
  mouse_wait(TRUE);
  outb(PS2_CMD_PORT, 0x60);  // Write controller config
  mouse_wait(TRUE);
  outb(MOUSE_DATA_PORT, status);

  ioapicenable(IRQ_MOUSE, 0);  // For multi-core
}*/

void mouseinit(void) {
    g_mouse_x_pos = 5;
    g_mouse_y_pos = 2;

    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0xA8);  // Enable mouse device

    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0x20);  // Read controller config
    mouse_wait(FALSE);
    uint8_t status = inb(MOUSE_DATA_PORT);
    status |= 0x02;            // Enable IRQ12 (mouse)
    status |= 0x01;            // Enable IRQ1 (keyboard) - CRITICAL!
    status &= ~0x20;           // Disable mouse clock disable

    mouse_wait(TRUE);
    outb(PS2_CMD_PORT, 0x60);  // Write controller config
    mouse_wait(TRUE);
    outb(MOUSE_DATA_PORT, status);

    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    mouse_read();  // ACK

    mouse_write(MOUSE_CMD_ENABLE_PACKET_STREAMING);
    mouse_read();  // ACK

    ioapicenable(IRQ_MOUSE, 0);
}
