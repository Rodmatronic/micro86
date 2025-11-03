/*
 * ved - visual editor
 */

#include <stdio.h>
#include <fcntl.h>
#include <stat.h>
#include <types.h>
#include <errno.h>

size_t buffsize = 1;
char *buff;
size_t modchar = 0;
int currline = 0;
int scrollline = 0;
int interactive = 0;
int firstrun = 1;
int ibuff = 0;

int resize_buff(int size) {
	char *tmp = malloc(size);
	buffsize += size;
	strcpy(tmp, buff);
	buff = tmp;
	return 0;
}

void enable(){
	struct ttyb t;
	gtty(&t);
	t.tflags = ECHO;
	stty(&t);
}

void disable(){
	struct ttyb t;
	gtty(&t);
	t.tflags |= RAW;
	t.tflags &= ~ECHO;
	stty(&t);
}

void gotoxy(int x, int y) {
	printf("\x1b[%d;%dH", y, x);
}

// int insline(char *ins, int l, int r) {
// 	char *tmp1 = malloc(1);
// 	char *tmp2 = malloc(1);
// 	size_t s;
// 	resize_buff(strlen(ins));
//
// 	for(int i; i < strlen(buff) && l >= 0; i++) {
// 		if (buff[i] == '\n') {
// 			l--;
// 			s = i;
// 		}
// 	}
// 	if (s < modchar) {
// 		modchar = s;
// 	}
//
// 	tmp1 = malloc(s);
//
// }

int printbuff() {
	char s[1] = "a";

	if (firstrun) {
		firstrun = 0;
		printf("\033[H\033[2J");
		for (int row = 0; row < 24; row++) {
			for (int col = 0; col < 80; col++) {
				if (ibuff < strlen(buff)){
					s[0] = buff[ibuff];
					write(stdout, s, 1);
				} else {
					write(stdout, "\n", 1);
					break;
				}
				ibuff++;
				if (buff[ibuff] == '\n' || row >= 80) {
					break;
				}
			}
		}
	} else {
		if (scrollline < currline) {
			scrollline = currline;
			printf("\n");
			gotoxy(0, 23);
			printf("    ");
			gotoxy(0, 22);
			for (int col = 0; col < 80; col++) {
				if (ibuff < strlen(buff)){
					s[0] = buff[ibuff];
					write(stdout, s, 1);
				} else {
					write(stdout, "\n", 1);
					break;
				}
				ibuff++;
				if (buff[ibuff] == '\n') {
					break;
				}
			}
			//printf("\n");
		}
		gotoxy(0, 24);
	}
	if (!interactive) printf("cmd>");
	return 0;
}

int writebufftofd(int *fd){
	char *writebuff = malloc(strlen(buff));
	strcpy(writebuff, buff);
	memmove(writebuff, writebuff + modchar, buffsize - modchar + 1);
	lseek(fd, modchar, SEEK_SET);
	write(fd, writebuff, strlen(writebuff));
	return 0;
}

int cmd(int *fd, struct stat st) {
	char cmdline[512];
	unsigned char c;

	read(fd, buff, buffsize);
	modchar = st.st_size;
	printbuff();
	/*
	printf("\033[H\033[2J");
	printf("%s\n", buff);

	printf("DEBUG:\n");
	printf("File size: %d\n", st.st_size);
	printf("modchar: %d\n", modchar);
	printf("buffer size: %d\n", buffsize);
	printf("buffer string length: %d\n", strlen(buff));
	*/

	//printf("cmd>");

	if (!interactive) {
		strcpy(cmdline, gets(stdin, sizeof(cmdline)));
		switch (cmdline[0]) {
			case 'q':
				exit(0);
				break;
			case 'w':
				writebufftofd(fd);
				break;
			case 'l':
				currline++;
				break;
			case 'i':
				interactive = 1;
				break;
			default:
				resize_buff(strlen(cmdline));
				strcat(buff, cmdline);
		}

	} else {
		disable();
		while (read(0, &c, 1) == 1) {
			switch (c) {
				case 0x00: // up
					currline--;
					break;
				case 0x01: // down
					currline++;
					break;
				case 0x02: // left
					interactive = 0;
					break;
				// case 0x03: // right
				// 	x++;
				// 	break;
				default:
				 	break;
			}
		}
	}
	return 0;
}

int main (int argc, char *argv[]) {
	buff = malloc(buffsize);
	int fd;
	struct stat st;

	if (argc > 1) {
		fd = open(argv[1], O_RDWR | O_CREAT);
		if (fd < 0) {
			perror(argv[1]);
			return 1;
		}
		stat(argv[1], &st);
		resize_buff(st.st_size-1);
		while(1) {
			stat(argv[1], &st);
			cmd(fd, st);
		}
	}

	return 0;
}
