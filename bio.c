// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
} bcache;

/**
* This method initializes the buffer cache by creating a linked list of buffers and initializing sleep locks for each buffer.
* It first initializes the lock for the buffer cache.
* Then, it creates a linked list of buffers by setting the head's previous and next pointers to point to itself.
* It iterates through the buffer array, setting each buffer's next pointer to point to the head's next pointer, and previous pointer to point to the head.
* It initializes a sleep lock for each buffer with the name "buffer".
* Finally, it updates the previous and next pointers of the head and the current buffer to maintain the linked list structure.
*
* @exception None
*
* Example:
* binit();
*/
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

//PAGEBREAK!
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

/**
* This method retrieves a buffer from the cache based on the device and block number provided. 
* If the block is already cached, it increments the reference count and returns the buffer.
* If the block is not cached, it recycles an unused buffer by selecting one with refcnt=0 and B_DIRTY flag not set.
* If no suitable buffer is found, it triggers a panic.
* 
* @param dev The device number of the buffer to retrieve.
* @param blockno The block number of the buffer to retrieve.
* @return A pointer to the retrieved buffer.
* @throws panic if no suitable buffer is found in the cache.
* 
* Example:
* struct buf *buffer = bget(1, 10);
*/
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.
  // Even if refcnt==0, B_DIRTY indicates a buffer is in use
  // because log.c has modified it but not yet committed it.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->flags = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

/**
* This method reads a block from the specified device and block number.
* It first retrieves the buffer using the bget function for the given device and block number.
* If the buffer's flags do not indicate that it is valid (B_VALID flag is not set), it invokes iderw to read from the device.
* Returns the buffer containing the data read from the specified block.
* 
* @param dev The device number from which to read the block.
* @param blockno The block number to be read from the device.
* @return A pointer to the buffer containing the data read from the specified block.
* @throws Exception if there is an error reading from the device or if the buffer retrieval fails.
* 
* Example:
* struct buf *buffer = bread(1, 10);
*/
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if((b->flags & B_VALID) == 0) {
    iderw(b);
  }
  return b;
}

/**
* This method writes the contents of the buffer <paramref name="b"/> to disk.
* It first checks if the buffer's lock is held, otherwise it triggers a panic with the message "bwrite".
* It then sets the B_DIRTY flag in the buffer's flags and proceeds to write the buffer using iderw().
* 
* @param b Pointer to the buffer structure to be written to disk.
* @throws PanicException if the buffer's lock is not held.
* @see iderw
* 
* Example:
* struct buf myBuffer;
* bwrite(&myBuffer);
*/
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  b->flags |= B_DIRTY;
  iderw(b);
}

/**
* This method releases a buffer <paramref name="b"/> by decrementing its reference count and updating the buffer cache accordingly.
* It first checks if the buffer lock is held, and if not, it triggers a panic.
* After releasing the buffer lock, it decrements the reference count of the buffer.
* If the reference count reaches 0, it updates the buffer cache to remove the buffer from the list of buffers.
* 
* @param b Pointer to the buffer structure to be released.
* @throws panic if the buffer lock is not held.
* 
* Example:
* struct buf *buffer = get_buffer();
* brelse(buffer);
*/
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}
//PAGEBREAK!
// Blank page.

