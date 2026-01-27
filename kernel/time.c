/*
 * time.c - kernel time. Handles epoch and whatnot. Portions of this file are copied from Linux 0.01.
 * Though, they have been modified to fix bugs related to Y2k.
 *
 * cmos_read was originally a macro but was changed to a proper function to shut up GCC.
 */

#include <types.h>
#include <time.h>
#include <defs.h>
#include <x86.h>

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

int tsc_calibrated = 1;

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

uint8_t cmos_read(uint8_t addr){
	outb(0x70, 0x80 | addr);
	return inb(0x71);
}

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

time_t epoch_mktime(void) {
	return tsc_realtime / 1000000000ULL;;
}

time_t mktime(struct tm * tm)
{
	time_t res;
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
		time.tm_sec = cmos_read(0);
		time.tm_min = cmos_read(2);
		time.tm_hour = cmos_read(4);
		time.tm_mday = cmos_read(7);
		time.tm_mon = cmos_read(8);
		time.tm_year = cmos_read(9);
		century = cmos_read(century_register);
	} while (time.tm_sec != cmos_read(0));
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
	tsc_realtime = mktime(&time) * 1000000000ULL;
}
