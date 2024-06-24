// Fake IDE disk; stores blocks in memory.
// Useful for running kernel without scratch disk.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

extern uchar _binary_fs_img_start[], _binary_fs_img_size[];

static int disksize;
static uchar *memdisk;

/**
* This method initializes the IDE by setting the memdisk pointer to the start of the binary file fs_img and calculating the disk size based on the size of the binary file.
* 
* @exception None
* 
* Example:
* ideinit();
*/
void
ideinit(void)
{
  memdisk = _binary_fs_img_start;
  disksize = (uint)_binary_fs_img_size/BSIZE;
}

/**
* This method represents the interrupt service routine for IDE devices.
* It does not perform any operations and serves as a placeholder for future implementation.
* 
* @throws None
* 
* Example:
* 
* SupportedLanguage.C
* void
* ideintr(void)
* {
*   // no-op
* }
*/
void
ideintr(void)
{
  // no-op
}

/**
* This method reads or writes data to a buffer in the memory disk.
* It checks if the buffer is locked, if the data is valid and dirty, if the request is for disk 1, and if the block number is within range.
* If the buffer is dirty, it updates the memory disk with the buffer data. Otherwise, it updates the buffer with data from the memory disk.
* 
* @param b Pointer to the buffer structure
* @throws panic if the buffer is not locked, if there is nothing to do, if the request is not for disk 1, or if the block is out of range
* @example
* struct buf myBuffer;
* iderw(&myBuffer);
*/
void
iderw(struct buf *b)
{
  uchar *p;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 1)
    panic("iderw: request not for disk 1");
  if(b->blockno >= disksize)
    panic("iderw: block out of range");

  p = memdisk + b->blockno*BSIZE;

  if(b->flags & B_DIRTY){
    b->flags &= ~B_DIRTY;
    memmove(p, b->data, BSIZE);
  } else
    memmove(b->data, p, BSIZE);
  b->flags |= B_VALID;
}
