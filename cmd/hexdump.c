#include "../include/types.h"
#include "../include/user.h"
#include "../include/fs.h"
#include "../include/fcntl.h"

#define BUFFER_SIZE 16

void hexdump(int fd, int cflag) {
    unsigned char buffer[BUFFER_SIZE];
    int offset = 0;
    int bytes_read;
    int i;

    while((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        // Print offset
        if(cflag == 0)
		printf("%07x ", offset);
	else
		printf("%08x ", offset);

        for(int i = 0; i < bytes_read; i += 2) {
            if(i+1 < bytes_read) {
		if(cflag == 0)
			printf("%02x%02x ", buffer[i+1], buffer[i]);
		else
                	printf("%02x %02x ", buffer[i], buffer[i+1]);
            } else {
                printf("%02x   ", buffer[i]);
            }
        }

        if(cflag) {
            printf(" |");
            for(i = 0; i < bytes_read; i++) {
                if(buffer[i] >= 32 && buffer[i] <= 126) {
                    printf("%c", buffer[i]);
                } else {
                    printf(".");
                }
            }
            printf("|");
        }

        printf("\n");
        offset += bytes_read;
    }
}

int
main(int argc, char *argv[])
{
    int fd;
    int cflag = 0;
    char *filename;
    int arg_offset = 1;

    if (argc > 1 && strcmp(argv[1], "-C") == 0) {
        cflag = 1;
        arg_offset = 2; // skip the -C argument
    }

    if (argc <= arg_offset) {
        hexdump(0, cflag);
        exit(0);
    }

    filename = argv[arg_offset];
    if ((fd = open(filename, O_RDONLY)) < 0) {
        fprintf(stderr, "hexdump: cannot open %s\n", filename);
        exit(1);
    }

    hexdump(fd, cflag);
    close(fd);
    exit(0);
}
