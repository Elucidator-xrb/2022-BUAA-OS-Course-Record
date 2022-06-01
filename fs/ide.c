/*
 * operations on IDE disk.
 */

#include "fs.h"
#include "lib.h"
#include <mmu.h>

// Overview:
// 	read data from IDE disk. First issue a read request through
// 	disk register and then copy data from disk buffer
// 	(512 bytes, a sector) to destination array.
//
// Parameters:
//	diskno: disk number.
// 	secno: start sector number.
// 	dst: destination for data read from IDE disk.
// 	nsecs: the number of sectors to read.
//
// Post-Condition:
// 	If error occurrs during the read of the IDE disk, panic.
//
// Hint: use syscalls to access device registers and buffers
/*** exercise 5.2 ***/
#define IDE_ADDR 0x13000000
#define IDE_OFFSET_ADDR ((IDE_ADDR) + 0x0000)
#define IDE_ID_ADDR ((IDE_ADDR) + 0x0010)
#define IDE_OP_ADDR ((IDE_ADDR) + 0x0020)
#define IDE_STATUS_ADDR ((IDE_ADDR) + 0x0030)
#define IDE_BUFFER_ADDR ((IDE_ADDR) + 0x4000)

void
ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs)
{
	// 0x200: the size of a sector: 512 bytes.
    int offset_begin = secno * 0x200;
    int offset_end = offset_begin + nsecs * 0x200;
    int offset = 0;

    int offset_cur;
    int rw_tmp;

    while (offset_begin + offset < offset_end) {
        offset_cur = offset_begin + offset;

        /* step1: set device registers, prepare to read */
        if (syscall_write_dev(&diskno, IDE_ID_ADDR, 4)) // select IDE id
            user_panic("IDE_READ Error: failed to write IDE_ID!");
        if (syscall_write_dev(&offset_cur, IDE_OFFSET_ADDR, 4)) // offset
            user_panic("IDE_READ Error: failed to write IDE_OFFSET!");
        rw_tmp = 0;
        if (syscall_write_dev(&rw_tmp, IDE_OP_ADDR, 4)) // start to read
            user_panic("IDE_READ Error: failed to write IDE_OP!");

        /* step2: check whether ide_read is succeeded or not */
        if (syscall_read_dev(&rw_tmp, IDE_STATUS_ADDR, 4))
            user_panic("IDE_READ Error: failed to read IDE_STATUS!");
        if (rw_tmp == 0) user_panic("IDE_READ Error: operation failed!");

        /* step3: copy content from dev buffer to destination addr */
        if (syscall_read_dev(dst + offset, IDE_BUFFER_ADDR, 512))
            user_panic("IDE_READ Error: failed to read IDE_BUFFER!");

        offset += 0x200;
    }
}


// Overview:
// 	write data to IDE disk.
//
// Parameters:
//	diskno: disk number.
//	secno: start sector number.
// 	src: the source data to write into IDE disk.
//	nsecs: the number of sectors to write.
//
// Post-Condition:
//	If error occurrs during the read of the IDE disk, panic.
//
// Hint: use syscalls to access device registers and buffers
/*** exercise 5.2 ***/
void
ide_write(u_int diskno, u_int secno, void *src, u_int nsecs)
{
    int offset_begin = secno * 0x200;
    int offset_end = offset_begin + nsecs * 0x200;
    int offset = 0;

    int offset_cur;
    int rw_tmp;

	// DO NOT DELETE WRITEF !!!
	writef("diskno: %d\n", diskno);

    while (offset_begin + offset < offset_end) {
        offset_cur = offset_begin + offset;

        /* step1: write target content to dev buffer */
        if (syscall_write_dev(src + offset, IDE_BUFFER_ADDR, 512))
            user_panic("IDE_WRITE Error: failed to write IDE_BUFFER!");

        /* step2: set device registers, prepare to write */
        if (syscall_write_dev(&diskno, IDE_ID_ADDR, 4)) // select IDE id
            user_panic("IDE_WRITE Error: failed to write IDE_ID!");
        if (syscall_write_dev(&offset_cur, IDE_OFFSET_ADDR, 4)) // offset
            user_panic("IDE_WRITE Error: failed to write IDE_OFFSET!");
        rw_tmp = 1;
        if (syscall_write_dev(&rw_tmp, IDE_OP_ADDR, 4)) // start to write
            user_panic("IDE_WRITE Error: failed to write IDE_OP!");

        /* step3: check whether ide_write is succeeded or not */
        if (syscall_read_dev(&rw_tmp, IDE_STATUS_ADDR, 4))
            user_panic("IDE_WRITE Error: failed to read IDE_STATUS!");
        if (rw_tmp == 0) user_panic("IDE_WRITE Error: operation failed!");

        offset += 0x200;
    }
}
