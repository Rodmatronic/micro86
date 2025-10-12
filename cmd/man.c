#include <types.h>
#include <stdio.h>
#include <fcntl.h>

#define MAX_LINE 512
#define MAX_PATH 100

void remove_backslashes(char *src, char *dst) {
    int j = 0;
    for (int i = 0; src[i]; i++) {
        if (src[i] == '\\' || src[i] == '\"') continue;
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

int read_line(int fd, char *buf, int max) {
    int i = 0;
    char c;
    while (i < max - 1) {
        if (read(fd, &c, 1) != 1) {
            if (i == 0) return -1;
            break;
        }
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

void write_line(int fd, char *line) {
    write(fd, line, strlen(line));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: man <command> [section]\n");
        exit(1);
    }

    char *command = argv[1];
    int section = (argc >= 3) ? atoi(argv[2]) : 1;

    char man_path[MAX_PATH];
    char tmp_path[MAX_PATH];
    sprintf(man_path, "/usr/man/man%d/%s.%d", section, command, section);
    sprintf(tmp_path, "/tmp/%s.%d", command, section);

    int fd_man = open(man_path, O_RDONLY);
    if (fd_man < 0) {
        fprintf(stderr, "man: cannot open %s\n", man_path);
        exit(1);
    }

    int fd_tmp = open(tmp_path, O_CREAT | O_WRONLY);
    if (fd_tmp < 0) {
        fprintf(stderr, "man: cannot create %s\n", tmp_path);
        close(fd_man);
        exit(1);
    }

    char raw_line[MAX_LINE];
    char cleaned_line[MAX_LINE];
    int in_indent = 0;

    while (read_line(fd_man, raw_line, MAX_LINE) >= 0) {
        remove_backslashes(raw_line, cleaned_line);

        if (cleaned_line[0] == '.') {
            char *cmd = cleaned_line + 1;
            char *arg = strchr(cmd, ' ');
            if (arg) *arg++ = '\0';
            while (arg && (*arg == ' ' || *arg == '\t')) arg++;

            if (strcmp(cmd, "TH") == 0 && arg) {
                char *name = arg;
                char *sec = strchr(name, ' ');
                if (sec) {
                    *sec++ = '\0';
                    while (*sec == ' ' || *sec == '\t') sec++;
                }

                char left_str[50] = {0};
                char *p = name;
                int idx = 0;
                while (*p) left_str[idx++] = toupper(*p++);
                left_str[idx++] = '(';
                p = sec;
                while (*p && *p != ' ') left_str[idx++] = *p++;
                left_str[idx++] = ')';
                left_str[idx] = '\0';

                char center_str[] = "General Commands Manual";
                int len_left = strlen(left_str);
                int len_center = strlen(center_str);
                int center_start = (80 - len_center) / 2;
                int right_start = 80 - len_left;

                char header[81];
                memset(header, ' ', 80);
                memmove(header, left_str, len_left);
                memmove(header + center_start, center_str, len_center);
                memmove(header + right_start, left_str, len_left);
                header[80] = '\0';
                write_line(fd_tmp, header);
                write(fd_tmp, "\n", 1);
            } 
            else if (strcmp(cmd, "SH") == 0 && arg) {
		write(fd_tmp, "\n", 1);
                write_line(fd_tmp, arg);
                write(fd_tmp, "\n", 1);
                in_indent = 1;
            } 
            else if ((strcmp(cmd, "B") == 0 || strcmp(cmd, "I") == 0 || strcmp(cmd, "IR") == 0) && arg) {
                write_line(fd_tmp, arg);
                write(fd_tmp, "\n", 1);
            }
        } else {
            if (in_indent) {
                write(fd_tmp, "        ", 8);
            }
            write_line(fd_tmp, cleaned_line);
            write(fd_tmp, "\n", 1);
        }
    }

    close(fd_man);
    close(fd_tmp);

    int pid = fork();
    if (pid == 0) {
        execl("/bin/cat", "cat", tmp_path, 0);
        exit(0);
    } else if (pid > 0) {
        wait(0);
    }

    exit(0);
}
