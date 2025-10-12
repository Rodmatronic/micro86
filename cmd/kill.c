/* $NetBSD: kill.c,v 1.33 2022/05/16 10:53:14 kre Exp $ */

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

static void nosig(const char *);
void printsignals(FILE*, int);
static int processnum(const char *, pid_t *);
static void usage(void);

void printsignals(FILE* fd, int unused) {
    const char *signals[] = {
"HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "EMT", "FPE", "KILL", "BUS", "SEGV", "SYS", "PIPE", "ALRM", "TERM", "URG", "STOP", "TSTP", "CONT", "CHLD", "TTIN", "TTOU", "IO", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "INFO", "USR1", "USR2"	
    };
    int i;
    for (i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) {
        write(fd, signals[i], strlen(signals[i]));
        write(fd, " ", 1);
    }
    write(fd, "\n", 1);
}

struct sigmap {
    const char *name;
    int number;
};

static struct sigmap signals[] = {
    { "HUP",     1 },
    { "INT",     2 },
    { "QUIT",    3 },
    { "ILL",     4 },
    { "TRAP",    5 },
    { "ABRT",    6 },
    { "EMT",     7 },
    { "FPE",     8 },
    { "KILL",    9 },
    { "BUS",    10 },
    { "SEGV",   11 },
    { "SYS",    12 },
    { "PIPE",   13 },
    { "ALRM",   14 },
    { "TERM",   15 },
    { "URG",    16 },
    { "STOP",   17 },
    { "TSTP",   18 },
    { "CONT",   19 },
    { "CHLD",   20 },
    { "TTIN",   21 },
    { "TTOU",   22 },
    { "IO",     23 },
    { "XCPU",   24 },
    { "XFSZ",   25 },
    { "VTALRM", 26 },
    { "PROF",   27 },
    { "WINCH",  28 },
    { "INFO",   29 },
    { "USR1",   30 },
    { "USR2",   31 },
};

int signalnumber(const char *sn) {
    int i;

    if (sn[0] >= '0' && sn[0] <= '9') {
        int num = atoi(sn);
        for (i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) {
            if (signals[i].number == num)
                return num;
        }
        return 0;
    }

    for (i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) {
        if (strcmp(signals[i].name, sn) == 0)
            return signals[i].number;
    }
    return 0;
}

intmax_t strtoimax(const char *str, char **endptr, int base) {
    intmax_t result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        result = result * base + (*str - '0');
        str++;
    }
    if (endptr)
        *endptr = (char *)str;
    return sign * result;
}

int signalnext(int numsig) {
    int i;
    int found = 0;

    for (i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) {
        if (found)
            return signals[i].number;
        if (signals[i].number == numsig)
            found = 1;
    }

    return -1;
}

int
main(int argc, char *argv[])
{
	int errors;
	int numsig;
	pid_t pid;
	const char *sn;

	setprogname(argv[0]);
	setlocale(LC_ALL, "");
	if (argc < 2)
		usage();

	numsig = SIGTERM;

	argc--, argv++;

	/*
	 * Process exactly 1 option, if there is one.
	 */
	if (argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'l':
			if (argv[0][2] != '\0')
				sn = argv[0] + 2;
			else {
				argc--; argv++;
				sn = argv[0];
			}
			if (argc > 1)
				usage();
			if (argc == 1) {
				if (isdigit((unsigned char)*sn) == 0)
					usage();
				numsig = signum(sn);
				if (numsig >= 128)
					numsig -= 128;
				if (numsig == 0 || signalnext(numsig) == -1)
					nosig(sn);
//				sn = signalname(numsig);
//				if (sn == NULL)
//					err(EXIT_FAILURE,
//					   "unknown signal number: %d", numsig);
				printf("%s\n", sn);
				exit(0);
			}
			printsignals(stdout, 0);
			exit(0);

		case 's':
			if (argv[0][2] != '\0')
				sn = argv[0] + 2;
			else {
				argc--, argv++;
				if (argc < 1) {
					warn(
					    "option requires an argument -- s");
					usage();
				}
				sn = argv[0];
			}
			if (strcmp(sn, "0") == 0)
				numsig = 0;
			else if ((numsig = signalnumber(sn)) == 0) {
				if (sn != argv[0])
					goto trysignal;
				nosig(sn);
			}
			argc--, argv++;
			break;

		case '-':
			if (argv[0][2] == '\0') {
				/* process this one again later */
				break;
			}
			/* FALL THROUGH */
		case '\0':
			usage();
			break;

		default:
 trysignal:
			sn = *argv + 1;
			if (((numsig = signalnumber(sn)) == 0)) {
				if (isdigit((unsigned char)*sn))
					numsig = signum(sn);
				else
					nosig(sn);
			}

			if (numsig != 0 && signalnext(numsig) == -1)
				nosig(sn);
			argc--, argv++;
			break;
		}
	}

	/* deal with the optional '--' end of options option */
	if (argc > 0 && strcmp(*argv, "--") == 0)
		argc--, argv++;

	if (argc == 0)
		usage();

	for (errors = 0; argc; argc--, argv++) {
			if (processnum(*argv, &pid) != 0) {
				errors = 1;
				continue;
			}

		if (kill(pid, numsig) == -1) {
			warn("%s %s", pid < -1 ? "pgrp" : "pid", *argv);
			errors = 1;
		}
	}

	exit(errors);
	/* NOTREACHED */
}

int
signum(const char *sn)
{
	intmax_t n;
	char *ep;

	n = strtoimax(sn, &ep, 10);

	/* check for correctly parsed number */
	if (*ep || n <= INT_MIN || n >= INT_MAX )
		err(EXIT_FAILURE, "bad signal number: %s", sn);
		/* NOTREACHED */

	return (int)n;
}

static int
processnum(const char *s, pid_t *pid)
{
	intmax_t n;
	char *ep;

	errno = 0;
	n = strtoimax(s, &ep, 10);

	/* check for correctly parsed number */
	if (ep == s || *ep || n == INTMAX_MIN || n == INTMAX_MAX ||
	    (pid_t)n != n || errno != 0) {
		warn("bad process%s id: '%s'", (n < 0 ? " group" : ""), s);
		return -1;
	}

	*pid = (pid_t)n;
	return 0;
}

static void
nosig(const char *name)
{

	warn("unknown signal %s; valid signals:", name);
	printsignals(stderr, 0);
	exit(1);
	/* NOTREACHED */
}

static void
usage(void)
{
	const char *pn = getprogname();

	fprintf(stderr, "usage: %s [-s signal_name] pid ...\n"
			"       %s -l [exit_status]\n"
			"       %s -signal_name pid ...\n"
			"       %s -signal_number pid ...\n",
	    pn, pn, pn, pn);
	exit(1);
	/* NOTREACHED */
}
