// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
  int r;

  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
/**
* This method waits for the IDE device to become ready before proceeding.
* It checks for specific flags in the status register and waits until the device is ready.
* If <paramref name="checkerr"/> is set to 1, it also checks for error conditions.
* If an error is detected and <paramref name="checkerr"/> is set to 1, the method returns -1.
* 
* @param checkerr Flag to indicate whether to check for error conditions (1 for yes, 0 for no).
* @return 0 if the IDE device is ready, -1 if an error is detected and checkerr is set to 1.
* @exception None
*
* Example:
* idewait(1);
*/

void
ideinit(void)
{
  int i;

  initlock(&idelock, "ide");
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  /**
  * This method initializes the IDE by setting up the necessary locks, enabling IO APIC for IDE interrupts, and checking for the presence of disk 1.
  * It then switches back to disk 0.
  *
  * Exceptions:
  * - None
  *
  * Example:
  * ideinit();
  */
  for(i=0; i<1000; i++){
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

/**
* This method starts an IDE operation for the specified block in the file system.
* It checks if the input buffer <paramref name="b"/> is valid and within the file system size.
* It calculates the number of sectors per block, sector number, read and write commands based on sector size.
* It generates an interrupt, sets the number of sectors, and sets the sector address for IDE operation.
* It handles reading or writing data to/from the IDE device based on buffer flags.
* @param b Pointer to the buffer structure representing the block to operate on.
* @throws panic if the input buffer is NULL, block number is incorrect, or sector per block is greater than 7.
* @example
* struct buf *myBuffer = getBuffer(); // Get a buffer
* idestart(myBuffer); // Start IDE operation for the specified buffer
*/
static void
idestart(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, sector_per_block);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

/**
* This method handles interrupt requests for the IDE controller. It processes the queued buffers, reads data if needed, updates buffer flags, wakes up processes waiting for buffers, and starts disk operations on the next buffer in the queue.
* 
* @exception If there are no buffers in the queue, the method returns without processing anything.
* 
* @example
* ideintr();
*/
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

/**
* This method performs input/output operations on a buffer <paramref name="b"/> in the IDE subsystem.
* It ensures that the buffer is locked, checks for valid and dirty flags, and verifies the presence of IDE disk 1.
* It acquires the IDE lock, appends the buffer to the idequeue, starts the disk if necessary, and waits for the request to finish.
* If any of the conditions are not met, it triggers a panic.
* 
* @param b Pointer to the buffer struct
* @throws panic if the buffer is not locked, there is nothing to do, or IDE disk 1 is not present
* @example
* struct buf *myBuffer;
* iderw(myBuffer);
*/
void
iderw(struct buf *b)
{
  struct buf **pp;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b;

  // Start disk if necessary.
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
    sleep(b, &idelock);
  }


  release(&idelock);
}
