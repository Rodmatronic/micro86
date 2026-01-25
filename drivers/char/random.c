/*
 * Simple entropy
 */

#include <types.h>
#include <defs.h>
#include <param.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>

uint32_t rng_state;

// Devnodes devices
uint32_t xorshift32(uint32_t *state){
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

// very simple entropy
int rndread(struct inode *ip, char *dst, int n, uint32_t off){
	uint32_t lo, hi;

	// Generate entropy using rdtsc and mix into PRNG state
	asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
	uint32_t entropy = lo ^ hi;
	rng_state ^= entropy; // Mix entropy into existing state

	uint32_t *buf32 = (uint32_t*)dst;
	int num_words = n / sizeof(uint32_t);
	int remainder = n % sizeof(uint32_t);

	for (int i = 0; i < num_words; i++) {
		buf32[i] = xorshift32(&rng_state);
	}

	if (remainder > 0) {
		uint32_t val = xorshift32(&rng_state);
		char *tail = (char*)(buf32 + num_words);
		for (int i = 0; i < remainder; i++) {
			tail[i] = (val >> (i * 8)) & 0xFF;
		}
	}

	return n;
}

