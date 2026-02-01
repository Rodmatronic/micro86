#include <multiboot.h>
#include <types.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>

unsigned int mbi_addr;
unsigned int mbi_size;
char * cmdline;

void * modget(int type){
	for(struct multiboot_header_tag * tag = (struct multiboot_header_tag *)(mbi_addr + 8); tag->type != MULTIBOOT_TAG_TYPE_END; tag = (struct multiboot_header_tag *)((uchar *)tag + ((tag->size + 7) & ~7))){
		if (tag->type == type){
			return (void *)tag;
		}
	}
	return 0;
}

void set_phystop(void){ // Set the memory amount based on Multiboot2 Memory maps
	struct multiboot_tag *tag;
	struct multiboot_tag_mmap *mmap;
	uint64_t max_top = 0;
	extern char end[];
	uint64_t limit = 0x7DFE0000ULL;

	printk("Using multiboot2 provided memory map\n");
	tag = (struct multiboot_tag *)modget(MULTIBOOT_TAG_TYPE_MMAP);
	if (!tag)
		return;

	mmap = (struct multiboot_tag_mmap *)tag;
	unsigned char *p = (unsigned char *)mmap->entries;
	unsigned char *endp = (unsigned char *)mmap + mmap->size;

	while (p + sizeof(struct multiboot_mmap_entry) <= endp) {
		struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
		uint64_t start = e->addr;
		uint64_t len   = e->len;

		p += mmap->entry_size;

		if (e->type != MULTIBOOT_MEMORY_AVAILABLE)
			continue;

		if (len == 0)
			continue;

		if (start >= 0x100000000ULL)
			continue;

		uint64_t end = start + len;

		if (end < start)
			continue;

		if (end > 0x100000000ULL)
			end = 0x100000000ULL;

		if (end > max_top)
			max_top = end;

		debug("[start=%08x end=%08x len=%08x] usable\n", (uint32_t)start, (uint32_t)end, (uint32_t)len);
	}

	if (max_top == 0)
		return;

	if (max_top > limit){ // FIXME: Memory can only go to 0x7DFE0000ULL, and not as high as it should
		max_top = limit;
		printk("max_top gimping PHYSTOP to %u\n", limit);
	}

	/* round down to page boundary so PHYSTOP is safe to use for page allocs */
	PHYSTOP = (uint32_t)(max_top & ~(PGSIZE - 1));
	uint32_t pa_end = V2P(end);
	uint32_t pa_end_aligned = PGROUNDUP(pa_end);
	uint32_t total_mem = PHYSTOP - pa_end_aligned;

	debug("PHYSTOP pushed to %08x\n", PHYSTOP);
	printk("Found %dM of memory\n", total_mem / 1048576 + 2);
}

void multiboot_init(unsigned long addr){
	struct multiboot_tag_string * multiboot_cmdline;

	mbi_addr = (unsigned int)P2V(addr);
	if (mbi_addr & 7)
		panic("mbootinit: invalid boot info\n");

	mbi_size = *(unsigned int *)mbi_addr;
	printk("Multiboot2 header is valid\n");
	debug("addr : 0x%08x\n", mbi_addr);
	debug("size : %d bytes\n", mbi_size);

	multiboot_cmdline = (struct multiboot_tag_string *)modget(MULTIBOOT_TAG_TYPE_CMDLINE);
	cmdline = (char*)multiboot_cmdline->string;
	printk("Command line: %s\n", cmdline);
	set_phystop();
}
