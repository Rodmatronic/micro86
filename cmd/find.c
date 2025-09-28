#include "../include/types.h"
#include "../include/stat.h"
#include "../include/stdio.h"
#include "../include/fs.h"

void
find(char *path)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if (stat(path, &st) < 0) {
        fprintf(stderr, "find: cannot stat '%s'\n", path);
        return;
    }

    printf("%s\n", path);

    // Only directories need further processing
    if (st.mode & S_IFDIR) {
        if ((fd = open(path, 0)) < 0) {
            fprintf(stderr, "find: cannot open directory '%s'\n", path);
            return;
        }
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            fprintf(stderr, "find: path too long to process '%s'\n", path);
            close(fd);
            return;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        // Read directory entries
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0) // Skip free entries
                continue;
            
            if (de.name[0] == '.' && de.name[1] == 0) 
                continue;
            if (de.name[0] == '.' && de.name[1] == '.' && de.name[2] == 0) 
                continue;

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            find(buf);
        }
        close(fd);
    }
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        find(".");
    } else {
        for (int i = 1; i < argc; i++) {
            find(argv[i]);
        }
    }
    exit(0);
}
