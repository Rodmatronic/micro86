#include "../include/multiboot.h"
#include "../include/types.h"
#include "../include/defs.h"
#include "../include/memlayout.h"

uint mbi_addr;
uint mbi_size;

void * modget(int type) {
    for(struct multiboot_header_tag * tag = (struct multiboot_header_tag *)(mbi_addr + 8); tag->type != MULTIBOOT_TAG_TYPE_END; tag = (struct multiboot_header_tag *)((uchar *)tag + ((tag->size + 7) & ~7))) {
        if (tag->type == type) {
            return (void *)tag;
        }
    }
    return 0;
}

void mbootinit(unsigned long addr) {
    mbi_addr = (uint)P2V(addr);
    if (mbi_addr & 7) {
        panic("mbootinit: invalid boot info\n");
    }
    mbi_size = *(uint *)mbi_addr;
    cprintf("mbootinit: mboot info addr=0x%08x, size=%d bytes\n", mbi_addr, mbi_size);
}
