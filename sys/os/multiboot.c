#include "../include/multiboot.h"
#include "../include/types.h"
#include "../include/defs.h"
#include "../include/memlayout.h"
#include "../include/mmu.h"

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

void set_phystop(void) {
	struct multiboot_tag *tag;
	struct multiboot_tag_mmap *mmap;
	uint32_t max_top = 0;

	tag = (struct multiboot_tag *)modget(MULTIBOOT_TAG_TYPE_MMAP);
	if (!tag)
		return;

	mmap = (struct multiboot_tag_mmap *)tag;
	for (unsigned char *p = (unsigned char *)mmap->entries;
		p < (unsigned char *)mmap + mmap->size;
		p += mmap->entry_size) {

		struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
		uint32_t start = e->addr;
		uint32_t len   = e->len;
		uint32_t end   = start + len;

		/* type 1 means available RAM per Multiboot2 spec */
		if (e->type == 1) {
			if (end > max_top)
				max_top = end;
		}
	}

	if (max_top == 0)
		return;

	/* FIXME: this sucks! */
	if (max_top > 0x7DFE0000)
		max_top = 0x7DFE0000;

	/* round down to page boundary so PHYSTOP is safe to use for page allocs */
	PHYSTOP = (uint)(max_top & ~(PGSIZE - 1));
}

void mbootinit(unsigned long addr) {
	mbi_addr = (uint)P2V(addr);
	if (mbi_addr & 7) {
		panic("mbootinit: invalid boot info\n");
	}
	mbi_size = *(uint *)mbi_addr;
	cprintf("mbootinit: mboot info addr=0x%08x, size=%d bytes\n", mbi_addr, mbi_size);

	set_phystop();
	cprintf("mbootinit: PHYSTOP set to 0x%08x\n", PHYSTOP);

}
