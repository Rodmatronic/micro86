#include <types.h>
#include <time.h>
#include <defs.h>
#include <x86.h>

// unix time-related shenanigans are ported from Linux 0.01
#define CMOS_READ(addr) ({ \
outb(0x70, 0x80|addr); \
inb(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

int tsc_calibrated = 1;
unsigned long startup_time = 0;
unsigned long kernel_time = 0;

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

/* interestingly, we assume leap-years */
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

void set_kernel_time(unsigned long new_epoch) {
    acquire(&tickslock);
    uint ticks_since_boot = ticks;
    kernel_time = new_epoch - (ticks_since_boot / 100);
    release(&tickslock);
}

uint epoch_mktime(void) {
  uint ticks_since_boot;
  acquire(&tickslock);
  ticks_since_boot = ticks;
  release(&tickslock);
  return kernel_time + (ticks_since_boot / 100);
}

unsigned long mktime(struct tm * tm)
{
	long res;
	int year;

	year = tm->tm_year;
/* magic offsets (y+1) needed to get leapyears right.*/
	res = YEAR*year + DAY*((year+1)/4);
	res += month[tm->tm_mon];
/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
	if (tm->tm_mon>1 && ((year+2)%4))
		res -= DAY;
	res += DAY*(tm->tm_mday-1);
	res += HOUR*tm->tm_hour;
	res += MINUTE*tm->tm_min;
	res += tm->tm_sec;
	return res;
}

void
timeinit(void)
{
	int full_year;
	struct tm time;
	unsigned char century_register = 0x32;
	unsigned char century;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
		century = CMOS_READ(century_register);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	time.tm_mon -= 1;
	BCD_TO_BIN(time.tm_year);
	BCD_TO_BIN(century);

	// century exists to prevent Yk2. linux stored the year as '25',
	// so i make it calculate the current century, then tack that onto
	// the current year.
	full_year = (century * 100) + time.tm_year;
	time.tm_year = full_year - 1970;
	startup_time = mktime(&time);
	kernel_time = mktime(&time);
}
