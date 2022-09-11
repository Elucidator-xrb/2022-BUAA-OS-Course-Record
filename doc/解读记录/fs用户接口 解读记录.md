# fs用户接口 解读记录

### 一、宏和相关定义

```c
/* fd.h */
#define debug 0

#define MAXFD 32
#define FILEBASE 0x60000000
#define FDTABLE (FILEBASE-PDMAP)

#define INDEX2FD(i)		(FDTABLE  + (i)*BY2PG) // 根据fd序号找到fd地址
#define INDEX2DATA(i)	(FILEBASE + (i)*PDMAP)

struct Dev { /* 设备块(包括基本方法)，可能为: file, console, pipe */
	int dev_id;
	char *dev_name;
	int (*dev_read)(struct Fd *, void *, u_int, u_int);
	int (*dev_write)(struct Fd *, const void *, u_int, u_int);
	int (*dev_close)(struct Fd *);
	int (*dev_stat)(struct Fd *, struct Stat *);
	int (*dev_seek)(struct Fd *, u_int);
};
extern struct Dev devcons;
extern struct Dev devfile;
extern struct Dev devpipe;

struct Fd { /* 文件描述符 */
	u_int fd_dev_id;	
	u_int fd_offset;	// 当前的位置，用于fseek
	u_int fd_omode;		// 文件打开模式
};

struct Filefd { /* 带上文件的文件描述符 */
	struct Fd f_fd;		// 文件描述符
	u_int f_fileid;		// 文件id
	struct File f_file;	// 文件FCB (struct File)
};

struct Stat {  /* 状态 ？不懂 */
	char st_name[MAXNAMELEN];
	u_int st_size;
	u_int st_isdir;
	struct Dev *st_dev;
};


/* fd.c */

```





### 二、函数操作解释

#### 1. user/file.c

**系统设备**：文件file

```c
struct Dev devfile = {
	.dev_id =	'f',
	.dev_name  = "file",
	.dev_read  = file_read,
	.dev_write = file_write,
	.dev_close = file_close,
	.dev_stat  = file_stat,
}; // 定义了devfile
```

----



**提供给用户使用**

```c
int open(const char *path, int mode) {
    struct Fd *fd;
    struct Filefd * ffd;
    int r;
    u_int va, size, fileid;
    u_int i;
    
    if ((r = fd_alloc(&fd))) return r; // 分配一个文件描述符
    if ((r = fsipc_open(path, mode, fd))) return r; // 核心，ipc请求服务
    va  = fd2data(fd); // 确定文件数据的位置（一个fd对应确定的data位置）
    ffd = fd;	       // 带文件的文件描述符
    size   = ffd->f_file.f_size; // 确定文件大小
    fileid = ffd->f_fileid;		 // 确定文件id
    for (i = 0; i < size; i += BY2PG) {// 为文件数据分配内存（映射到物理地址）
        if ((r = syscall_mem_alloc(0, va + i, PTE_R | PTE_V))) return r;
        if ((r = fsipc_map(fileid, i, va + i))) return r;
    }
    return fd2num(fd); // 返回文件描述符编号
}
```

```c
int remove(const char *path) {
    return fsipc_remove(path);
}
```

```c
int sync(void) {
    return fsipc_sync();
}
```

```c
int create()
```



```c
int read_map(int fdnum, u_int offset, void **blk) {
    /* 找到该文件块对应的页的虚拟地址 */
    int r;
    u_int va;
    struct Fd *fd;
    if ((r = fd_lookup(fdnum, &fd))) return r;	// fd找不到
    
    if (fd->fd_dev_id != devfile.dev_id) return -E_INVAL;
    if (offset >= MAXFILESIZE) return -E_NO_DISK;
    va = fd2data(fd) + offset;
    if (!((*vpd)[PDX(va)] & PTE_V) || !((*vpt)[VPN(va)] & PTE_V)) 
        return -E_NO_DISK;
    *blk = (void *)va;
    return 0;
}
```

```c
int ftruncate(int fdnum, u_int size) {
    /* 将一个打开的文件大小调整到size，（增加或截断）：配合file_write使用 */
}
```



**提供给系统设备**

```c
int file_close(struct Fd *fd) {
    struct Filefd *ffd;
    int r;
    u_int va, size, fileid;
    u_int i;
    /* 获取文件基本信息，是基操了 */
    va  = fd2data(fd);
    ffd = fd;
    size   = ffd->f_file.f_size;
    fileid = ffd->f_fileid;
    for (i = 0; i < size; i += BY2PG) {
        fsipc_dirty(fileid, i);// 将整个文件内容标记为脏，有点浪费的感觉
    }
    if ((r = fsipc_close(fileid)) < 0) { // 关闭文件
        writef("cannot close the file\n");
        return r;
    }
    for (i = 0; i < size; i += BY2PG) { // 取消内存映射
        if ((r = syscall_mem_unmap(0, va + i))) {
            writef("cannot unmap the file\n");
            return r;
        }
    }
    return 0;
}
```

```c
static int file_read(struct Fd *fd, void *buf, u_int n, u_int offset) {
    /* 从fd对应文件offset处开始，读取n个字节到buf中 */
    struct Filefd *ffd;
    u_int size;
    
    ffd = fd;
    size = f->file.f_size;	// 为了避免读过头超出本文件
    if (offset > size) return 0;				// 偏移量超过文件大小
    if (offset + n > size) n = size - offset;	// 欲读取内容超出文件
    user_bcopy((char *)fd2data(fd) + offset, buf, n); // 本质不过就是拷贝
    return n;
}
```

```c
static int file_write(struct Fd *fd, const void *buf, u_int n, u_int offset) {
    /* 从buf中写n个字节到fd对应文件offset处 */
    struct Filefd *ffd;
    int size, total;
    int r;
    
    ffd = fd;
    size = f->f_file.f_size;
    total = offset + n;
    if (total > MAXFILESIZE) return -E_NO_DISK; // 写内容超出最大文件容量
    if (total > size) {
        if ((r = ftruncate(fd2num(fd), total))) return r;
    }
    user_bcopy(buf, (char *)fd2data(fd) + offset, n);
    return n;
}
```

```c
static int file_stat(struct Fd *fd, struct Stat *stat) {
    /* 读取文件基本信息 */
    struct Filefd *ffd = fd;
    
    strcpy(st->st_name, (char *)ffd->f_file.f_name);
    st->st_size = ffd->f_file.f_size;
    st->st_isdir = ffd->f_file.f_type == FTYPE_DIR;
    return 0;
}
```



#### 2. user/fd.c

```c
/* 初始化设备指针数组 */
static struct Dev *devtab[] = {
    &devfile,
    &devcons,
    &devpipe,
    0	// 数组结束标记，好用法（见下for循环条件）
};
```

**· dev/fd相关操作**

```c
int dev_lookup(int dev_id, struct Dev **dev) {
    /* 查找id为dev_id的设备 */
    for (int i = 0; devtab[i]; ++i) {
        if (devtab[i]->dev_id == dev_id) {
            *dev = devtab[i];
            return 0;
        }
    }
    writef("[%08x] unknown device type %d\n", env->envid, dev_id);
    return -E_INVAL;
}
```

```c
int fd_alloc(struct Fd **fd) {
    /* 分配一个序号最小的未使用的fd （只是找到地址赋给目标，但不会该fd改变状态）*/
    u_int va;
    u_int fdno;
    
    for (fdno = 0; fdno < MAXFD - 1; ++fdno) {
        va = INDEX2FD(fdno);
        if (((*vpd)[va / PDMAP] & PTE_V) == 0) { // 看va的页目录项是否存在
            *fd = (struct Fd *)va;
            return 0;
        }
        if (((*vpt)[va / BY2PG] & PTE_V) == 0) { // 看va的页表项是否存在
            *fd = (struct Fd *)va;
            return 0;
        }
    }
    // 一轮下来，页表项都有，说明fd已经分配到上限，不能再多了
    return -E_MAX_OPEN;
}
```

```c
void fd_close(struct Fd *fd) {
    syscall_mem_umap(0, (u_int)fd);
}
```

```c
void fd_lookup(int fdnum, struct Fd **fd) {
    /* 仅仅只是检查一下fdnum对应的fd是否被使用， 已使用则用fd指向 */
    u_int va;
    if (fdnum >= MAXFD) return -E_INVAL;
    va = INDEX2FD(fdnum);
    if (((*vpt)[va / BY2PG] & PTE_V) != 0)  {
        *fd = (struct Fd *)va;
        return 0;
    }
    return -E_INVAL;
}
```

**· 基本转换函数** （其实写成宏会更好看）

```c
#define MAXFD 32
#define FILEBASE 0x60000000
#define FDTABLE (FILEBASE-PDMAP)

#define INDEX2FD(i)		(FDTABLE  + (i)*BY2PG) // 根据fd序号找到fd地址
#define INDEX2DATA(i)	(FILEBASE + (i)*PDMAP)

/* fd -> 其地址 */
u_int fd2data(struct Fd *fd) { return INDEX2DATA(fd2num(fd)); }
/* fd -> 其序号 */
int fd2num(struct Fd *fd) { return ((u_int)fd-FDTABLE) / BY2PG; }
/* 序号 -> 其fd */
int num2fd(int fd) { return fd * BY2PG + FDTABLE; }
// 不是太懂 num和INDEX的命名，应该没区别吧
```

也就是说，在FILEBASE以上存储着文件的数据，FDTABLE中存储着文件描述符。

- `struct Fd *fd`：此fd为fd指针，看其位置可知，其值为FDTABLE+fdnum*BY2PG
- `int fdnum`：此为fd编号，fd从0开始分配，最大MAXFD（32），因其顺序存储易求；也即所谓的`int index`
- `u_int data`：对应文件数据的地址，其实也是指针了

**· 基本操作**

```c
int close(int fdnum) {
    int r;
    struct Dev *dev;
    struct Fd *fd;
    if ((r = fd_lookup(fdnum, &fd))) return r; 			 // 找不到fd
    if ((r = dev_lookup(fd->fd_dev_id, &dev))) return r; // 找不到dev
    
    r = (*dev->dev_close)(fd); // 关闭dev (执行属于dev的close操作)
    fd_close(fd);			   // 关闭fd
    return r;
}
void close_all(void) {
    for (int i = 0; i < MAXFD; ++i) close(i);
}
```

```c
int dup(int oldfdnum, int newfdnum) {
    /* 将newfdnum对应的文件映射到oldfdnum文件相同的物理内存中 */
    int i, r;
    u_int ova, nva, pte;
    struct Fd *oldfd, *newfd;
    
    if ((r = fd_lookup(oldfdnum, &oldfd))) return r; // 找到oldfd
    close(newfdnum);  newfd = INDEX2FD(newfdnum);	 // 找到newfd
    ova = fd2data(oldfd);	// 找到oldfd对应文件数据地址
    nva = fd2data(newfd);	// 找到newfd对应文件数据地址
    if ((*vpd)[PDX(ova)]) {
        for (i = 0; i < PDMAP; i += BY2PG) {
            pte = (*vpt)[VPN(ova + i)];
            if (pte & PTE_V) {
                if ((r = syscall_mem_map(
                    0, ova + i, 
                    0, nva + i,
                    pte & (PTE_V | PTE_R | PTE_LIBRARY)
                ))) goto err; // 为newfd对应的data分配空间（map到oldfd）
            }
        }
    }
    if ((r = syscall_mem_map(
        0, (u_int)oldfd,
        0, (u_int)newfd,
        ((*vpt)[VPN(oldfd)]) & (PTE_V | PTE_R | PTE_LIBRARY)
    ))) goto err;	// 为newfd分配空间空间（map到oldfd）
    
    err:
	syscall_mem_unmap(0, (u_int)newfd);
	for (i = 0; i < PDMAP; i += BY2PG) {
		syscall_mem_unmap(0, nva + i);
	}
	return r;
}
```

```c
int read(int fdnum, void *buf, u_int n) {
    int r;
    struct Dev *dev;
    struct Fd *fd;
	if ((r = fd_lookup(fdnum, &fd))) return r; 			 // 找不到fd
    if ((r = dev_lookup(fd->fd_dev_id, &dev))) return r; // 找不到dev
    
    if ((fd->fd_omode & O_ACCMODE) == O_WRONLY) { // 文件不可读
        wirtef("[%08x] read %d -- bad mode\n", env->env_id, fdnum);
        return -E_INVAL;
    }
    r = (*dev->dev_read)(fd, buf, n, fd->fd_offset); // 读dev
    if (r > 0) { // 读成功，设置偏移，封闭缓冲区'\0'
        fd->fd_offset += r;
        ((char *)buf)[r] = '\0';
    }
    return r;
}
int readn(int fdnum, void *buf, u_int n) {
    int m, tot;
    for (tot = 0; tot < n; tot += m) {
        m = read(fdnum, (char *)buf + tot, n - tot);
        if (m < 0) return m; // 读失败
        if (m == 0) break; // 不动了，读到底了
    }
    return tot;
}
```

```c
int write(int fdnum, void *buf, u_int n) {
    int r;
    struct Dev *dev;
    struct Fd *fd;
	if ((r = fd_lookup(fdnum, &fd))) return r; 			 // 找不到fd
    if ((r = dev_lookup(fd->fd_dev_id, &dev))) return r; // 找不到dev
    
}
```

```c
int seek(int fdnum, u_int offset) {
    /* 重置一下偏移量 (可以想想c库的fseek函数) */
    int r;
    struct Fd *fd;
    if ((r = fd_lookup(fdnum, &fd))) return r;			// 找不到fd
    fd->fd_offset = offset;
    return 0;
}
```

```c
int fstat(int fdnum, struct Stat *stat) {
    int r;
    struct Dev *dev;
    struct Fd *fd;
	if ((r = fd_lookup(fdnum, &fd))) return r; 			 // 找不到fd
    if ((r = dev_lookup(fd->fd_dev_id, &dev))) return r; // 找不到dev
    
    // 初始化stat块，等待被填入
    stat->st_name[0] = 0;
    stat->st_size = 0;
    stat->st_isdir = 0;
    stat->st_dev = dev;
    return (*dev->dev_stat)(fd, stat);
}
int stat(const char *path, struct Stat *stat) {
    int fd, r;
    if ((fd = open(path, O_RDONLY)) < 0) return fd;
    r = fstat(fd, stat);
    close(fd);
    return r;
}
```



#### 3. user/fsipc.c

```c
int fsipc_close(u_int fileid) {
    struct Fsreq_close *req = (struct Fsreq_close *)fsipcbuf;
    
    req->req_fileid = fileid;
    return fsipc(FSREQ_CLOSE, req, 0, 0);
}
```



```c
int fsipc_dirty(u_int fileid, u_int offset) {
    /* 请求文件系统server将文件某块标记为脏 */
    struct Fsreq_dirty *req = (struct Fsreq_dirty *)fsipcbuf;
    
    req->req_fileif = fileid;
    req->req_offset = offset;
    return fsipc(FSREQ_DIRTY, req, 0, 0);
}
```

