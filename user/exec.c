#include "lib.h"
#include <kerelf.h>

int exec(char *prog, char **argv) {
    int exec_envid;

    u_char elfbuf[512];
	int r;
	int fd;
	u_int child_envid;
	int size, text_start;
	u_int i, *blk;
	u_int esp;
	Elf32_Ehdr* elf;
	Elf32_Phdr* ph;

	char prog_name[MAXNAMELEN];
	strcpy(prog_name, prog);
	int len = strlen(prog);
	if (len <= 2 || prog_name[len-1] != 'b' && prog_name[len-2] != '.')
		strcat(prog_name, ".b");

	// Step 1: Open the file specified by `prog` (prog is the path of the program)
	if((fd = open(prog_name, O_RDONLY)) < 0) {
		// user_panic("spawn ::open line 102 RDONLY wrong !\n");
		return fd;
	}

	// Get ELF file header
	if ((r = readn(fd, elfbuf, sizeof(Elf32_Ehdr))) < 0) return r;
	elf = (Elf32_Ehdr *)elfbuf;

	// Before Step 2 , You had better check the "target" spawned is a execute bin 
	if (!usr_is_elf_format(elf) || elf->e_type != 2) return -E_INVAL;

	
	Elf32_Half ph_entry_cnt = elf->e_phnum;
    Elf32_Half ph_entry_size = elf->e_phentsize;
    u_char *ph_entry_start;
	u_char *binary;
	if ((r = read_map(fd, 0, &binary))) return r;
	fwritef(1, "[binary]:%x\n", binary);
    ph_entry_start = binary + elf->e_phoff;
    fwritef(1, "cnt:%d, size:%d, start:%x\n", elf->e_phnum, elf->e_phentsize, binary + elf->e_phoff);
    for (i = 0; i < elf->e_phnum; ++i) { 
        ph = (Elf32_Phdr *)ph_entry_start;
        fwritef(1, "[p-type]:%d\n", ph->p_type);
        if (ph->p_type == 1) fwritef(1, "[load one section]\n");
        ph_entry_start += ph_entry_size;
    }

    struct Stat state;

    exec_envid = fork();
    if (exec_envid == 0) for (;;);  // child_env : waiting for modification
    //exec_envid = syscall_getenvid();

    fwritef(1, "exec envid:[%08x]\n", exec_envid);

    if ((r = syscall_exec(exec_envid, argv, elfbuf, binary, &state))) return r;

    return 0;

}

// int exec(char *prog, char **argv) {
//     int exec_envid;

//     int fd;
//     u_char *binary;
//     u_char elfbuf[512];
//     Elf32_Ehdr *elf;
//     struct Stat state;
//     int r, i;

//     char prog_name[MAXNAMELEN];
// 	strcpy(prog_name, prog);
// 	int len = strlen(prog);
// 	if (len <= 2 || prog_name[len-1] != 'b' && prog_name[len-2] != '.')
// 		strcat(prog_name, ".b");

//     exec_envid = fork();
//     if (exec_envid == 0) for (;;);  // child_env : waiting for modification
//     //exec_envid = syscall_getenvid();
// fwritef(1, "name:%s\n", prog_name);
//     if ((fd = open(prog_name, O_RDONLY)) < 0) return fd;
//     if ((r = fstat(fd, &state)) < 0) return r;
//     if ((r = read_map(fd, 0, &binary))) return r;
//     fwritef(1, "[binary]:%x\n", binary);
//     if ((r = readn(fd, elfbuf, sizeof(Elf32_Ehdr))) < 0) return r;
//     close(fd);

//     elf = (Elf32_Ehdr *)elfbuf;
//     Elf32_Half ph_entry_cnt = elf->e_phnum;
//     Elf32_Half ph_entry_size = elf->e_phentsize;
//     u_char *ph_entry_start;
//     Elf32_Phdr *ph;
//     ph_entry_start = binary + elf->e_phoff;
//     fwritef(1, "cnt:%d, size:%d, start:%x\n", elf->e_phnum, elf->e_phentsize, binary + elf->e_phoff);
//     for (i = 0; i < elf->e_phnum; ++i) { 
//         ph = (Elf32_Phdr *)ph_entry_start;
//         fwritef(1, "[p-type]:%d\n", ph->p_type);
//         if (ph->p_type == 1) fwritef(1, "[load one section]\n");
//         ph_entry_start += ph_entry_size;
//     }

//     fwritef(1, "exec envid:[%08x]\n", exec_envid);

//     if ((r = syscall_exec(exec_envid, argv, elfbuf, binary, &state))) return r;

//     return 0;

// }

/*
int exec(char *prog, char **argv) {
    int fd;
    u_char *binary;
    u_char elfbuf[512];
    Elf32_Ehdr *elf;
    struct Stat state;
    int r;

    char prog_name[MAXNAMELEN];
	strcpy(prog_name, prog);
	int len = strlen(prog);
	if (len <= 2 || prog_name[len-1] != 'b' && prog_name[len-2] != '.')
		strcat(prog_name, ".b");

    if ((fd = open(prog_name, O_RDONLY)) < 0) return fd;

    if ((r = fstat(fd, &state)) < 0) return r;
    
    if ((r = read_map(fd, 0, &binary))) return r;
    
    if ((r = readn(fd, elfbuf, sizeof(Elf32_Ehdr))) < 0) return r;
    elf = elfbuf;
    fwritef(1, "cnt:%d, size:%d, start:%x\n", elf->e_phnum, elf->e_ehsize, binary + elf->e_phoff);

    close(fd);

    fwritef(1, "current envid:[%08x]\n", syscall_getenvid());

    if ((r = syscall_exec(argv, elfbuf, binary, &state))) return r;

    return 0;
}
*/