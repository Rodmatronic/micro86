/*
 * login [ name ]
 */

#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <fs.h>
#include <fcntl.h>
#include <syslog.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#define SCPYN(a, b)	strncpy(a, b, sizeof(a))

struct {
	char	ut_name[128];
	char	tty;
	int 	ut_line;
	char	ifill;
	int	ut_time[2];
	int	ufill;
} utmp;

char	maildir[30] =	"/usr/spool/mail/";
struct	passwd nouser = {"", "nope"};
struct 	ttyb ttyb;
char	*ttyprompt = NULL;
char	minusnam[16] = "-";
char	homedir[64] = "HOME=";
char	*envinit[] = {homedir, "PATH=:/bin:/usr/bin", 0};
struct	passwd *pwd;
static char loginmsg[] = "login: ";
static char passwdmsg[] = "Password:";
static char incorrectmsg[] = "Login incorrect\n";
struct	passwd *getpwnam();

main(argc, argv)
char **argv;
{
	register char *namep;
	int t, c;
	char *ttyn;
	setprogname(argv[0]);
	if (getuid() != 0) {
		fprintf(stderr, "%s: %s\n", getprogname(), strerror(EPERM));
		exit(1);
	}
	alarm(60);
//	signal(SIGQUIT, SIG_IGN);
//	signal(SIGINT, SIG_IGN);
	nice(-100);
	nice(20);
	nice(0);
//	gtty(0, &ttyb);
	ttyb.erase = '#';
	ttyb.kill = '@';
	for (t=3; t<20; t++)
		close(t);
	ttyn = ttyname(0);
	if (ttyn==0)
		ttyn = "/dev/tty??";
    loop:
        ttyb.tflags = ECHO;
        stty(&ttyb);
	SCPYN(utmp.ut_name, "");
	if (argc>1) {
		SCPYN(utmp.ut_name, argv[1]);
		argc = 0;
	}
	while (utmp.ut_name[0] == '\0') {
		namep = utmp.ut_name;
		ttyprompt = getenv("TTYPROMPT");
		if ((ttyprompt == NULL) || (*ttyprompt == '\0'))
			fputs(loginmsg, stdout);
		else
			fputs(ttyprompt, stdout);
		while ((c = getchar()) != '\n') {
			if(c == ' ')
				c = '_';
			if (c == EOF)
				exit(0);
			if (namep < utmp.ut_name+8)
				*namep++ = c;
		}
	}
	setpwent();
	if ((pwd = getpwnam(utmp.ut_name)) == NULL)
		pwd = &nouser;
	endpwent();
	if (*pwd->pw_passwd != '\0') {
		ttyb.tflags &= ~ECHO;
		stty(&ttyb);
		namep = crypt(getpass(passwdmsg),pwd->pw_passwd);
		if (strcmp(namep, pwd->pw_passwd)) {
			ttyb.tflags = ECHO;
			stty(&ttyb);
			goto bad;
		}
		ttyb.tflags = ECHO;
		stty(&ttyb);
	}
	setenv("PWD", pwd->pw_dir, 1);
	setenv("HOME", pwd->pw_dir, 1);
	if( chdir(pwd->pw_dir) < 0 )
	{
		printf("Unable to change directory to \"%s\"\n",pwd->pw_dir);
		goto loop;
	}
	time(&utmp.ut_time);
/*	t = ttyslot();
	if (t>0 && (f = open("/etc/utmp", 1)) >= 0) {
		lseek(f, (long)(t*sizeof(utmp)), 0);
		SCPYN(utmp.ut_line, index(ttyn+1, '/')+1);
		write(f, (char *)&utmp, sizeof(utmp));
		close(f);
	}
	if (t>0 && (f = open("/usr/adm/wtmp", 1)) >= 0) {
		lseek(f, 0L, 2);
		write(f, (char *)&utmp, sizeof(utmp));
		close(f);
	}
	chown(ttyn, pwd->pw_uid, pwd->pw_gid);*/
	setgid(pwd->pw_gid);
	setuid(pwd->pw_uid);
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = "/bin/sh";
	setenv("PATH", ":/bin:/usr/bin:/sbin:/usr/games", 1);
	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	if ((namep = rindex(pwd->pw_shell, '/')) == NULL)
		namep = pwd->pw_shell;
	else
		namep++;
	strcat(minusnam, namep);
	alarm(0);
	umask(02);
	showmotd();
	strcat(maildir, pwd->pw_name);
	if(access(maildir,4)==0) {
		struct stat statb;
		stat(maildir, &statb);
		if (statb.st_size)
			printf("You have mail.\n");
	}
//	signal(SIGQUIT, SIG_DFL);
//	signal(SIGINT, SIG_DFL);
	execl(pwd->pw_shell, minusnam, 0);
	printf("No shell\n");
	exit(0);
bad:
	printf(incorrectmsg);
	execl("/sbin/login", "login", 0);
	goto loop; // old fallback
}

int	stopmotd;
catch()
{
//	signal(SIGINT, SIG_IGN);
	stopmotd++;
}

showmotd()
{
	FILE *mf;
	register c;

//	signal(SIGINT, catch);
	if((mf = open("/etc/motd",O_RDONLY)) != NULL) {
		while((c = getc(mf)) != EOF && stopmotd == 0)
			putchar(c);
		fclose(mf);
	}
//	signal(SIGINT, SIG_IGN);
}
