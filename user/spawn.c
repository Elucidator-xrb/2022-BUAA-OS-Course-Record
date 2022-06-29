#include "lib.h"
#include <mmu.h>
#include <env.h>
#include <kerelf.h>

#define debug 0
#define TMPPAGE		(BY2PG)
#define TMPPAGETOP	(TMPPAGE+BY2PG)

int
init_stack(u_int child, char **argv, u_int *init_esp)
{
	int argc, i, r, tot;
	char *strings;
	u_int *args;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (tot)
	tot = 0;
	for (argc=0; argv[argc]; argc++)
		tot += strlen(argv[argc])+1;

	// Make sure everything will fit in the initial stack page
	if (ROUND(tot, 4)+4*(argc+3) > BY2PG)
		return -E_NO_MEM;

	// Determine where to place the strings and the args array
	strings = (char*)TMPPAGETOP - tot;
	args = (u_int*)(TMPPAGETOP - ROUND(tot, 4) - 4*(argc+1));

	if ((r = syscall_mem_alloc(0, TMPPAGE, PTE_V|PTE_R)) < 0)
		return r;
	// Replace this with your code to:
	//
	//	- copy the argument strings into the stack page at 'strings'
	char *ctemp,*argv_temp;
	u_int j;
	ctemp = strings;
	for(i = 0;i < argc; i++)
	{
		argv_temp = argv[i];
		for(j=0;j < strlen(argv[i]);j++)
		{
			*ctemp = *argv_temp;
			ctemp++;
			argv_temp++;
		}
		*ctemp = 0;
		ctemp++;
	}
	//	- initialize args[0..argc-1] to be pointers to these strings
	//	  that will be valid addresses for the child environment
	//	  (for whom this page will be at USTACKTOP-BY2PG!).
	ctemp = (char *)(USTACKTOP - TMPPAGETOP + (u_int)strings);
	for(i = 0;i < argc;i++)
	{
		args[i] = (u_int)ctemp;
		ctemp += strlen(argv[i])+1;
	}
	//	- set args[argc] to 0 to null-terminate the args array.
	ctemp--;
	args[argc] = ctemp;
	//	- push two more words onto the child's stack below 'args',
	//	  containing the argc and argv parameters to be passed
	//	  to the child's umain() function.
	u_int *pargv_ptr;
	pargv_ptr = args - 1;
	*pargv_ptr = USTACKTOP - TMPPAGETOP + (u_int)args;
	pargv_ptr--;
	*pargv_ptr = argc;
	//
	//	- set *init_esp to the initial stack pointer for the child
	//
	*init_esp = USTACKTOP - TMPPAGETOP + (u_int)pargv_ptr;
//	*init_esp = USTACKTOP;	// Change this!

	if ((r = syscall_mem_map(0, TMPPAGE, child, USTACKTOP-BY2PG, PTE_V|PTE_R)) < 0)
		goto error;
	if ((r = syscall_mem_unmap(0, TMPPAGE)) < 0)
		goto error;

	return 0;

error:
	syscall_mem_unmap(0, TMPPAGE);
	return r;
}

int usr_is_elf_format(u_char *binary){
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)binary;
	if (ehdr->e_ident[0] == ELFMAG0 &&
        ehdr->e_ident[1] == ELFMAG1 &&
        ehdr->e_ident[2] == ELFMAG2 &&
        ehdr->e_ident[3] == ELFMAG3) {
        return 1;
    }   

    return 0;
}

/* Hint: maybe this function is useful 
 *       If you want to use this func, you should fill it ,it's not hard
 */
/*** for exercise 6.5 ***/
int 
usr_load_elf(int fd, Elf32_Phdr *ph, int child_envid){
	// Similar to load_icode_mapper
	u_long va = ph->p_vaddr;
	u_int32_t seg_size = ph->p_memsz;
	u_int32_t bin_size = ph->p_filesz;
	u_char *bin;

	u_long i;
	u_long tmp = USTACKTOP; 
	// Remeber the one "Invalid memory" page above USTACKTOP ?
	int r;
	int size = 0;
	u_long offset = va - ROUNDDOWN(va, BY2PG);
	if ((r = read_map(fd, ph->p_offset, &bin))) return r;

	if (offset) {
		if ((r = syscall_mem_alloc(child_envid, va, PTE_V | PTE_R)))
			return r;
		if ((r = syscall_mem_map(child_envid, va, 0, tmp, PTE_V | PTE_R)))
			return r;
		size = MIN(bin_size, BY2PG - offset);
		user_bcopy(bin, tmp + offset, size);
		if ((r = syscall_mem_unmap(0, tmp))) return r;
	}

	for (i = size; i < bin_size; i += BY2PG) { //<= change from size to BY2PG
		if ((r = syscall_mem_alloc(child_envid, va + i, PTE_V | PTE_R)))
			return r;
		if ((r = syscall_mem_map(child_envid, va + i, 0, tmp, PTE_V | PTE_R)))
			return r;
		size = MIN(bin_size - i, BY2PG);
		user_bcopy(bin + i, tmp, size);
		if ((r = syscall_mem_unmap(0, tmp))) return r;
	}
	// Needn't worry the cases when "end of bin_size" unaligned with Page, 
	// for the guidebook promise to fill the rest of the Page with 0.
	// So there was a redundant part of code in Lab3, get rid of it here!

	while (i < seg_size) {
		if ((r = syscall_mem_alloc(child_envid, va + i, PTE_V | PTE_R)))
			return r;
		i += BY2PG;
	}

	return 0;
}

/*** exercise 6.5 ***/
int spawn(char *prog, char **argv)
{
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

	// Note 0: some variable may be not used,you can cancel them as you like
	// Step 1: Open the file specified by `prog` (prog is the path of the program)
	if((fd = open(prog_name, O_RDONLY))<0){
		user_panic("spawn ::open line 102 RDONLY wrong !\n");
		return fd;
	}

	// Get ELF file header
	if ((r = readn(fd, elfbuf, sizeof(Elf32_Ehdr))) < 0) return r;
	elf = (Elf32_Ehdr *)elfbuf;

	// Before Step 2 , You had better check the "target" spawned is a execute bin 
	if (!usr_is_elf_format(elf) || elf->e_type != 2) return -E_INVAL;

	// Step 2: Allocate an (child) env (Hint: using syscall_env_alloc())
	//  Similar to fork
	if ((r = syscall_env_alloc()) < 0) return r;
	if (r == 0) {	// when child_env awake
		env = envs + ENVX(syscall_getenvid());
		return 0;
	}
	child_envid = r;

	// Step 3: Using init_stack(...) to initialize the stack of the allocated env
	init_stack(child_envid, argv, &esp);

	// Step 4: Map file's content to new env's text segment
	//   Hint 1: what is the offset of the text segment in file? 
	//           Try to use objdump to find out.
	//   Hint 2: using read_map(...)
	//   Hint 3: Important!!! sometimes, its not safe to use read_map, guess why 
	//           If you understand, you can achieve the "load APP" with any method
	// Note1: Step 1 & 2 need sanity check. Videlicet, you should check whether
	//        the file is opened successfully, and env is allocated successfully.
	// Note2: You can achieve func in any way. Remember to ensure it's correct.
	//        Maybe you can review lab3 
	//  Similar to load_elf
	text_start = elf->e_phoff;
	size = elf->e_phentsize;
	for (i = 0; i < elf->e_phnum; ++i) {
		if ((r = seek(fd, text_start)))        return r; // set an offset
		if ((r = readn(fd, elfbuf, size)) < 0) return r; // read an entry
		ph = (Elf32_Phdr *)elfbuf;
		if (ph->p_type == PT_LOAD) { // loadable program segment
			if ((r = usr_load_elf(fd, ph, child_envid))) return r;
		}
		text_start += size;
	}
	// Your code ends here

	struct Trapframe *tf;
	writef("::::::::::spawn size : %x  sp : %x::::::::\n",size,esp);
	tf = &(envs[ENVX(child_envid)].env_tf);
	tf->pc = UTEXT;
	tf->regs[29]=esp;

	// Share memory
	u_int pdeno = 0;
	u_int pteno = 0;
	u_int pn = 0;
	u_int va = 0;
	for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
		if(!((*vpd)[pdeno] & PTE_V))
			continue;
		for (pteno = 0; pteno <= PTX(~0); pteno++) {
			pn = (pdeno << 10) + pteno;
			if (((*vpt)[pn] & PTE_V) && ((*vpt)[pn] & PTE_LIBRARY)) {
				va = pn * BY2PG;
				if((r = syscall_mem_map(
					0, va, child_envid, va, 
					(PTE_V | PTE_R | PTE_LIBRARY)
				))) {
					writef("va: %x   child_envid: %x   \n", va, child_envid);
					user_panic("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
					return r;
				}
			}
		}
	}

	if ((r = syscall_set_env_status(child_envid, ENV_RUNNABLE))) {
		writef("set child runnable is wrong\n");
		return r;
	}
	return child_envid;		
}

int
spawnl(char *prog, char *args, ...)
{
	return spawn(prog, &args);
}


