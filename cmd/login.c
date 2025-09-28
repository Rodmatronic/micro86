/*
 * login [ name ]
 */

#include "../include/types.h"
#include "../include/stat.h"
#include "../include/stdio.h"
#include "../include/fs.h"
#include "../include/fcntl.h"
#include "../include/syslog.h"

struct {
	char	name[128];
	char	tty;
	char	ifill;
	int	time[2];
	int	ufill;
} utmp;

struct stat statb;

struct ttyb ttyb;

char	*ttyx;

#define	ECHO	010

main(argc, argv)
char **argv;
{
	char pbuf[128];
	char salt[3];
//	char loginbuf[128];
	register char *namep, *np;
	char pwbuf[9];
	int t, f, c, uid, gid;

	setprogname(argv[0]);

//	signal(3, 1);
//	signal(2, 1);
//	nice(0);
	ttyx = "/dev/ttyx";
	if (getuid() != 0) {
		write(1, "Sorry.\n", 7);
		exit(1);
	}
//	ttyx[8] = utmp.tty;
//	gtty(0, &ttyb);
//	ttyb.erase = '#';
//	ttyb.kill = '@';
//	stty(ECHO);
	loop:

        ttyb.tflags = ECHO;
        stty(&ttyb);

	memset(utmp.name, 0, sizeof(utmp.name));
	namep = utmp.name;

	if (argc>1) {
		np = argv[1];
		while (namep<utmp.name+8 && *np)
			*namep++ = *np++;
		argc = 0;
	} else {
		write(1, "login: ", 7);
		while ((c = getchar()) != '\n') {
			if (c <= 0)
				exit(0);
			if (namep < utmp.name+8)
				*namep++ = c;
		}
	}

	*namep = '\0';
	/* check for blank login */
	if (utmp.name[0] == '\0')
		goto loop;
	if (getpwentry(utmp.name, pbuf))
		goto bad;
	np = colon(pbuf);
	if (*np!=':') {
		ttyb.tflags &= ~ECHO;
		stty(&ttyb);
		write(1, "Password: ", 10);
		namep = pwbuf;
		while ((c=getchar()) != '\n') {
			if (c <= 0)
				exit(0);
			if (namep<pwbuf+8)
				*namep++ = c;
		}
		*namep++ = '\0';
		ttyb.tflags = ECHO;
		stty(&ttyb);
		write(1, "\n", 1);
		salt[0] = np[0];
		salt[1] = np[1];
		salt[2] = '\0';
		namep = crypt(pwbuf, salt);
		while (*namep++ == *np++);
		if (*--namep!='\0' || *--np!=':')
			goto bad;
	}
	np = colon(np);
	uid = 0;
	while (*np != ':')
		uid = uid*10 + *np++ - '0';
	np++;
	gid = 0;
	while (*np != ':')
		gid = gid*10 + *np++ - '0';
	np++;
	np = colon(np);
	namep = np;
	np = colon(np);
	if (chdir(namep)<0) {
		write(1, "No directory\n", 13);
		goto loop;
	}
	setenv("PWD", namep, 0);
	setenv("HOME", namep, 0);
	time(utmp.time);
	if ((f = open("/etc/utmp", 1)) >= 0) {
		t = utmp.tty;
		if (t>='a')
			t =- 'a' - (10+'0');
		lseek(f, (t-'0')*16, 0);
		write(f, &utmp, 16);
		close(f);
	}
	if ((f = open("/usr/adm/wtmp", 1)) >= 0) {
		lseek(f, 0, 2);
		write(f, &utmp, 16);
		close(f);
	}
	if (!uid){
		char * tty = "console";
		syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s",
				utmp.name, tty);
	}
	if ((f = open("/etc/motd", 0)) >= 0) {
		while(read(f, &t, 1) > 0)
			write(1, &t, 1);
		close(f);
	}
	if(stat(".mail", &statb) >= 0 && statb.size)
		write(1, "You have mail.\n", 15);
	//chown(ttyx, uid);
	setgid(gid);
	setuid(uid);
	if (*np == '\0')
		np = "/bin/sh";
	execl(np, 0);
	write(1, "No shell.\n", 9);
	exit(1);
bad:
	write(1, "Login incorrect.\n", 17);
	execl("/sbin/login", "login", 0);
	goto loop; // old fallback
}

int
getpwentry(name, buf)
char *name;
char *buf;
{
    int fi;
    int r = 1;
    if((fi = open("/etc/passwd", 0)) < 0)
        return r;

    int c;
    register char *gnp, *rnp;

    while(1) {
        rnp = buf;
        // Read a line from passwd file
        while((c = getc(fi)) != '\n' && c != '\0' && c != -1) {
            *rnp++ = c;
        }
        if(c <= 0) break;

        *rnp = '\0';

        gnp = name;
        rnp = buf;
        int match = 1;

        // Compare until colon or end of either string
        while(*gnp != '\0' && *rnp != ':' && *rnp != '\0') {
            if(*gnp++ != *rnp++) {
                match = 0;
                break;
            }
        }

        if(match && *gnp == '\0' && *rnp == ':') {
            r = 0;
            break;
        }
    }

    close(fi);
    return r;
}

colon(p)
char *p;
{
	register char *rp;

	rp = p;
	while (*rp != ':') {
		if (*rp++ == '\0') {
			write(1, "Bad /etc/passwd\n", 16);
			exit(1);
		}
	}
	*rp++ = '\0';
	return(rp);
}
