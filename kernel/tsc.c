/*
 * tsc.c - accurate timekeeping. This file has all functions related to rdtsc along with some helpers.
 */

#include <types.h>
#include <defs.h>
#include <x86.h>

uint64_t tsc_freq_hz = 0;
uint64_t tsc_offset = 0;
uint64_t tsc_realtime = 0;

uint64_t rdtsc(void){
	uint32_t lo, hi;
	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
}

void tscinit(void){
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
	tsc_realtime = (uint64_t)epoch_mktime() * 1000000000ULL;

	printk("using TSC with PIT clocksource\n");
}

uint64_t tsc_to_us(uint64_t tsc){
	return ((tsc - tsc_offset) * 1000000) / tsc_freq_hz;
}

uint64_t tsc_to_ns(uint64_t tsc){
	return ((tsc - tsc_offset) * 1000000000ULL) / tsc_freq_hz;
}

