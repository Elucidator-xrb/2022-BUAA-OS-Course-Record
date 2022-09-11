## pmap.c解读记录

## 系统启动时操作

```c
static void *alloc(u_int n, u_int align, int clear) {
    extern char end[];
    u_long alloced_mem;
    
    if (freemem == 0) freemem = (u_long)end;
    alloced_mem = freemem = ROUND(freemem, align);
    freemem += n;
    
    if (PADDR(freemem) >= maxpa) {
        panic("out of memory\n");
        return (void *)-E_NO_MEM;
    }
    
    if (clear) bzero((void *)alloced_mem, n);
}
```

将freemem关于align对齐，然后至此开始分配n个字节，clear则清零分配的字节，返回所分配空间的**虚拟地址**（我们都是通过虚拟地址操纵内存）；

**原始工具函数**，在其他相应函数`boot_pgdir_walk()`中分配地址用，freemem不断增加



```c
static Pte *boot_pgdir_walk(Pde *pgdir, u_long va, int create) {
    Pde *pgdir_entry;	// 页目录项地址（指针）
    Pte *pgtable, *pgtable_entry;	// 页表基地址；页表项地址（指针）
    
    /*======= 定位页目录项地址 =========*/
    pgdir_entry = pgdir + PDX(va);
    
    /*======= 定位页表基地址 ==========*/
    if ((*pgdir_entry & PTE_V) == 0) {	// 所找页目录项不存在
        if (create)
            *pgdir_entry = PADDR(alloc(BY2PG, BY2PG, 1)) | PTE_V | PTE_R;
        else return 0;
    }
    pgtable = (Pte *)KADDR(PTE_ADDR(*pgdir_entry));
    
    /*======= 定位页表项地址 =========*/
    pgtable_entry = pgtable + PTX(va);
    
    return pgtable_entry;
}
```

返回找到的页目录基地址pgdir下**va对应的页表项地址**：

- 页目录基地址：pgdir
- va对应-页目录项地址：pgdir_entry，从pgdir偏移页目录号（`PDX(va)`）项
- va对应-页表基地址：页目录项前20位为页表物理页号，得物理地址转内核虚拟地址
- va对应-页表项地址：pgtable_entry， 从pgtable偏移页表号（`PTX(va)`）项

若不存在相应**页目录项**，则用alloc分配一块内存并设置权限位使其成为**页目录项**（我们只负责找到相应的页表项地址，至于页表项存不存在不关心，例如：交给boot_map_segment初始化）



```c
void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm) {
    int i;
    Pte *pgtable_entry;
    for (i = 0, size = ROUND(size, BY2PG); i < size; i += BY2PG) {
    	pgtable_entry = boot_pgdir_walk(pgdir, va + i, 1);
        *pgtable_entry = PTE_ADDR(pa + i) | perm | PTE_V; //生成页表项
    }
}
```

在pgdir下，建立[va，va+size]到[pa, pa+size]的映射。所谓建立映射，就是最终生成页表项（`PTE_ADDR(pa + i) | perm | PTE_V`）



```c
void mips_vm_init() {
    extern char end[];
    extern int mCONTEXT;
    extern struct Env *envs;
    extern struct Page *pages;	
    // 其实pages是在该文件中定义的，只是为了和Env看着对称再写上的，语法上不能写
    
    Pde *pgdir;
    u_int n;
    /*=========================== 分配页目录 =============================*/
    pgdir = alloc(BY2PG, BY2PG, 1); // 此时freemem为0x8040_1000
    mCONTEXT = (int)pgdir;
    boot_pgdir = pgdir;	// pmap.c的全局变量，应该要在其他文件里用
    
    /*========================= 分配页框并映射 ===========================*/
    pages = (struct Page *)alloc(npage * sizeof(struct Page), BY2PG, 1);
    n = ROUND(npage * sizeof(struct Page), BY2PG);
    boot_map_segment(pgdir, UPAGES, n, PADDR(pages), PTE_R);
    /*======================= 分配进程控制块并映射 ========================*/
    envs = (struct Env *)alloc(npage * sizeof(struct Env), BY2PG, 1);
    n = ROUNF(NENV * sizeof(struct Env), BY2PG);
    boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R);
    
    printf("pmap.c:\t mips vm init success\n");
}
```

从end地址（定义在tools/scse0_3.lds中，`0x8040_0000`，通过`extern char end[]`引入）开始：

- 给页目录分配一页的虚拟内存（`0x8040_0000 ~ 0x8040_1000`）；
- - 给所有物理页框（`npage`个`sizeof(struct Page)`）分配对应直接指向的内核虚拟内存，pages[]存储（pages指向其首地址，全区域`0x8040_1000 ~ 0x8043_1000`）；
  - 用`boot_map_segment()`完成从kuseg中的虚拟地址（从UPAGES开始的虚拟地址——`0x7f80_0000 ~ 0x7fc0_0000`）到物理地址（PADDR(pages)）的映射关系建立
- + 给所有进程控制块（`NENV`个`sizeof(struct Env)`）分配对应直接指向的内核虚拟内存，envs[]存储（envs指向其首地址）；（位置上接着之前的物理页框）
  + 用`boot_map_segment()`完成从kuseg中的虚拟地址（从UENVS开始的虚拟地址——`0x7f40_0000 ~ 0x7f80_0000`）到物理地址（PADDR(envs)）的映射关系建立

我们在高地址kseg0（0x8000_0000以上）为Page分配的虚拟地址，可直接通过PADDR转换成相应物理地址，所以可以认为在直接操纵物理地址



-------------------------

## 页式内存管理 Page

### 定义、他处引用

```c
/* pmap.h */
LIST_HEAD(Page_list, Page);
//	struct Page_list {
//		struct Page *lh_first;
//	}
typedef LIST_ENTRY(Page) Page_LIST_enty_t;
//	struct {
//		struct Page *le_next;
//		struct Page **le_prev;
//	}

struct Page {
    Page_LIST_entry_t pp_link;	// 链表项
    // LIST_ENTRY(Page)
    u_short pp_ref;
}
```

```c
/* mmu.h */
#define	BY2PG	4096
#define PDMAP	(4*1024*1024)
#define PGSHIFT	12
#define PDSHIFT	22

#define PDX(va)	((((u_long)(va))>>22) & 0x03FF) // 0x03FF: get last 10 bit
#define PTX(va)	((((u_long)(va))>>12) & 0x03FF)
#define PTE_ADDR(pte)	((u_long)(pte)&~0xFFF)
#define PPN(va)	(((u_long)(va))>>12)
#define VPN(va)	PPN(va)

#define PTE2PT	1024
#define PTE_V	0x0200	// Valid bit
#define PTE_R	0x0400	// "Dirty", but really a write-enable bit.
----
#define PTE_G	0x0100	// Global bit
#define PTE_D   0x0002  // Dirty bit, fileSystem Cached is dirty
#define PTE_COW 0x0001  // Copy On Write : in sys_mem_alloc

#define ULIM	0x80000000

#define PADDR(kva)	({										\
		u_long a = (u_long) (kva);							\
		if (a < ULIM)										\
			panic("PADDR called with invalid kva %08lx", a);\
		a - ULIM;											\
	})
#define KADDR(pa)	({										\
		u_long ppn = PPN(pa);								\
		if (ppn >= npage)									\
			panic("KADDR called with invalid pa %08lx", (u_long)pa);\
		((u_long)(pa)) + ULIM;								\
	})


```

```c
/* pmap.h */
u_long page2ppn(struct Page *pp) { return pp - pages; }	// 页号

u_long page2pa(struct Page *pp) { return page2ppn(pp) << PGSHIFT; } // 页指针（虚拟地址） -> 物理地址：页号左移12位（页长度）

struct Page *pa2page(u_long pa) { return &pages[PPN(pa)]; } // 物理地址 -> 页指针

u_long page2kva(struct Page *pp) { return KADDR(page2pa(pp)); }

u_long va2pa(Pde *pgdir, u_long va) {
    Pte *p; //...
}
```



struct Page *pp 中，pp的值和pa2kva(pp)的值有何区别：好像有些许区别，不明所以



### 正文

```c
void page_init() {
    extern char end[];
    
    LIST_INIT(&page_free_list);
    freemem = ROUND(freemem, BY2PG);
    
    struct Page *cur;	
    // 原本用了page2kva(cur)，但我认为 (u_long) cur = page2kva(cur) ?
    for (cur = pages; cur < freemem; ++cur) {
        cur->pp_ref = 1;
    }
    
//记得修改代码测试看自己的想法对不对
    for (;page2ppn(cur) < npage; ++cur) {
        cur->pp_ref = 0;
        LIST_INSERT_HEAD(&page_free_list, cur, pp_link);
    }
}
```

将所有页纳入管理（`pages[npage]`）：

- 已用的（freemem以下）：相应Page中pp_ref字段设为1；
- 未用的（freemem到pages[]结束）：pp_ref字段设为0，链入page_free_list中

注意：freemem会在首次使用alloc()时与end对齐，每alloc()一次分配一点，经历了`mips_vm_init()`



```c
int page_alloc(struct Page **pp) {
    struct Page *ppage_tmp;
    
    if (LIST_EMPTY(&page_free_list)) return -E_NO_MEM;
    
    ppage_tmp = LIST_FIRST(&page_free_list);	// 从链表头处分配页，得到指针
    LIST_REMOVE(ppage_tmp, pp_link);
    bzero(page2kva(ppage_tmp), BY2PG);	//?这里也感觉ppage_tmp就行，地址一样
    *pp = ppage_tmp;
    
    return 0;
}
```

这个alloc可不是原始alloc分配内存空间了，这个page_alloc在分配空闲页。从page_free_list中取出空闲的页分配出去。

由于需要返回值来返回是否分配成功，所以分配出的页由pp存储，`*pp`即为struct *

分配成功，返回0，否则返回-E_NO_MEM



```c
void page_free(struct Page *pp) {
    if (pp->pp_ref > 0) return;
    
    if (pp->pp_ref == 0) {
        LIST_INSERT_HEAD(&page_free_list, pp, pp_link);
        return;
    }
    
    panic("cgh:pp->pp_ref is less than zero\n");
}
```

释放非空闲页：释放pp_ref为0的页，将该页插回空闲链表



```c
int pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte) {
    Pde *pgdir_entry;
    Pte *pgtable;
    struct Page *ppage;
    int r;
    
    pgdir_entry = pgdir + PDX(va);
    
    if ((*pgdir_entry & PTE_V) == 0) {
        if (create) {
            if (r = page_alloc(&ppage))  return r;
                // true:!0, false:0
            *pgdir_entry = page2pa(ppage) | PTE_V | PTE_R;
            ++ ppage->pp_ref;
        } else {
            *ppte = 0;
            return 0;
        }
    }
    pgtable = (Pte *)KADDR(PTE_ADDR(*pgdir_entry));
    
    *ppte = pgtable + PTX(va);
    
    return 0;
}
```

找到页目录基地址pgdir下va对应的**页表项地址**，并用ppte指向（ppte指向页表项地址，即页表项地址的地址，*ppte为页表项地址， \*\*ppte为页表项内容），即ppte为**页表项地址的地址**；

若不存在相应页表项，则使用page_alloc分配出一页内存，并设置权限位使其成为页表项。**这里和boot_pgdir_walk有所不同，尽可能保证walk到的页表项存在**。

整个操作成功则返回0，不成功（page_alloc失败）则返回-E_NO_MEM（page_alloc返回）



```c
int page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm) {
    Pte *pgtable_entry;
    int r;
    
    // 检查va是否已经建立和pa的映射关系
    pgdir_walk(pgdir, va, 0, &pgtable_entry);
    	// 若pgtable_entry存在且有效
    if (pgtable_entry != 0 && (*pgtable_entry & PTE_V) != 0) {
        if (pa2page(*pgtable_entry) != pp) { // va映射到了另一个物理页框
            page_remove(pgdir, va);	// 解除映射
        } else {							 // va就映射到pp对应页框
            tlb_invalidate(pgdir, va);
            *pgtable_entry = page2pa(pp) | perm | PTE_V; // 更新perm
            return 0;
        }
    }
    
    // 建立va到pa的映射关系
    tlb_invalidate(pgdir, va);
    if (r = pgdir_walk(pgdir, va, 1, &pgtable_entry)) return r;
    *pgtable_entry = page2pa(pp) | perm | PTE_V;
    ++ pp->pp_ref;
    
    return 0;
}
```

将**页目录基地址 pgdir 的两级页表结构对应**的虚拟地址va，映射到**内存控制块 pp 对应**的物理页面，并将页表项权限为设置为 perm。

成功，则返回0



```c
struct Page *page_lookup(Pde *pgdir, u_long va, Pte **ppte) {
    struct Page *ppage;
    Pte *pgtable_entry;
    
    // 检查pgtable_entry是否存在且有效
    pgdir_walk(pgdir, va, 0, &pgtable_entry);
    if (pgtable_entry == 0 || (*pgtable_entry & PTE_V) == 0) {
        return 0;
    }
    
    ppage = pa2page(*pgtable_entry); // 不管页内偏移了，pa2page就是右移12位
    if (ppte) *ppte = pte;	// ? 为什么要判断ppte是否不为0
    return ppage;
}
```

返回**页目录基地址 pgdir **下虚拟地址va对应的物理页面的内存控制块（struct Page），同时将 ppte 指向的空间设为对应的二级页表项地址。 



```c
void page_decref(struct Page *pp) {
    if (--pp->pp_ref == 0) 
        page_free(pp);
}
```

减一次引用，如果pp_ref就此归零，则把该页free了



```c
void page_remove(Pde *pgdir, u_long va) {
    struct Page *ppage;
    Pte *pgtable_entry;
    
    ppage = page_lookup(pgdir, va, &pgtable_entry);
    if (ppage == 0) return;
    page_decref(ppage);	//
   	*pgtable_entry = 0;	// 清理页表项内容
    tlb_invalidate(pgdir, va);
    
    return;
}
```

取消va与其在pgdir下页表项中的物理页的映射



```c
void tlb_invalidate(Pde *pgdir, u_long va) {
    if (curenv) {
        tlb_out(PTE_ADDR(va) | GET_ENV_ASID(curenv->env_id));
    } else {
        tlb_out(PTE_ADDR(va));
    }
}
```

```assembly
LEAF(tlb_out)
nop
	mfc0	k1,CP0_ENTRYHI
	mtc0	a0,CP0_ENTRYHI
	nop
	tlbp  # insert tlbp or tlbwi
	nop
	nop
	nop
	nop
	mfc0	k0,CP0_INDEX
	bltz	k0,NOFOUND
	nop
	mtc0	zero,CP0_ENTRYHI
	mtc0	zero,CP0_ENTRYLO0
	nop
	tlbwi # insert tlbp or tlbwi
NOFOUND:

	mtc0	k1,CP0_ENTRYHI
	
	j	ra
	nop
END(tlb_out)
```

刷新tlb，不是太懂



```c
void pageout(int va, int context) {
    u_long r;
    struct Page *p = NULL;
    
    if (context < 0x80000000)
        panic("tlb refill and alloc error!");
    if (va > 0x7f400000 && va < 0x7f800000)
        panic(">>>>>>>>>>>>it's env's zone!");
    if (va < 0x10000)
        panic("^^^^^^^^TOO LOW^^^^^^^");
    
    if ((r = page_alloc(&p)))
        panic("page alloc error!");
    page_insert((Pde *)context, p, VA2PFN(va), PTE_R);
    printf("pageout:\t@@@___0x%x___@@@  ins a page \n", va);
}
```

内核在捕获到 一个常规的缺页中断（page fault）时（在 MOS 中这个情况特指页缺失），会进入到一个在 `trap_init` 中“注册”的 `handle_tlb` 的内核处理函数中， 这一汇编函数的实现在 lib/genex.S 中，化名为一个叫 `do_refill` 的函数。如果物理页面在页表中存在，则会将其填入 TLB 并 返回异常地址再次执行内存存取的指令。如果物理页面不存在，则会触发一个一般意义的缺页错误，并跳转到 mm/pmap.c 中的 `pageout` 函数中。**如果存取地址是合法的用户空间地址，内核会为对应地址分配并映射一个物理页面（被动地分配页面）来解决缺页的问题。**

