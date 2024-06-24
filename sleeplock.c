// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

/**
 * Initializes a sleep lock with the provided name.
 * 
 * This function initializes a sleep lock structure with the given name and sets its initial values.
 * 
 * @param lk Pointer to the sleep lock structure to be initialized.
 * @param name Name of the sleep lock.
 * 
 * @exception None
 * 
 * Example:
 * struct sleeplock sl;
 * initsleeplock(&sl, "my_sleep_lock");
 */
void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

/**
* This method acquires a sleep lock for the specified sleeplock <paramref name="lk"/>.
* It ensures that the lock is acquired before proceeding and puts the current process to sleep if the lock is already locked.
* Once the lock is acquired, it sets the locked flag to 1 and assigns the process ID of the current process to the sleeplock.
* Finally, it releases the lock.
*
* @param lk The sleeplock structure for which the lock needs to be acquired.
* @throws None
*
* Example:
* struct sleeplock mylock;
* acquiresleep(&mylock);
*/
void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

/**
* This method releases the sleep lock <paramref name="lk"/> by setting it as unlocked and waking up any threads waiting on it.
* 
* @param lk The sleep lock to be released.
* @throws None
* 
* Example:
* struct sleeplock mylock;
* releasesleep(&mylock);
*/
void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lk);
}

/**
* This method checks if the calling process holds the sleep lock <paramref name="lk"/>.
* It acquires the lock, checks if it is locked and if the process ID matches the calling process ID, then releases the lock.
* Returns 1 if the lock is held by the calling process, otherwise returns 0.
*
* @param lk The sleep lock to be checked.
* @return Returns 1 if the lock is held by the calling process, otherwise returns 0.
* @exception None
*
* Example:
* struct sleeplock sl;
* int result = holdingsleep(&sl);
*/
int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}



