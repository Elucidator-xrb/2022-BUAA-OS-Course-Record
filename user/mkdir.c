#include "lib.h"

void umain(int argc, char **argv) {
    int fd = open(argv[1], O_RDONLY);

    if (fd >= 0) {
        fwritef(1, "Directory \"%s\" has already exist!\n", argv[1]);
        return;
    }
    
    if (create(argv[1], FTYPE_DIR) < 0) {
        fwritef(1, "Failed to create directory!\n");
    }
}