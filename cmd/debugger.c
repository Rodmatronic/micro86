#include <errno.h>
#include <stdio.h>
#include <stat.h>
#include <fcntl.h>

void gotoxy(int x, int y) {
	printf("\x1b[%d;%df", y, x);
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

int
main(int argc, char ** argv)
{
/*	printf("Move with the arrows");
	struct ttyb t;
	gtty(&t);
	t.tflags |= RAW;
	t.tflags &= ~ECHO;
	stty(&t);

	int x = 10;
	int y = 10;
	unsigned char c;
	gotoxy(x, y);
	while (read(0, &c, 1) == 1) {
		switch (c) {
		case 0x00: // up
			y--;
			break;
		case 0x01: // down
			y++;
			break;
		case 0x02: // left
			x--;
			break;
		case 0x03: // right
			x++;
			break;
		default:
			break;
		}
	enable();
	gotoxy(1, 1);
	printf("x %d, y %d", x, y);
	disable();
	gotoxy(x, y);
	}

	return 0;*/

	int fd = open("/test", O_CREAT | O_RDWR, 666);
	close(fd);

	for (int i = 0; i < 8; i++){
		printf("\033[4%dm#", i);
	}
		printf("\033[0m\n");
	for (int i = 0; i < 8; i++){
		printf("\033[9%dm#", i);
	}
	printf("\n");
	for (int i = 0; i < 8; i++){
		printf("\033[10%dm#", i);
	}
	printf("\033[0m\n");

	//printf("\033[H\033[2J");

	printf("%d\n", devctl(3, 0, 0));
	struct uproc info;
	if (getproc(24, &info) == 0)
		printf("pid=%d name=%s state=%d\n", info.p_pid, info.name, info.state);
	else
	printf("process not found\n");

	printf("will now segfault\n");
	printf("%s", argv[99]);
	printf("huh?\n");

	exit(1);
}
