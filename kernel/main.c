/*
 * Kernel C entrypoint
 */

#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <config.h>
#include <time.h>
#include <version.h>
#include <traps.h>

extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file
char * banner = sys_version "\n";
extern void tty_init(void);

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int kmain(uint32_t addr){
	// The first thing we do is set up serial for debugging.
	uart_init();	// serial port for debugging

	// Enable the early TTY console so that we can see
	// important kernel messages
	tty_init();
	printk(banner);
	multiboot_init(addr);	// Verify Multiboot2 header & set up physical memory
	kinit1(end, P2V(4*1024*1024)); // phys page allocator
	kvm_alloc();	// Kernel page table

	// time_init and tsc_init both set up time
	time_init();	// Set up unix date&time
	tsc_init();	// TSC/PIT granular time
	pit_init();	// PIT timer
	pic_init();	// Enable the PIC

	pic_enable(IRQ_COM1);	// Enable keyboard interrupts for COM1
	segment_init();	// Segment descriptors
	console_init();	// Console hardware
	process_init();	// Process table
	trap_init();	// Trap vectors

	keyboard_init();	// PS/2 Keyboard

	// Below are for setting up the filesystem/IDE disk
	buffer_init();	// Buffer cache
	file_init();	// File table
	ide_init();	// IDE bootdisk 
	kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // Must come after startothers()

	// At this point, we are ready to start loading the first user process (initcode)
	user_init();	// First user process
	idt_init();	// Load idt register
	scheduler();	// Start running processes

	while (1){ // If we somehow end up here, spin indefinitely as to not crash and burn
		asm("hlt");
	}

	return -1; /* NOT REACHED */
}

pde_t entrypgdir[];	// For entry.S

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
	// Map VA's [0, 4MB) to PA's [0, 4MB)
	[0] = (0) | PTE_P | PTE_W | PTE_PS,
	// Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
	[KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

