/*
 * simple vi - visual text editor
 */

#include <errno.h>
#include <stdio.h>
#include <stat.h>
#include <fcntl.h>
#include <types.h>

int x=0;
int y=0;
int tempfd;
int sourcefd;
char * filebuf = "\0";
char * sourcefb = "\0";
int status;
int exists;
int aoffset=0;
int newfile=0;
int scrollup=0;

char * writemsg = "\033[107m\033[30mFile modified since write; write or use ! to override.\033[0m";
char * badcommand = "\033[107m\033[30mUnknown command.\033[0m";
char * notimplemented = "\033[107m\033[30mNot implemented.\033[0m";

void viexit(int s);
void echoback();
int copy(char *input, char *dest);

/*
 * sync the ansi cursor with reality
 */

void syncxy() {
	if (x < 1)
		x=1;
	if (x > 80)
		x=80;
	if (y < 1)
		y=1;
	if (y > 24)
		y=24;
	printf("\033[%d;%dH", y, x);
}

void clear() {
	printf("\033[2J");
}

void gotoxy(x, y) {
	printf("\033[%d;%dH", y, x);
}

void enable() {
	struct ttyb t;
	gtty(&t);
	t.tflags = ECHO;
	t.tflags |= RAW;
	stty(&t);
}

void disable() {
	struct ttyb t;
	gtty(&t);
	t.tflags |= RAW;
	t.tflags &= ~ECHO;
	stty(&t);
}

void
tobottom() {
	gotoxy(1, 24);
	for(int i=0;i<79;i++)
		write(stdout, " ", 1);
	gotoxy(1, 24);
	printf("\033[24;0H");
}

int cursor_offset(int line, int col) {
	if (line <= 0 || col <= 0) return -1;
	lseek(tempfd, 0, SEEK_SET);

	int current = 1;
	int offset = 0;
	int disp_col = 1;
	unsigned char ch;
	while (read(tempfd, &ch, 1) == 1) {
		if (current == line && disp_col == col)
			break;

		if (ch == '\n') {
			if (current == line) {
				return offset;
			}
			current++;
			disp_col = 1;
		} else {
			if (ch == '\t') {
				int spaces = 8 - ((disp_col - 1) % 8);
				disp_col += spaces;
			} else {
				disp_col++;
			}
		}
		offset++;
	}
	return offset;
}

int cmp() {
	char buf1[1024], buf2[1024];
	ssize_t n1, n2;
	int same = 0;

	while ((n1 = read(sourcefd, buf1, sizeof(buf1))) > 0 &&
			(n2 = read(tempfd, buf2, sizeof(buf2))) > 0) {
		if (n1 != n2 || memcmp(buf1, buf2, n1) != 0) {
			same = 1;
			break;
		}
	}
	if (same) {
		if (read(sourcefd, buf1, 1) > 0 || read(tempfd, buf2, 1) > 0) {
			same = 1;
		}
	}
	return same;
}

void
insertmode() {
	syncxy();
	unsigned char c;
	while (read(0, &c, 1) == 1) {
		switch (c) {
		case 'k':
		case 0x00: // up
			y--;
			if (y == 0 && aoffset > 0) {
				aoffset--;
				y++;
				echoback();
			}
			break;
		case 'j':
		case 0x01: // down
			y++;
			if (y == 24) {
				aoffset++;
				y--;
				echoback();
			}
			break;
		case 'h':
		case 0x02: // left
			x--;
			break;
		case 'l':
		case 0x03: // right
			x++;
		case '\033': // esc
			enable();
			tobottom();
			disable();
			return;
		case '\b':
			return;
		default: 
			int off = cursor_offset(y+aoffset, x);
			lseek(tempfd, off, SEEK_SET);
			write(tempfd, &c, 1);
			enable();
			gotoxy(x, y);
			printf("%c", c);
			x++;
			disable();
			break;
		}
		syncxy();
	}
}

void
commandmode() {
	char c;
	char buf[128];
	int len=0;
	struct stat st;
	memset(buf, 0, sizeof(buf));

	enable();
	tobottom();
	write(stdout, ":", 1);
	while (read(0, &c, 1) == 1) {
		if (c == '\n' || c == '\r') {
			buf[len] = '\0';  // terminate
			if (strcmp(buf, "q") == 0) {
				if (cmp()) {
					tobottom();
					printf(writemsg);
					disable();
					return;
				}
				viexit(0);
			} else if (strcmp(buf, "w") == 0) {
				copy(filebuf, sourcefb);
				tobottom();
				stat(sourcefb, &st);
				printf("%s: %d bytes written.", basename(sourcefb), st.st_size);
				disable();
				return;
			} else {
				tobottom();
				printf(badcommand);
				disable();
			}
			return;  // exit command mode
		} else if (c == '\033') {
			tobottom();
			disable();
			return;
		} else if (c == '\b') {  // backspace
			if (len > 0) {
				len--;
				gotoxy(len+2, 24);
			}
		} else if (len < (int)sizeof(buf) - 1) {
			buf[len++] = c;
		}
	}
}


void
viexit(int s) {
	printf("\033[25;0H");
	struct ttyb t;
	gtty(&t);
	t.tflags = ECHO;
	stty(&t);
	unlink(filebuf);
	close(tempfd);
	close(sourcefd);
	exit(s);
}

int seekline(int lines) {
	char buf[1024];
	ssize_t bytesread;
	int pos = 0;
	int line_count = 0;
	if (lines <= 0) {
		lseek(tempfd, 0, SEEK_SET);
		return 0;
	}
	lseek(tempfd, 0, SEEK_SET);
	while ((bytesread = read(tempfd, buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < bytesread; i++) {
			pos++;
			if (buf[i] == '\n') {
				line_count++;
				if (line_count == lines) {
					lseek(tempfd, pos, SEEK_SET);
					return pos;
				}
			}
		}
	}
	return -1;
}

/*
 * echo temp file contents to display -- used on refresh ONLY!
 */

void
echoback() {
	enable();
	clear();
	gotoxy(1, 1);
	char buf[1024];
	ssize_t bytesread;
	int line_count = 0;
	seekline(aoffset);
	while ((bytesread = read(tempfd, buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < bytesread; i++) {
			if (write(stdout, &buf[i], 1) != 1) {
				perror("write");
				close(tempfd);
				exit(1);
			}
			if (buf[i] == '\n') {
				line_count++;
				if (line_count >= 22) {
					disable();
					return;
				}
			}
		}
	}

	if (bytesread < 0)
		perror("read");
	disable();
}

int
copy(char * input, char * dest)
{
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return(EXIT_FAILURE);
	} else if (pid == 0) {
		execl("/bin/cp", "cp", input, dest, (char *)NULL);
		perror("execl");
		return(EXIT_FAILURE);
	} else {
		wait(NULL);
	}
	return 0;
}

void
notim()
{
	enable();
	tobottom();
	printf(notimplemented);
	disable();
}

int
main(int argc, char ** argv)
{
	if (argc < 2) {
		fprintf(stderr, "Please provide a file\n");
		exit(1);
	}
	char * buf = "\0";
	struct stat st;
	sourcefb = argv[1];
	if (stat(argv[1], &st) == -1) {
		newfile = 1;
		int fd = open(argv[1], O_RDONLY | O_CREAT);
		close(fd);
	}
	sourcefd = open(argv[1], O_RDONLY);
	sprintf(buf, "/tmp/%s", basename(argv[1]));
	filebuf = buf;
	copy(argv[1], buf);
	tempfd = open(buf, O_RDWR);
	printf("\033[2J");
	echoback();
	tobottom();
	if (!newfile)
		printf("%s: %d bytes.", basename(sourcefb), st.st_size);
	else
		printf("%s: new file: %d bytes.", basename(sourcefb), st.st_size);
	disable();
	syncxy();
	unsigned char c;
	while (read(0, &c, 1) == 1) {
		switch (c) {
		case 'k':
		case 0x00: // up
			y--;
			if (y == 0 && aoffset > 0) {
				scrollup=1;
				aoffset--;
				y++;
				echoback();
			}
			break;
		case 'j':
		case 0x01: // down
			y++;
			if (y == 24) {
				aoffset++;
				y--;
				echoback();
			}
			break;
		case 'h':
		case 0x02: // left
			x--;
			break;
		case 'l':
		case 0x03: // right
			x++;
			break;
		case ':':
			commandmode();
			break;
		case 'i':
			insertmode();
			break;
		case 'a':
			x++;
			insertmode();
			break;
		case 'd':
			notim();
			break;
		case 'H':
			notim();
			break;
		case 'u':
			notim();
			break;
		default:
			break;
		}
		syncxy();
	}
	exit(1);
}
