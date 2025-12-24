#include <multiboot.h>
#include <types.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>

unsigned int mbi_addr;
unsigned int mbi_size;
char * cmdline;

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
	extern char end[];

	printk("using multiboot2 provided memory map\n");
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

		printk("[start=%08x end=%08x len=%08x %s]\n", start, end, len, e->type ? "usable" : "reserved");
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
	PHYSTOP = (unsigned int)(max_top & ~(PGSIZE - 1));
	unsigned int pa_end = V2P(end);
	unsigned int pa_end_aligned = PGROUNDUP(pa_end);
	unsigned int total_mem = PHYSTOP - pa_end_aligned;
	printk("PHYSTOP pushed to %08x\n", PHYSTOP);
	printk("found %dM of memory\n", total_mem / 1048576 + 2);
}

void mbootinit(unsigned long addr) {
	struct multiboot_tag_string * multiboot_cmdline;

	mbi_addr = (unsigned int)P2V(addr);
	if (mbi_addr & 7) {
		panic("mbootinit: invalid boot info\n");
	}
	mbi_size = *(unsigned int *)mbi_addr;
	printk("mboot info addr=0x%08x, size=%d bytes\n", mbi_addr, mbi_size);
	multiboot_cmdline = (struct multiboot_tag_string *)modget(MULTIBOOT_TAG_TYPE_CMDLINE);
	cmdline = (char*)multiboot_cmdline->string;
	printk("Command line: %s\n", cmdline);
	set_phystop();
}
