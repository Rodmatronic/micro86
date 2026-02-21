/*
 * Granular timekeeping using the TSC
 */

#include <types.h>
#include <defs.h>
#include <x86.h>

uint64_t tsc_freq_hz = 0;
uint64_t tsc_offset = 0;
uint64_t tsc_realtime = 0;

uint64_t rdtsc(void){
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}

// TODO: TSC granular time is not accurate, and this is why. I should really figure out how to clean up this dumb
// code. It should read from the kernel's clockspeed to calibrate itself.
void tsc_init(void){
	uint64_t tsc_start, tsc_end;

	outb(0x61, (inb(0x61) & ~0x02) | 0x01);
	outb(0x43, 0xB0);
	outb(0x42, 0x9B);	// Low byte
	outb(0x42, 0x2E);	// High byte

	tsc_start = rdtsc();

	// Wait for PIT to count down
	while(!(inb(0x61) & 0x20));
	tsc_end = rdtsc();
	tsc_freq_hz = (tsc_end - tsc_start) * 100;
	tsc_calibrated = 0;

	tsc_offset = rdtsc();
	printk("Using TSC with PIT clocksource\n");
	debug("tsc_freq_hz  : 0x%x\n", tsc_freq_hz);
	debug("tsc_offset   : 0x%x\n", tsc_offset);
	debug("tsc_realtime : 0x%x\n", tsc_realtime);
}

uint64_t tsc_to_us(uint64_t tsc){
	return ((tsc - tsc_offset) * 1000000LL) / tsc_freq_hz;
}

uint64_t tsc_to_ns(uint64_t tsc){
	return ((tsc - tsc_offset) * 1000000000LL) / tsc_freq_hz;
}

