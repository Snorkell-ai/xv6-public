#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(void)
{
  int tail;
/**
* Initializes the log for the specified device.
* 
* @param dev The device number for which the log is being initialized.
* 
* @exception panic If the size of the logheader structure is greater than or equal to BSIZE.
* 
* Example:
* initlog(1);
*/

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  /**
  * This method installs transactions by copying log blocks to destination blocks on disk.
  * It reads log blocks and destination blocks, copies the data from log blocks to destination blocks, and writes the destination blocks to disk.
  * 
  * @exception If there is an error reading or writing blocks, an exception may be thrown.
  * 
  * Example:
  * install_trans();
  */
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  /**
  * This method reads the log header information from the specified device and stores it in the log structure.
  * It reads the log header data from the device starting at the specified location.
  * 
  * @throws Exception if there is an error reading the log header or releasing the buffer.
  * 
  * Example:
  * read_head();
  */
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

/**
* This method recovers the system state from the log by performing the following steps:
* 1. Reads the head of the log.
/**
* This method writes the log header information to the specified device.
* It reads the log header from the device, updates the number of blocks, and writes the updated header back to the device.
* 
* @throws Exception if there is an error reading or writing to the device.
* 
* Example:
* write_head();
*/
* 
* @exception This method may throw exceptions if there are errors during the recovery process.
* 
* @example
* recover_from_log();
*/
static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
      /**
      * This method begins an operation by acquiring the log lock and checking if there is enough space in the log for the operation to proceed. 
      * If there is not enough space, it waits for the committing process to finish before proceeding.
      * 
      * Exceptions:
      * - None
      * 
      * Example:
      * begin_op();
      */
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
    /**
    * This method ends an operation by decrementing the outstanding count in the log. If the outstanding count reaches 0, it sets a flag to commit the changes. 
    * It ensures that the log is not already committing, and if so, it panics. If committing is required, it calls the commit function without holding locks to prevent sleeping with locks.
    * 
    * Exceptions:
    * - If log.committing is already true, it will panic.
    * 
    * Example:
    * end_op();
    */
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    /**
    * This method writes log data from cache blocks to log blocks.
    * It iterates through the log blocks and copies data from cache blocks to log blocks.
    * 
    * @exception If there is an error reading or writing data from/to the blocks, an exception may be thrown.
    * 
    * @example
    * write_log();
    */
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

/**
* This method commits the changes made in the log to disk by writing modified blocks from cache to log, writing the header to disk, and installing writes to home locations. If there are no changes in the log, the method does nothing.
* 
* @exception None
* 
* Example:
* commit();
*/
static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

/**
* This method writes the contents of the input buffer <paramref name="b"/> to the log. 
* It checks if the log size is not exceeded and if there is an ongoing transaction. 
* It then acquires the log lock, checks for duplicate block numbers in the log, updates the log header, 
* marks the buffer as dirty to prevent eviction, and releases the log lock.
* @param b The buffer containing the data to be written to the log.
* @throws panic("too big a transaction") if the log size is exceeded or almost exceeded.
* @throws panic("log_write outside of trans") if called outside of a transaction.
* Example:
* struct buf myBuffer;
* log_write(&myBuffer);
*/
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n)
    log.lh.n++;
  b->flags |= B_DIRTY; // prevent eviction
  release(&log.lock);
}

