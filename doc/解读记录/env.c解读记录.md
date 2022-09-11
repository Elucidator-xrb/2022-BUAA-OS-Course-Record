# env.c解读记录

**变量声明**

```c
struct Env *envs = NULL;
struct Env *curenv = NULL;

extern void env_pop_tf(struct Trapframe *tf, int id);
extern void lcontext(u_int contxt);
```



**ASID相关操作**

```c
static u_int asid_bitmap[2] = {0}
// 两个int共8字节，64位，来表示最多64个进程同时跑（64个ASID分配）

static u_int asid_alloc();	
// 分配出一个asid(0~63)，修改asid_bitmap以记录
static void asid_free(u_int);
// 释放一个asid(0~63)
```



```c
u_int mkenvid(struct Env *e) {
    u_int idx = e - envs; // 该env地址（指针）相对与envs的偏移，即数组envs中第几个env
    u_int asid = asid_alloc(); // 分配出一个asid来
    return (asid << (1 + LOG2NENV)) | (1 << LOG2NENV) | idx;
    // LOG2NENV定义在include/env.h中，表示操作系统最大进程数目1024的位数：10（log2嘛）
    // |------|1|----------|
    // {6'b_asid, 1'b1, 10'b_idx}
}
```

构造一个唯一的进程id	（前提：e本身合法）



**env_init及相关变量**

```c
/* env.h */
LIST_HEAD(Env_list, Env);
//	struct Env_list { 
// 		struct Env *lh_first;
//	}
struct Env {
    ...
    LIST_ENTRY(Env) env_link;	// 链表项
    //	struct {
    //		struct Env *le_next;
    //		struct Env **le_prev;
    //	} env_link;
    ...
}


static struct Env_list env_free_list;	// 空闲进程链表
struct Env_list env_sched_list[2];		// 调度进程链表

```



```c
int envid2env(u_int envid, struct Env **penv, int chekcperm) {
    struct Env *e;
    
    if (envid == 0) {
        *penv = curenv;
        return 0;
    } // envid=0对应curenv：在user/fork.c的duppage函数中间接被利用
    
    e = envs + ENVX(envid);
    if (e->env_status == ENV_FREE || e->env_id != envid) {
        *penv = NULL;
        return -E_BAD_ENV;
    }
    
    if (checkperm) {	// 需要检查perm
        if (e != curenv && e->env_parent_id != curenv->env_id) {
            *penv = 0;
            return -E_BAD_ENV;
        }
    }
    
    *penv = e;
    return 0;
}

// 如果envid值为0，则返回curenv <- 没懂
```

通过一个env的id获取其对应的进程控制块的功能。功能正常则返回0，否则非零（负）



```c
void env_init() {
    int i;
    LIST_INIT(&env_free_list);
    LIST_INIT(&env_sched_list[0]);
    LIST_INIT(&env_sched_list[1]);
    
    for (i = NENV - 1; i >= 0; --i) {
        envs[i].env_status = ENV_FREE;
        LIST_INSERT_HEAD(&env_free_list, envs + i, env_link);
    }
}
```

初始化所有进程块，将其链入env_free_list中等待被分配。可以参考mm/pmap.c中page_init()等函数理解



```c
static int env_setup_vm(struct Env *e) {
    int i, r;
    struct Page *p = NULL;
    Pde *pgdir;
    
    if ((r = page_alloc(&p))) {
        panic("env_setup_vm - page alloc error\n");
        return r;
    }
    
    p->pp_ref++;
    pgdir = (Pde *)page2kva(p);
    bzero((void *)pgdir, PDX(UTOP)*sizeof(Pde)); 
    // 将kernel段中指向UTOP以下的页目录清空，此即映射用户段kuseg空间的页目录
    bcopy((void *)boot_pgdir + PDX(UTOP), 
          (void *)pgdir      + PDX(UTOP), PTE2PT - PDX(UTOP));
    // 将kernel段中指向UTOP以下的页目录复制到该进程用户段的页目录中
    e->env_pgdir = pgdir;
    // 设置进程用户页目录
    e->env_cr3 = PADDR(pgdir);
    e->env_pgdir[PDX(UVPT)] = e->env_cr3 | PTE_V;

    return 0;
}
```

为一个进程（struct Env *e）配置好虚拟地址空间



```c
int env_alloc(struct Env **new, u_int parent_id) {
    int r;
    struct Env *e;
    
    /*=============== 获得一个空白的进程控制块 =============*/
    if (LIST_EMPTY(&env_free_list)) {
        *new = NULL;
        return -E_NO_FREE_ENV;
    }
    e = LIST_FIRST(&env_free_list);
    
    /*================== 为进程块初始化 ================*/
    env_setup_vm(e);
    
    e->env_id = mkenvid(e);
    e->env_status = ENV_RUNNABLE;
    e->env_parent_id = parent_id;
    e->env_tf.cp0_status = 0x1000100c;
    e->env_tf.reg[29] = USTACKTOP;
    
    // 善后
    LIST_REMOVE(e, env_link);
    *new = e;
    return 0;
}
```

分配一个进程，成功分配则返回0。



```c
static int load_icode_mapper(u_long va, u_int32_t sgsize, u_char *bin, u_int32_t bin_size, void *user_data) {}

static void load_icode(struct Env *e, u_char *binary, u_int size) {}
```



```c
void env_create_priority(u_char *binary, int size, int priority) {}

void env_create(u_char *binary, int size) {}
```



```c
void env_free(struct Env *e) {}
```



```c
void env_destroy(struct Env *e) {
    env_free(e);
    
    // 如果摧毁的是当前进程，则需要切换另一个进程出来
    if (curenv == e) {
        curenv = NULL;
        // 将系统调用处保留的现场 复制到 时钟中断处的现场
        // 同sys_yield()函数，非正常时钟中断进入sched_yield需如此吧
        // 因为yield之后就是env_run()，而env_run()看得都是时钟中断！
        bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
              (void *)TIMESTACK - sizeof(struct Trapframe),
              sizeof(struct Trapframe));
        printf("i am killed ... \n");
        sched_yield();
    }
}
```



```c
void env_run(struct Env *e) {
    if (curenv != NULL) {
        struct Trapframe *old;
        old = (struct Trapframe *)(TIMESTACK - sizeof(struct Trapframe));
        bcopy(old, &(curenv->env_tf), sizeof(struct Trapframe));
        curenv->env_tf.pc = curenv->env_tf.cp0_epc;
    } // 如果当前有其他进程在跑，终止它并保存现场： TIMESTACK（时钟中断）
    
    curenv = e;	// 切换进程
    ++ curenv->env_runs; // 进程被运行咯
    lcontext(e->env_pgdir); // 切换上下文到当前进程，说白了就是当前页目录
    
    env_pop_tf(&e->env_tf, GET_ENV_ASID(e->env_id)); 
    // 进程执行完，恢复现场
}
```

