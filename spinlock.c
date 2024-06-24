// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

/**
* Initializes a spinlock with the specified name.
* 
* @param lk Pointer to the spinlock structure to be initialized.
* @param name Name to assign to the spinlock.
* @throws None
* @example
* struct spinlock mylock;
* initlock(&mylock, "my_spinlock");
*/
void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

/**
* This method acquires a spinlock to ensure mutual exclusion in a multi-threaded environment.
* It disables interrupts to avoid deadlock and checks if the lock is already held to prevent double acquisition.
* The xchg operation is used to atomically set the lock to 1.
* Memory barriers are used to ensure memory ordering and prevent reordering of critical section instructions.
* Debugging information is recorded for lock acquisition.
* 
* @param lk Pointer to the spinlock structure to be acquired.
* @exception panic If the lock is already held, a panic is triggered.
* 
* Example:
* struct spinlock mylock;
* acquire(&mylock);
*/
void
acquire(struct spinlock *lk)
{
  pushcli(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // The xchg is atomic.
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

/**
* This method releases the lock held by the current thread.
* It ensures that all the stores in the critical section are visible to other cores before the lock is released.
* This method should only be called if the lock is currently being held by the thread.
* 
* @param lk A pointer to the spinlock structure representing the lock to be released.
* @throws panic if the lock is not currently held by the thread.
* 
* Example:
* struct spinlock myLock;
* acquire(&myLock);
* // Critical section
* release(&myLock);
*/
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code can't use a C assignment, since it might
  // not be atomic. A real OS would use C atomics here.
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  popcli();
}

/**
* This method retrieves the program counters (pcs) of the calling functions from the provided stack frame pointer.
* It takes a void pointer <paramref name="v"/> and an array of unsigned integers <paramref name="pcs"/> as input.
* The function iterates through the stack frame pointers to extract the saved %eip values and saved %ebp values.
* If certain conditions are met, the iteration stops to avoid accessing invalid memory locations.
* If less than 10 pcs are found, the remaining elements in the pcs array are set to 0.
* Note: This function assumes a specific stack frame structure and may not work in all environments.
* @param v A void pointer pointing to the stack frame pointer.
* @param pcs An array of unsigned integers to store the retrieved program counters.
* @throws None
* Example:
* uint pcs[10];
* getcallerpcs(&v, pcs);
*/
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // saved %eip
    ebp = (uint*)ebp[0]; // saved %ebp
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

/**
* This method checks if the specified spinlock <paramref name="lock"/> is currently being held.
* It temporarily disables interrupts, checks if the lock is locked and if it is held by the current CPU.
* It then re-enables interrupts and returns a boolean value indicating whether the lock is held or not.
* 
* @param lock A pointer to the spinlock structure to be checked.
* @return An integer value representing whether the lock is held (1) or not held (0).
* @exception None
* 
* Example:
* struct spinlock my_lock;
* int isHeld = holding(&my_lock);
*/
int
holding(struct spinlock *lock)
{
  int r;
  pushcli();
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}



/**
* This method temporarily disables interrupts by pushing the current interrupt state onto the stack.
* It first reads the current interrupt flags and then disables interrupts using the cli() function.
* If the current CPU's interrupt count is 0, it saves the interrupt enable flag in the CPU structure.
* Finally, it increments the interrupt count for the CPU.
* This method is used to prevent interrupts from occurring during critical sections of code execution.
* @exception None
* Example:
* pushcli();
*/
void
pushcli(void)
{
  int eflags;

  eflags = readeflags();
  cli();
  if(mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

/**
* This method disables interrupts by decrementing the number of interrupts being held by the current CPU. If interrupts are enabled and the decrement causes the count to go below 0, a panic is triggered. If the interrupt count reaches 0 and interrupts were previously enabled, interrupts are re-enabled.
* 
* @exception panic("popcli - interruptible") if interrupts are enabled
* @exception panic("popcli") if the interrupt count goes below 0
* 
* Example:
* popcli();
*/
void
popcli(void)
{
  if(readeflags()&FL_IF)
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)
    sti();
}

