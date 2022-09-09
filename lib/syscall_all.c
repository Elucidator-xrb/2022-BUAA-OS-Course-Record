#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>
#include <kerelf.h>
#include "../user/lib.h"

extern char *KERNEL_SP;
extern struct Env *curenv;

/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5)
{
	printcharc((char) c);
	return ;
}

/* Overview:
 * 	This function enables you to copy content of `srcaddr` to `destaddr`.
 *
 * Pre-Condition:
 * 	`destaddr` and `srcaddr` can't be NULL. Also, the `srcaddr` area
 * 	shouldn't overlap the `destaddr`, otherwise the behavior of this
 * 	function is undefined.
 *
 * Post-Condition:
 * 	the content of `destaddr` area(from `destaddr` to `destaddr`+`len`) will
 * be same as that of `srcaddr` area.
 */
void *memcpy(void *destaddr, void const *srcaddr, u_int len)
{
	char *dest = destaddr;
	char const *src = srcaddr;

	while (len-- > 0) {
		*dest++ = *src++;
	}

	return destaddr;
}

/* Overview:
 *	This function provides the environment id of current process.
 *
 * Post-Condition:
 * 	return the current environment id
 */
u_int sys_getenvid(void)
{
	return curenv->env_id;
}

/* Overview:
 *	This function enables the current process to give up CPU.
 *
 * Post-Condition:
 * 	Deschedule current process. This function will never return.
 *
 * Note:
 *  For convenience, you can just give up the current time slice.
 */
/*** exercise 4.6 ***/
void sys_yield(void)
{
    bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
          (void *)TIMESTACK - sizeof(struct Trapframe),
          sizeof(struct Trapframe));
    sched_yield();
}

/* Overview:
 * 	This function is used to destroy the current environment.
 *
 * Pre-Condition:
 * 	The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 when error occurs.
 */
int sys_env_destroy(int sysno, u_int envid)
{
	/*
		printf("[%08x] exiting gracefully\n", curenv->env_id);
		env_destroy(curenv);
	*/
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0) {
		return r;
	}

	//printf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

/* Overview:
 * 	Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 * 	xstacktop points to one byte past exception stack.
 *
 * Post-Condition:
 * 	The envid's pagefault handler will be set to `func` and its
 * 	exception stack will be set to `xstacktop`.
 * 	Returns 0 on success, < 0 on error.
 */
/*** exercise 4.12 ***/
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
{
	struct Env *env;
	int ret;

    if ((ret = envid2env(envid, &env, 0))) return ret;
    env->env_pgfault_handler = func;
    env->env_xstacktop = xstacktop;

	return 0;
	//	panic("sys_set_pgfault_handler not implemented");
}

/* Overview:
 * 	Allocate a page of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 *
 * 	If a page is already mapped at 'va', that page is unmapped as a
 * side-effect.
 *
 * Pre-Condition:
 * perm -- PTE_V is required,
 *         PTE_COW is not allowed(return -E_INVAL),
 *         other bits are optional.
 *
 * Post-Condition:
 * Return 0 on success, < 0 on error
 *	- va must be < UTOP
 *	- env may modify its own address space or the address space of its children
 */
/*** exercise 4.3 ***/
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
{
	// Your code here.
	struct Env *env;
	struct Page *ppage;
	int ret;
	ret = 0;

    if (va >= UTOP)                          return -E_INVAL; // Invalid va
    if ((perm & PTE_COW) || !(perm & PTE_V)) return -E_INVAL; // Invalid perm

    if ((ret = envid2env(envid, &env, 1)))  return ret; // Get env failed
    if ((ret = page_alloc(&ppage)))         return ret; // Get page failed
    if ((ret = page_insert(env->env_pgdir, ppage, va, perm))) return ret; 
    // map failed

    return ret;
}

/* Overview:
 * 	Map the page of memory at 'srcva' in srcid's address space
 * at 'dstva' in dstid's address space with permission 'perm'.
 * Perm must have PTE_V to be valid.
 * (Probably we should add a restriction that you can't go from
 * non-writable to writable?) --> you asked me?
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Note:
 * 	Cannot access pages above UTOP.
 */
/*** exercise 4.4 ***/
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
				u_int perm)
{
	int ret;
	u_int round_srcva, round_dstva;
	struct Env *srcenv, *dstenv;
	struct Page *ppage;
	Pte *ppte;

	ppage = NULL;
	ret = 0;
	round_srcva = ROUNDDOWN(srcva, BY2PG);
	round_dstva = ROUNDDOWN(dstva, BY2PG);

    if (srcva >= UTOP || dstva >= UTOP) return -E_INVAL;
    if ((perm & PTE_V) == 0)            return -E_INVAL;
    if ((ret = envid2env(srcid, &srcenv, 0))) return ret;
    if ((ret = envid2env(dstid, &dstenv, 0))) return ret;

    ppage = page_lookup(srcenv->env_pgdir, round_srcva, &ppte);
    if (ppage == NULL) return -E_INVAL;
    if ((*ppte & PTE_R) == 0 && (perm & PTE_R) == 1) return -E_INVAL;//-->I agree.
    ppage = pa2page(PTE_ADDR(*ppte));
    if ((ret = page_insert(dstenv->env_pgdir, ppage, round_dstva, perm)))
        return ret;
    // page_alloc not used

	return ret;
}

/* Overview:
 * 	Unmap the page of memory at 'va' in the address space of 'envid'
 * (if no page is mapped, the function silently succeeds)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Cannot unmap pages above UTOP.
 */
/*** exercise 4.5 ***/
int sys_mem_unmap(int sysno, u_int envid, u_int va)
{
	int ret;
	struct Env *env;

    if (va >= UTOP) return -E_INVAL;
    if ((ret = envid2env(envid, &env, 0))) return ret;
    page_remove(env->env_pgdir, va);

	return ret;
	//	panic("sys_mem_unmap not implemented");
}

/* Overview:
 * 	Allocate a new environment.
 *
 * Pre-Condition:
 * The new child is left as env_alloc created it, except that
 * status is set to ENV_NOT_RUNNABLE and the register set is copied
 * from the current environment.
 *
 * Post-Condition:
 * 	In the child, the register set is tweaked so sys_env_alloc returns 0.
 * 	Returns envid of new environment, or < 0 on error.
 */
/*** exercise 4.8 ***/
int sys_env_alloc(void)
{
	int r;
	struct Env *e;

    if ((r = env_alloc(&e, curenv->env_id))) return r;
    e->env_status = ENV_NOT_RUNNABLE;
    e->env_pri    = curenv->env_pri;
    bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
          (void *)&(e->env_tf), sizeof(struct Trapframe));
    e->env_tf.pc  = e->env_tf.cp0_epc; // has already +4 in trap.h
    e->env_tf.regs[2] = 0; // $v0 fork in son_env should return 0

	return e->env_id;
	//	panic("sys_env_alloc not implemented");
}

/* Overview:
 * 	Set envid's env_status to status.
 *
 * Pre-Condition:
 * 	status should be one of `ENV_RUNNABLE`, `ENV_NOT_RUNNABLE` and
 * `ENV_FREE`. Otherwise return -E_INVAL.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if status is not a valid status for an environment.
 * 	The status of environment will be set to `status` on success.
 */
/*** exercise 4.14 ***/
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
	struct Env *env;
	int ret;
    extern struct Env_list env_sched_list[];

    if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE && status != ENV_FREE)
        return -E_INVAL;
    if ((ret = envid2env(envid, &env, 0))) return ret;

	// why remove ENV_NOT_RUNNABLE? it won't be sched in sched_yield. puzzled
    if (env->env_status != ENV_RUNNABLE && status == ENV_RUNNABLE)
        LIST_INSERT_HEAD(env_sched_list, env, env_sched_link);
    if (env->env_status == ENV_RUNNABLE && status != ENV_RUNNABLE)
        LIST_REMOVE(env, env_sched_link);

    env->env_status = status;

	return 0;
	//	panic("sys_env_set_status not implemented");
}

/* Overview:
 * 	Set envid's trap frame to tf.
 *
 * Pre-Condition:
 * 	`tf` should be valid.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if the environment cannot be manipulated.
 *
 * Note: This hasn't be used now?
 */
int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf)
{

	return 0;
}

/* Overview:
 * 	Kernel panic with message `msg`.
 *
 * Pre-Condition:
 * 	msg can't be NULL
 *
 * Post-Condition:
 * 	This function will make the whole system stop.
 */
void sys_panic(int sysno, char *msg)
{
	// no page_fault_mode -- we are trying to panic!
	panic("%s", TRUP(msg));
}

/* Overview:
 * 	This function enables caller to receive message from
 * other process. To be more specific, it will flag
 * the current process so that other process could send
 * message to it.
 *
 * Pre-Condition:
 * 	`dstva` is valid (Note: 0 is also a valid value for `dstva`).
 *
 * Post-Condition:
 * 	This syscall will set the current process's status to
 * ENV_NOT_RUNNABLE, giving up cpu.
 */
/*** exercise 4.7 ***/
void sys_ipc_recv(int sysno, u_int dstva)
{
    if (dstva >= UTOP) return;

    curenv->env_ipc_recving = 1;
    curenv->env_ipc_dstva   = dstva;
    curenv->env_status      = ENV_NOT_RUNNABLE;
    sys_yield();
}

/* Overview:
 * 	Try to send 'value' to the target env 'envid'.
 *
 * 	The send fails with a return value of -E_IPC_NOT_RECV if the
 * target has not requested IPC with sys_ipc_recv.
 * 	Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends
 *    env_ipc_from is set to the sending envid
 *    env_ipc_value is set to the 'value' parameter
 * 	The target environment is marked runnable again.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Hint: You need to call `envid2env`.
 */
/*** exercise 4.7 ***/
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
					 u_int perm)
{
	int r;
	struct Env *e;
	struct Page *p;

    if (srcva >= UTOP) return -E_INVAL;
    if ((r = envid2env(envid, &e, 0))) return r;
    if (e->env_ipc_recving == 0) return -E_IPC_NOT_RECV;

    e->env_ipc_value = value;
    e->env_ipc_from  = curenv->env_id;
    e->env_ipc_perm  = perm;
    e->env_ipc_recving = 0;
    e->env_status = ENV_RUNNABLE;

    if (srcva != 0) {
        Pte *pte;
        p = page_lookup(curenv->env_pgdir, srcva, &pte);
        if (p == NULL) return -E_INVAL;
        page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm);
    }

	return 0;
}
/* Overview:
 * 	This function is used to write data to device, which is
 * 	represented by its mapped physical address.
 *	Remember to check the validity of device address (see Hint below);
 * 
 * Pre-Condition:
 *      'va' is the starting address of source data, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 * 	
 * Post-Condition:
 *      copy data from 'va' to 'dev' with length 'len'
 *      Return 0 on success.
 *	Return -E_INVAL on address error.
 *      
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 *	 Physical device address:
 *	* ---------------------------------*
 *	|   device   | start addr | length |
 *	* -----------+------------+--------*
 *	|  console   | 0x10000000 | 0x20   |
 *	|    IDE     | 0x13000000 | 0x4200 |
 *	|    rtc     | 0x15000000 | 0x200  |
 *	* ---------------------------------*
 */
 /*** exercise 5.1 ***/
#define check_csl(a, len) (0x10000000 <= (a) && (a)+(len) <= 0x10000020)
#define check_ide(a, len) (0x13000000 <= (a) && (a)+(len) <= 0x13004200)
#define check_rtc(a, len) (0x15000000 <= (a) && (a)+(len) <= 0x15000200)

int sys_write_dev(int sysno, u_int va, u_int dev, u_int len)
{
    if (!(check_csl(dev, len) || check_ide(dev, len) || check_rtc(dev, len)))
        return -E_INVAL;
    bcopy(va, 0xa0000000 + dev, len);
    return 0;
}

/* Overview:
 * 	This function is used to read data from device, which is
 * 	represented by its mapped physical address.
 *	Remember to check the validity of device address (same as sys_write_dev)
 * 
 * Pre-Condition:
 *      'va' is the starting address of data buffer, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 * 
 * Post-Condition:
 *      copy data from 'dev' to 'va' with length 'len'
 *      Return 0 on success, < 0 on error
 *      
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 */
 /*** exercise 5.1 ***/
int sys_read_dev(int sysno, u_int va, u_int dev, u_int len)
{
    if (!(check_csl(dev, len) || check_ide(dev, len) || check_rtc(dev, len)))
        return -E_INVAL;
    bcopy(0xa0000000 + dev, va, len);
    return 0;
}


int strlen(const char *s) {
	int n;
	for (n = 0; *s; s++) 
		n++;
	return n;
}


#define TMPPAGE		(BY2PG)
#define TMPPAGETOP	(TMPPAGE+BY2PG)
/*
int kernel_init_stack(struct Env *env, char **argv, u_int *init_esp) {
    int argc, i, r, tot;
	char *strings;
	u_int *args;

    struct Page *ppage;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (tot)
	tot = 0;
	for (argc=0; argv[argc]; argc++)
		tot += strlen(argv[argc])+1;

	// Make sure everything will fit in the initial stack page
	if (ROUND(tot, 4) + 4 * (argc + 3) > BY2PG) return -E_NO_MEM;

	// Determine where to place the strings and the args array
	strings = (char*)TMPPAGETOP - tot;
	args = (u_int*)(TMPPAGETOP - ROUND(tot, 4) - 4*(argc+1));

    if ((r = page_alloc(&ppage)))   return r;   // should we ? not sure
	if ((r = page_insert(env->env_pgdir, ppage, TMPPAGE, PTE_V|PTE_R))) return r;

	//  - copy the argument strings into the stack page at 'strings'
	char *ctemp,*argv_temp;
	u_int j;
	ctemp = strings;
	for (i = 0;i < argc; i++) {
		argv_temp = argv[i];
		for(j=0;j < strlen(argv[i]);j++) {
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
	for (i = 0; i < argc; i++) {
		args[i] = (u_int)ctemp;
		ctemp += strlen(argv[i]) + 1;
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

    Pte *ppte;
    u_int srcva = TMPPAGE;
    u_int dstva = USTACKTOP - BY2PG;
    ppage = page_lookup(env->env_pgdir, srcva, &ppte);
    ppage = pa2page(PTE_ADDR(*ppte));
    if ((r = page_insert(env->env_pgdir, ppage, dstva, PTE_V | PTE_R))) {
        page_remove(env->env_pgdir, srcva);
        return r;
    }
	page_remove(env->env_pgdir, srcva);
	return 0;
}
*/

static int
kernel_init_stack(u_int child, char **argv, u_int *init_esp)
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

	if ((r = sys_mem_alloc(0, 0, TMPPAGE, PTE_V|PTE_R)) < 0)
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

	if ((r = sys_mem_map(0, 0, TMPPAGE, child, USTACKTOP-BY2PG, PTE_V|PTE_R)) < 0)
		goto error;
	if ((r = sys_mem_unmap(0, 0, TMPPAGE)) < 0)
		goto error;

	return 0;

error:
	sys_mem_unmap(0, 0, TMPPAGE);
	return r;
}

int kernel_load_elf(u_char *binary, Elf32_Phdr *ph, struct Env *env) {
    u_long va = ph->p_vaddr;
    u_int32_t seg_size = ph->p_memsz;
	u_int32_t bin_size = ph->p_filesz;
    u_char *bin = binary + ph->p_offset;
printf("va: %x\n", va);
 	u_long i;
	u_long tmp = USTACKTOP;
    int r;
    u_long offset = va - ROUNDDOWN(va, BY2PG);
    int size = 0;

    struct Page *p = NULL;
    u_int perm = PTE_V | PTE_R;

    if (offset) {
        if ((r = page_alloc(&p))) return r;
        page_insert(env->env_pgdir, p, va, PTE_R);
        size = MIN(bin_size, BY2PG - offset);
        bcopy(bin, page2kva(p) + offset, size);
    }

	for (i = size; i < bin_size; i += BY2PG) {
        if ((r = page_alloc(&p))) return r;
        page_insert(env->env_pgdir, p, va + i, PTE_R);
        size = MIN(bin_size - i, BY2PG);
        bcopy(bin + i, page2kva(p), size);
    }
	
	while (i < seg_size) {
		if ((r = page_alloc(&p))) return r;
        page_insert(env->env_pgdir, p, va + i, PTE_R);
        i += BY2PG;
	}

	return 0;  
}

int kernel_load_icode(struct Env *e, u_char *binary, u_int size) {
    int r;
    r = env_load_icode(binary, size, e);
}

/*
int sys_exec(int sysno, char** argv, void* elfbuf, void* _binary, void* _binaryStat) {
    Elf32_Ehdr* elf = (Elf32_Ehdr*) elfbuf;
    Elf32_Phdr* ph;
    int r = 0;
    char* binary = (char*) _binary;

    u_int esp = 0; 
    u_int envid = curenv->env_id;
    printf("envid [%x]\n", envid);
    r = kernel_init_stack(envid, argv, &esp);
    if (r < 0){
        return r;
    }
    struct Stat* binaryStat = (struct Stat*)_binaryStat;
    
    kernel_load_icode(curenv, binary, binaryStat->st_size);
    //size = binaryStat->st_size;
    //printf("seting tf..\n");
    bcopy((void*)KERNEL_SP - sizeof(struct Trapframe), &(curenv->env_tf), sizeof(struct Trapframe));

    struct Trapframe* tf;
    tf = &(curenv->env_tf);
    (tf->regs)[29] = esp;
    tf->cp0_epc = UTEXT;
    tf->pc = UTEXT;
printf("Before run!\n");
    env_run(curenv);
    return 0;
}
*/


int sys_exec(int sysno, u_int envid, char **argv, void *elfbuf, void *binary, void *b_state)
{
    int r, i;

    Elf32_Ehdr *elf;
    Elf32_Phdr *ph;
    Elf32_Half ph_entry_cnt, ph_entry_size;
    u_char *ph_entry_start;

    struct Stat *state;
    struct Env *e;
    u_int esp;

    Pte *pt;
    u_int pdeno, pteno, pa;
    u_int pn, va;

    elf = (Elf32_Ehdr *) elfbuf;

    ph_entry_cnt = elf->e_phnum;
    ph_entry_size = elf->e_phentsize;
    ph_entry_start = binary + elf->e_phoff;
    printf("cnt:%d, size:%d, start:%x\n", ph_entry_cnt, ph_entry_size, ph_entry_start);

    if ((r = envid2env(envid, &e, 0))) panic("Failed to get env");

    kernel_init_stack(envid, argv, &esp);
    printf("After init stack. [%08x]\n", envid);


    // flush all mapped pages in the user portion of e
    for (pdeno = 0; pdeno < PDX(UTOP); ++pdeno) {
        if (!(e->env_pgdir[pdeno] & PTE_V)) continue;
        pa = PTE_ADDR(e->env_pgdir[pdeno]);
        pt = (Pte *)KADDR(pa);
        for (pteno = 0; pteno <= PTX(~0); ++pteno) {
            if (pt[pteno] & PTE_V)
                page_remove(e->env_pgdir, (pdeno << PDSHIFT) | pteno << PGSHIFT);
        }
        e->env_pgdir[pdeno] = 0;
        page_decref(pa2page(pa));
        tlb_invalidate(e->env_pgdir, UVPT + (pdeno << PGSHIFT));
    }
   
    for (i = 0; i < elf->e_phnum; ++i) { 
        ph = (Elf32_Phdr *)ph_entry_start;
        printf("[p-type]:%d\n", ph->p_type);
        if (ph->p_type == PT_LOAD) {
            if ((r = kernel_load_elf(binary, ph, e))) 
                panic("Failed when loading elf");
            printf("[load one section]\n");
        }
        ph_entry_start += ph_entry_size;
    }
printf("After load elf\n");

    e->env_tf.pc = UTEXT;
    //e->env_tf.cp0_epc = UTEXT;
    e->env_tf.regs[29] = esp;
    e->env_tf.cp0_status = 0x1000100c;
printf("Before run\n");
    //sched_yield();
    return 0;
}
