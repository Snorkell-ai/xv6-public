#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
/**
* This method defines the external function trapret, which is used to handle exceptions and return from a trap or interrupt service routine.
* 
* @exception None
* 
* @example
* trapret();
*/
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  /**
  * This method initializes the process table by calling the function initlock with the process table lock and a string identifier "ptable".
  * 
  * @throws None
  * 
  * Example:
  * pinit();
  */
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
  /**
  * This method returns the difference between the result of the function mycpu() and the variable cpus.
  * 
  * @return The difference between mycpu() and cpus.
  * 
  * @exception None
  * 
  * @example
  * int result = cpuid();
  * // result will contain the difference between the result of mycpu() and cpus
  */
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
/**
* This method returns a pointer to the CPU struct representing the current CPU.
* It checks if interrupts are enabled and panics if they are.
* It then retrieves the local APIC ID and searches for a matching CPU struct in the cpus array.
* If a matching CPU struct is found, it returns a pointer to it.
* If no matching CPU struct is found, it panics with an "unknown apicid" message.
* @return Pointer to the CPU struct representing the current CPU.
* @exception Panics if interrupts are enabled or if the APIC ID is unknown.
* Example:
* struct cpu* current_cpu = mycpu();
*/
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
/**
* This method returns the process structure pointer of the current running process.
* It pushes and pops the interrupt flag to ensure atomicity.
* 
* @return struct proc* - Pointer to the process structure of the current running process.
* 
* @exception None
* 
* @example
* struct proc* current_process = myproc();
*/

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  /**
  * This method allocates a new process in the system.
  * It searches for an unused process in the process table and initializes it.
  * If successful, it returns a pointer to the newly allocated process.
  * If unsuccessful, it returns 0.
  *
  * @return Pointer to the newly allocated process if successful, otherwise 0.
  *
  * @exception If kernel stack allocation fails, the process state is set to UNUSED and 0 is returned.
  *
  * Example:
  * struct proc* newProcess = allocproc();
  * if (newProcess != 0) {
  *     // Process successfully allocated
  * } else {
  *     // Allocation failed
  * }
  */
  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
/**
* This method initializes a new user process by allocating memory for the process, setting up the page directory, and initializing the user environment.
* It loads the initial user code into memory and sets up the process control block with necessary information.
* 
* @exception panic("userinit: out of memory?") - This exception is thrown if there is not enough memory available during the initialization process.
* 
* Example:
* userinit();
*/

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

/**
* This method increases the size of the process memory for the current process by <paramref name="n"/> bytes.
* If <paramref name="n"/> is positive, it allocates additional memory. If <paramref name="n"/> is negative, it deallocates memory.
* 
* @param n The number of bytes by which to grow or shrink the process memory.
* @return 0 if the memory allocation or deallocation was successful, -1 if an error occurred.
* @throws -1 if the memory allocation or deallocation fails.
* 
* Example:
* 
* int result = growproc(100);
* if(result == 0) {
*     printf("Memory allocation successful.\n");
* } else {
*     printf("Memory allocation failed.\n");
* }
*/
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  /**
  * This method creates a new process by forking the current process. It allocates a new process, copies the process state from the current process, and initializes the new process with the necessary attributes. 
  * If successful, it returns the process ID of the newly created process.
  * If an error occurs during the process creation, it returns -1.
  * 
  * @return The process ID of the newly created process if successful, -1 if an error occurs.
  * @exception Returns -1 if process allocation fails or if copying the process state fails.
  * 
  * Example:
  * int newProcessID = fork();
  */
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  /**
  * This method handles the process of exiting a process in the operating system. It closes all open files associated with the current process, releases resources, and handles parent-child relationships.
  * Exceptions: This method may panic if the current process is the init process or encounters an error during the exit process.
  * Example:
  * exit();
  */
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        /**
        * This method waits for any child processes of the current process to exit. 
        * It scans through the process table to find exited children and handles their cleanup.
        * If a child process has exited, it releases resources associated with the child process and returns its PID.
        * If no children have exited or the current process has been killed, it returns -1.
        * This method utilizes the ptable data structure and synchronization mechanisms for process management.
        * @return Returns the PID of the exited child process, or -1 if no child processes have exited or if the current process has been killed.
        * @throws None
        * Example:
        * int pid = wait();
        */
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
      /**
      * This method represents a scheduler that runs processes in a loop based on their state.
      * It iterates over the process table to find a runnable process and switches to it.
      * The chosen process is then switched to user mode, marked as running, and its context is switched.
      * After the process is done running, it releases the process table lock.
      * Exceptions: None
      * Example:
      * scheduler();
      */
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
/**
* This method performs the scheduling of processes in the system.
* It ensures that the current process is properly switched out and the next process is scheduled to run.
* This method should only be called when holding the process table lock and with interrupts disabled.
* 
* Exceptions:
* - If the process table lock is not held, it will panic with the message "sched ptable.lock".
* - If the number of CPU interrupts is not 1, it will panic with the message "sched locks".
* - If the current process state is already set to RUNNING, it will panic with the message "sched running".
* - If interrupts are enabled, it will panic with the message "sched interruptible".
* 
* Example:
* sched();
*/
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

/**
* This method releases the ptable.lock and performs initialization functions if it is the first time being called.
* It is important to note that some initialization functions must be run in the context of a regular process.
* This method does not return any value.
* 
* @exception None
* 
* Example:
* forkret();
*/
void
/**
* This method yields the CPU to other processes by marking the current process as RUNNABLE and calling the scheduler. 
* It acquires the process table lock before marking the process as RUNNABLE and releases the lock after calling the scheduler.
* @exception This method does not throw any exceptions.
* @example
* yield();
*/
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  /**
  * This method puts the current process to sleep and changes its state to SLEEPING. It requires a channel and a spinlock as parameters.
  * If the current process is not found, it will panic with the message "sleep".
  * If the spinlock is not provided, it will panic with the message "sleep without lk".
  * It acquires the ptable.lock to change the process state and then calls sched. It ensures that no wakeup is missed while holding the ptable.lock.
  * 
  * @param chan The channel to put the process to sleep.
  * @param lk The spinlock to be released while acquiring ptable.lock.
  * @exception panic If the current process is not found or if the spinlock is not provided.
  * 
  * Example:
  * sleep(&chan, &lk);
  */
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

/**
* This method wakes up a process that is in a sleeping state and waiting on a specific channel.
* It iterates through the process table and checks for processes that are in the SLEEPING state and waiting on the provided channel.
* If such a process is found, it changes its state to RUNNABLE, allowing it to be scheduled for execution.
* 
* @param chan A pointer to the channel on which the process is waiting.
* @throws None
* 
* Example:
* 
* // Define a channel
* void *channel = (void*)0x12345678;
* 
* // Call the wakeup1 method to wake up processes waiting on the specified channel
* wakeup1(channel);
*/
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

/**
* This method wakes up a thread waiting on the specified channel.
* 
* @param chan A pointer to the channel on which the thread is waiting.
* @throws None
* 
* Example:
* 
* void *channel = (void*)0x12345678;
* wakeup(channel);
*/
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

/**
* This method attempts to terminate the process with the specified process ID <paramref name="pid"/>.
* If the process is found, it sets the 'killed' flag to 1 and changes the state of the process from SLEEPING to RUNNABLE if necessary.
* If the process is successfully terminated, it returns 0. If the process is not found, it returns -1.
* Exceptions: This method may throw exceptions if there are issues with acquiring or releasing the process table lock.
* Example:
* int result = kill(123); // Terminates the process with PID 123
*/
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

/**
* This method prints the process information for each process in the system.
* It displays the process ID, state, and name of each process.
* If the process is in the SLEEPING state, it also prints the program counters.
* Exceptions: None
* Example:
* procdump();
*/
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
