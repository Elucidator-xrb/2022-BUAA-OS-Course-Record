# include "lib.h"

# define SPACE_FORMAT "|   "
# define BRANC_FORMAT "|-- "
# define END_FORMAT   "`-- "

char path[MAXPATHLEN];

void traverseTree(char *dir, int layer) {
	struct File file;
    int fd;
    int i, n;
    char buf[MAXNAMELEN];

	if ((fd = open(dir, O_RDONLY)) < 0) {
		writef("Fail to open directory \"%s\"!", dir);
        exit();
	}

	while ((n = readn(fd, &file, sizeof(struct File))) 
                              == sizeof(struct File)) {
		if (file.f_name[0] == 0) continue;

		for (i = 0; i < layer; ++i) fwritef(1, SPACE_FORMAT);
        switch (file.f_type) {
            case FTYPE_DIR:
                fwritef(1, BRANC_FORMAT);
                fwritef(1, "\033[36m%s\033[0m\n", file.f_name);
				strcpy(buf, dir);
				strcat(buf, file.f_name); strcat(buf, "/");
				traverseTree(buf, layer + 1);
                break;

            case FTYPE_REG:
                fwritef(1, BRANC_FORMAT);
                fwritef(1, "%s\n", file.f_name);
                break;
        
            case FTYPE_BIN:
                fwritef(1, BRANC_FORMAT);
                fwritef(1, "\033[31m%s\033[0m\n", file.f_name);
                break;

            default:
                break;
        }		
	}

	if (close(fd) < 0) user_panic("Error: failed to close directory\n");
	if (n) user_panic("Error: unsuccessful read in directory \"%s\"\n", dir);
}

void umain(int argc, char **argv) {
    struct Stat state;

    if (argc == 1) {
        traverseTree("/", 0);
    } else if (argc == 2) {
        strcpy(path, argv[1]);
        if (stat(path, &state) < 0) {
            fwritef(1, "Failed to open \"%s\"!\n", path);
            return;
        }
        if (!state.st_isdir) {
            fwritef(1, "Command tree can not traverse non-directory file \"%s\"\n", path);
            return;
        }
        if (path[strlen(path)-1] != '/') strcat(path, "/");
        traverseTree(path, 0);
    } else if (argc == 4) { // with argument
        strcpy(path, argv[1]);
        if (strcmp(argv[2], "-L") == 0) {

        }
    } else {
        fwritef(1, "Wrong format with command \"tree\"!\n");
    }
}
