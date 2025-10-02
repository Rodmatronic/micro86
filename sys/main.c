#include "../include/types.h"
#include "../include/defs.h"
#include "../include/param.h"
#include "../include/memlayout.h"
#include "../include/mmu.h"
#include "../include/proc.h"
#include "../include/x86.h"
#include "../include/config.h"
#include "../include/time.h"
#include "../include/version.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

struct cpuident {
	char	cpu_model[49];
	char	cpu_vendor[49];
	char 	*machine;
	int	cliplevel;
	int 	mhz;
};

char * copyrightbsd = "Copyright (c) 1989, 1993 The Regents of the University of California.  All rights reserved.\n";

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  cprintf("FreeNIX Release %s Version %s\n", sys_release, sys_version);
  cprintf(copyrightbsd);
  kvmalloc();      // kernel page table
  timeinit();	   // set up unix date&time
  mpinit();        // detect other processors
  //vbeinit();
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  devinit();	   // device nodes init
  uartinit();      // serial port
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
//  vgainit();
//  postvbe=1;
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

void
cpuid(uint eax_in, uint *eax, uint *ebx, uint *ecx, uint *edx) {
    uint eax_val, ebx_val, ecx_val, edx_val;
    asm volatile("cpuid"
                 : "=a"(eax_val), "=b"(ebx_val), "=c"(ecx_val), "=d"(edx_val)
                 : "a"(eax_in)
                 :);
    if (eax) *eax = eax_val;
    if (ebx) *ebx = ebx_val;
    if (ecx) *ecx = ecx_val;
    if (edx) *edx = edx_val;
}

void
identifycpu()
{
	struct cpuident cp;
	cp.machine = MACHINE;

	// Get the CPU model info
	uint reg_values[12];
	cpuid(0x80000000, &reg_values[0], &reg_values[1], &reg_values[2], &reg_values[3]);
	if (reg_values[0] < 0x80000004){
		cprintf("Nomodel ");
	} else {
		cpuid(0x80000002, &reg_values[0], &reg_values[1], &reg_values[2], &reg_values[3]);
		cpuid(0x80000003, &reg_values[4], &reg_values[5], &reg_values[6], &reg_values[7]);
		cpuid(0x80000004, &reg_values[8], &reg_values[9], &reg_values[10], &reg_values[11]);
		memmove(cp.cpu_model, reg_values, sizeof(reg_values));
		cp.cpu_model[48] = '\0';
	}

	// Get the CPU vendor info
	uint eax, ebx, ecx, edx;
	cpuid(0, &eax, &ebx, &ecx, &edx);
	*((uint *)cp.cpu_vendor) = ebx;
	*((uint *)(cp.cpu_vendor + 4)) = edx;
	*((uint *)(cp.cpu_vendor + 8)) = ecx;
	cp.cpu_vendor[12] = '\0';
	cprintf("cpu%d: %s %s\n", cpunum(), cp.cpu_model, cp.cpu_vendor);
}

// Common CPU setup code.
static void
mpmain(void)
{
  identifycpu();
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_sys_entryother_start[], _binary_sys_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_sys_entryother_start, (uint)_binary_sys_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

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

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

