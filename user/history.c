#include "lib.h"

char buf[8192];

void print_file(int fd) {
	long n;
	int r;

	while((n = read(fd, buf, (long)sizeof buf)) > 0)
		if((r = write(1, buf, n)) != n) {
            fwritef(1, "Error: when writing .history\n");
            exit();
        }
	if(n < 0) user_panic("Error: when reading .history: has read %e", n);
}

void umain(int argc, char **argv) {
    int fd = open(".history", O_RDONLY | O_CREAT);
    if (fd < 0) {
        fwritef(1, "Fail to open file .history\n");
        return;
    }
    fwritef(1, "==== shell: history ====\n");
    print_file(fd);
    fwritef(1, "\n==== end of history ====\n");
    close(fd);
}