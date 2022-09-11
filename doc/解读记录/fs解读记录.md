# fs解读记录

### 1.一些宏定义

```c
/* fs.h */
#define DISKNO		1
#define BY2SECT		512					// 512 byte per sector
#define SECT2BLK	(BY2BLK/BY2SECT)	// sectors per block

#define DISKMAP		0x10000000			// 文件系统进程地址空间中 块缓存位置
#define DISKMAX		0x40000000
```

```c
/* fsformat.c */
#define NBLOCK		1024				// 1024 blocks per disk
uint32_t nbitblock; // 表示位图占用了多少磁盘块
uint32_t nextbno;	// 下一个空闲块

struct Block {
    uint8_t data[BY2BLK];	// 块数据
    uint32_t type;			// 块类型
} disk[NBLOCK];

enum {
    BLOCK_FREE	= 0,
    BLOCK_BOOT	= 1,
    BLOCK_BMAP	= 2,
    BLOCK_SUPER	= 3,
    BLOCK_DATA	= 4,
    BLOCK_FILE	= 5,
    BLOCK_INDEX	= 6
};
```

```c
/* include/fs.h */
#define BY2BLK		BY2PG				// 块大小和页大小相同，都是4096-4KB-0x1000
#define BIT2BLK		(BY2BLK * 8)

#define MAXNAMELEN	128
#define MAXPATHLEN	1024
#define NDIRECT		10					// 文件描述块中的直接指针数
#define NINDIRECT	(BY2BLK / 4)		// 一个块的间接指针数
#define MAXFILESIZE	(NINDIRECT * BY2BLK)// 最大文件容量（直接10 + 间接 11~） 即等价于整个NINDIRECT块了
#define BY2FILE		256					// 文件描述块大小

struct File {
    u_char f_name[MAXNAMELEN];
    u_int  f_size;						// 文件大小 byte
    u_int  f_type;
    u_int  f_direct[NDIRECT];			// 文件直接指针（块号）
    u_int  f_indirect;					// 文件间接指针（块号）
    struct File *f_dir;					// 指向这个文件所属的目录
    
    u_char f_pad[BY2FILE - MAXNAMELEN - 4 - 4 - NDIRECT * 4 -4 -4];
};

#define FILE2BLK	(BY2BLK / sizeof(struct File)) // 一个块多少文件描述块
#define FTYPE_REG	0	// 普通文件
#define FTYPE_FIR	1	// 目录
#define FS_MAGIC	0x68286097

struct Super {
    u_int s_magic;		// FS_MAGIC
    u_int s_nblocks;	// disk中总块数
    struct File s_root;	// 根目录结点
}
```

```c
/* fs.c */
u_int *bitmap; 	// 指向块的位图，方便位图管理操作
```



### 2.函数操作解释

#### （1）fsformat.c 创建磁盘镜像文件

主函数：

```c
int main(int argc, char **argv) {
    int i;

    init_disk();

    if(argc < 3 || (strcmp(argv[2], "-r") == 0 && argc != 4)) {
        fprintf(stderr, "Usage: fsformat gxemul/fs.img files...\n\
       fsformat gxemul/fs.img -r DIR\n");
        exit(0);
    }

    if(strcmp(argv[2], "-r") == 0) {
        for (i = 3; i < argc; ++i) {
            write_directory(&super.s_root, argv[i]);
        }
    }
    else {
        for(i = 2; i < argc; ++i) {
            write_file(&super.s_root, argv[i]);
        }
    }

    flush_bitmap();
    finish_fs(argv[1]);

    return 0;
}

```



其他

```c
void init_disk() { /* 初始化disk块 */
    int i, diff;
    
    /* boot sector 块 */
    disk[0].type = BLOCK_BOOT;	
    
    /* super 块 */
    disk[1].type = BLOCK_SUPER;
    super.s_magic = FS_MAGIC;
    super.s_nblocks = NBLOCK;
    super.s_root.f_type = FTYPE_DIR;
    strcpy(super.s_root.f_name, "/");
    
    /* data 块的位图管理 */
    nbitblock = (NBLOCK + BIT2BLK - 1) / BIT2BLK; 
    // 只要NBLOCK > 0 就得占一块
    // 1024块的话，仅用1024bit，占一块里(BIT2BLK = 8 * 4096bit)很小的一部分
    nextbno = 2 + nbitblock;
    for (i = 0; i < nbitblock; ++i) {
        disk[2+i].type = BLOCK_BMAP;
        memset(disk[2+i].data, 0xff, BY2BLK);
    }
    if (NBLOCK != nbitblock * BIT2BLK) {
        diff = NBLOCK % BIT2BLK / 8;
        memset(disk[2+(nbitblock-1)].data + diff, 0x00, BY2BLK - diff);
    }
    
}
```

。



工具函数：

```c
int make_link_block(struct File *dirf, int nblk) {
    int bno = next_block(BLOCK_FILE);
    save_block_link(dirf, nblk, bno);
    dirf->f_size += BY2BLK;
    return bno;
}

int save_block_link(struct File *f, int nblk, int bno) {
    if (nblk < NINDIRECT) user_panic("file is too large!"); 
    // 这里能用panic嘛
    
    // nblk指要存入的地方，若小于NDIRECT（10），则存入直接指针；否则存入间接指针
    // 此处操作体现了间接指针从NDIRECT后开始使用，确实方便
    if (nblk < NDIRECT) f->f_direct[nblk] = bno;
    else {
        if (f->f_indirect == 0) // 空的间接指针,则分配一块
            f->f_indirect = next_block(BLOCK_INDEX);
        ((uint32_t *)(disk[f->f_indirect].data))[nblk] = bno;
    }
}

int next_block(int type) {
    disk[nextbno].type = type;
    return nextbno++;
}
```

make_link_block(dirf, nblk)：

save_block_link(f, nblk, bno)：

next_block(type)：返回一个空闲的磁盘块块号，并初始化改磁盘块类型未type





```c
struct File *create_file(struct File *dirf) {
    /* 在dirf目录文件下创建一个空白的文件控制块，并返回 */
    struct File *dirblk;
    int i, bno, found;
    int nblk = dirf->f_size / BY2BLK;
    for (i = 0; i < nblk; ++i) {
        if (i < NDIRECT) bno = dirf->f_direct[i];
        else bno = ((int *)(disk[dirf->f_indirect].data))[i];
        // 注意File.f_indirect指针的用法: disk[dirf->f_indirect]
        dirblk = (struct File *)disk[bno].data;
        
        for (found = 0; found < FILE2BLK; ++found)
            if (dirblk[found].f_name[0] == '\0') return &dirblk[found];
    }
    
    bno = make_link_block(dirf, nblk);
    return (struct File *)disk[bno].data;
}
```

``` c
void write_file(struct File *dirf, const char *path) {
    int iblk = 0, r = 0;
    int n = sizeof(disk[0].data);
    uint8_t buffer[n+1], *dist;
    struct File *target = create_file(dirf);
    
    if (target == NULL) return;	// create_file未完成
    
    int fd = open(path, O_RDONLY);
    const char *fname = strrchr(path, '/'); // 获取文件名（没有路径前缀）
    if(fname) fname++;
    else fname = path;
    strcpy(target->f_name, fname);

}
```

```c
void write_directory(struct File *dirf, char *name)
```



#### (2) fs.c 用户态文件系统实现

通过ide.c与磁盘镜像进行交互

**工具函数**

地址相关验证函数

```c
u_int diskaddr(u_int blockno) {
    /* 获取blockno所在“文件系统进程”中的虚拟地址 */
    if (super && blockno >= super->s_nblocks)
        user_panic("Error: blockno is over disk's nblocks!");
    return DISKMAP + blockno * BY2BLK; // 0x10000000 + [0, 1023]*0x1000
}
```

```c
u_int va_is_mapped(u_int va) { /* 判断是否映射: 即有没有页表和页目录 */
    return ((*vpd)[PDX(va)] & PTE_V) && ((*vpt)[VPN(va)] & PTE_V);
    // 用户进程 获取页目录项内容:(*vpd)[PDX(va)] 获取页表项内容:(*vpt)[VPN(va)]
    // 可回顾lab4: fork部分内容 fork.c
}
u_int block_is_mapped(u_int blockno) {
    u_int va = diskaddr(blockno);
    if (va_is_mapped(va)) return va;
    return 0;
}
```

```c
u_int va_is_dirty(u_int va) { /* 判断是否脏:需写回。块缓存技术 */
	return (*vpt)[VPN(va)] & PTE_D;
}

u_int block_is_dirty(u_int blockno) {
	u_int va = diskaddr(blockno);
	return va_is_mapped(va) && va_is_dirty(va);
}
```



块操作相关函数

```c
int map_block(u_int blockno) {
    if (block_is_mapped(blockno)) return 0;
    return syscall_mem_alloc(0, diskaddr(blockno), PTE_R | PTE_V);
}
void unmap_block(u_int blockno) {
    // 课设代码此处又有多余int r;声明 类似的地方很多
    if (!block_is_mapped(blockno)) return;
    if (!block_is_free(blockno) && block_is_dirty(blockno)) 
        write_block(blockno);
    syscall_mem_unmap(0, diskaddr(blockno));
    user_assert(!block_is_mapped(blockno));// 和user_panic一并，验证用
}
```

```c
int read_block(u_int blockno, void **blk, u_int *isnew) {
    u_int va;
    if (super && blockno >= super->s_nblocks)
        user_panic("reading non-existent block %08x\n", blockno);
    if (bitmap && block_is_free(blockno))
        user_panic("readding free block %08x\n", blockno);
    
    va = diskaddr(blockno);
    if (block_is_mapped(blockno)) { // 该块在内存中
        if (isnew) *isnew = 0;
    } else {						// 该块不在内存中
        if (isnew) *isnew = 1;
        syscall_mem_alloc(0, va, PTE_V | PTE_R);
        ide_read(0, blockno*SECT2BLK, (void *)va, SECT2BLK);
    }
    if (blk) *blk = (void *)va;	// *blk的内容为地址va
    // **blk到底是什么含义呢？
    return 0;
}
```

```c
int write_block(u_int blockno) {  /* 写一个磁盘块 */
    u_int va;
    if (!block_is_mapped(blockno)) // 这地方没有被页表映射，没法写
        user_panic("write unmapped block %08x", blockno);
    va = diskaddr(block);
    ide_write(0, blockno * SECT2BLK, (void *)va, SECT2BLK);
    // 写一个扇区回去 ？ 再看ide_write
    syscall_mem_map(0, va, 0, va, PTE_V | PTE_R | PTE_LIBRARY);
    // 没懂，为啥要共享
}
```

```c
void free_block(u_int blockno) { /* 释放一个磁盘块 */
    if (blockno == 0 || super == 0 || blockno >= super->s_nblocks) return;
    bitmap[blockno >> 5] |= (1 << (blockno & 0x1f));
    // bitmap每个元素4byte即32bit
}
int block_is_free(u_int blockno) {
    if (super == 0 || blockno >= super->s_nblocks) return 0;
    if (bitmap[blockno >> 5] & (1 << (blockno & 0x1f))) return 1;
    // 课设代码这里用了 blockno / 32 和 blockno % 32，显然不如位运算
    return 0;
}
```

```c
int alloc_block(void) { /* 分配使用一个磁盘块，返回它的blockno */
    int r, bno;
    if ((r = alloc_block_num()) < 0) return r;
    bno = r;
    if ((r = map_block(bno))) {	// 分配失败(返回负数)
        free_block(bno); // 便把这块free掉
        return r;
    }
    return bno;
}
int alloc_block_num(void) { /* 遍历块号(位图)，寻找处最小未用块，标记并返回 */
    int blockno;
    for (blockno = 3; blockno < super->s_nblocks; ++blockno) {
        if (bitmap[blockno >> 5] & (1 << (blockno & 0x1f))) {
            bitmap[blockno >> 5] &= ~(1 << (blockno & 0x1f)); // 取反即置零
            write_block(blockno / BIT2BLK + 2); // 将这变化同步到ide磁盘中
            return blockno;
        }
    }
    return -E_NO_DISK;
}
```



**文件系统实现函数**

建立与初始化

```c
void fs_init(void) {
    read_super();
    check_write_block();
    read_bitmap();
}
```

```c
void read_super(void) {}
void check_write_block(void) {}
void read_bitmap(void) {}
```



底层功能实现函数

```c
int file_block_walk(struct File *f, u_int filebno, u_int **ppdiskbno, u_int alloc) { /* 找到第filebno块在磁盘中的位置 */
    int r;
    u_int *ptr;
    void *blk;
    
    if (filebno < NDIRECT) {	// 用的是直接索引
        ptr = &f->f_direct[filebno];
    } else if (filebno < NINDIRECT) { // 用的是间接索引
        if (f->f_indirect == 0) { 
            // 如果没有间接索引块，且设定了alloc，则分配一块做间接索引
            if (alloc == 0) return -E_NOT_FOUND;
            if ((r = alloc_block()) < 0) return r;
            f->f_indirect = r;
        }
        if ((r = read_block(f->f_indirect, &blk, 0)) < 0) return r;
        ptr = (u_int *)blk + filebno;
    } else return -E_INVAL;
    
    *ppdiskbno = ptr; // 将结果存进ppdiskbno
    return 0;
}
```



```c
int file_block_walk() {} // ok
int file_map_block() {}
int file_clear_block() {}
int file_get_block() {}
int file_dirty() {}
```



```c
int dir_lookup(struct File *dir, char *name, struct File **file) {}
int dir_alloc_file(struct File *dir, struct File **file) {}
```

```c
char * skip_slash(char *p) {
    while (*p == '/') ++p;
    return p;
}
int walk_path() {}
```



```c
int file_open(char *path, struct File **file) {}
int file_create(char *path, struct File **file) {}
int file_truncate(struct File *f, u_int newsize) {}
int file_set_size(struct File *f, u_int newsize) {}
void file_flush(struct File *f) {}
void file_close(struct File *f) {}
int file_remove(char *path) {}
```



```c
void fs_sync(void)
```

