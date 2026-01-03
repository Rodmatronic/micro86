/*
 * main.c - kernel entrypoint. This is jumped to after entry.S and is where the magic happens. Bootstrap
 * the system and eventually setup userspace with INITCODE.
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

void cmosphys(void);
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file
char * banner = sys_version "\n";

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
// Doing int function returns so GCC will not complain
int kmain(unsigned int addr){
	gvga_init();
	printk(banner);
	mbootinit(addr); // multiboot
	kinit1(end, P2V(4*1024*1024)); // phys page allocator
	kvmalloc();	// kernel page table
	timeinit();	// set up unix date&time
	tscinit();	// TSC/PIT granular time
	mpinit();	// detect other processors
	lapicinit();	// interrupt controller
	seginit();	// segment descriptors
	ioapicinit();	// another interrupt controller
	consoleinit();	// console hardware
	devinit();	// device nodes init
	uartinit();	// serial port
	pinit();	// process table
	tvinit();	// trap vectors
	binit();	// buffer cache
	fileinit();	// file table
	ideinit();	// disk 
	kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
	userinit();	// first user process
	idtinit();	// load idt register
	scheduler();	// start running processes
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

uint64_t
__udivdi3(uint64_t dividend, uint64_t divisor)
{
	uint64_t quotient = 0;
	uint64_t remainder = 0;

	for(int i = 63; i >= 0; i--) {
		remainder <<= 1;
		remainder |= (dividend >> i) & 1;
		if(remainder >= divisor) {
			remainder -= divisor;
			quotient |= (1ULL << i);
		}
	}
	return quotient;
}

uint64_t __udivmoddi4(uint64_t dividend, uint64_t divisor, uint64_t *remainder_ptr){
	uint64_t quotient = 0;
	uint64_t remainder = 0;

	if(divisor == 0) {
		if(remainder_ptr)
			*remainder_ptr = 0;
		return 0;
	}

	// Fast path for small divisors
	if(divisor > dividend) {
		if(remainder_ptr)
			*remainder_ptr = dividend;
		return 0;
	}

	if(divisor == dividend) {
		if(remainder_ptr)
			*remainder_ptr = 0;
		return 1;
	}

	for(int i = 63; i >= 0; i--) {
		remainder <<= 1;
		remainder |= (dividend >> i) & 1;
		if(remainder >= divisor) {
			remainder -= divisor;
			quotient |= (1ULL << i);
		}
	}

	if(remainder_ptr)
		*remainder_ptr = remainder;

	return quotient;
}

