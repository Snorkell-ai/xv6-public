#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

/**
* This method creates a new child process by calling the fork system call.
* 
* @return Returns the process ID of the child process to the parent process, and 0 to the child process.
* 
* @exception Returns -1 if the fork system call fails to create a new process.
* 
* @example
* int pid = sys_fork();
* if (pid < 0) {
*     // Handle error
* } else if (pid == 0) {
*     // Code for child process
* } else {
*     // Code for parent process
* }
*/
int
sys_fork(void)
{
  return fork();
}

/**
* This method is used to exit the system.
* 
* @exception This method does not throw any exceptions.
* 
* @example
* sys_exit();
*/
int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

/**
* This method calls the wait() system call to wait for a child process to terminate.
* 
* @return Returns the process ID of the terminated child process.
* @exception If an error occurs during the wait operation, an error code may be returned.
* 
* Example:
* int status = sys_wait();
*/
int
sys_wait(void)
{
  return wait();
}

/**
* This method sends a signal to the process with the specified process ID.
* 
* @exception Returns -1 if the process ID is invalid or an error occurs during the signal sending process.
* 
* Example:
* int pid = 1234;
* int result = sys_kill(pid);
*/
int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

/**
* This method returns the process ID of the current process.
*
* @return The process ID of the current process.
*
* @exception None
*
* @example
* <code>
* int pid = sys_getpid();
* printf("Process ID: %d\n", pid);
* </code>
*/
int
sys_getpid(void)
{
  return myproc()->pid;
}

/**
* This method increases the program's data space by the specified amount.
* It takes an integer argument <paramref name="n"/> representing the amount by which to increase the data space.
* If the argument retrieval fails, it returns -1.
* It then calculates the new address by adding the specified amount to the current process size.
* If the process growth fails, it returns -1.
* Returns the new address after successfully increasing the data space.
* 
* @exception Returns -1 if argument retrieval or process growth fails.
* 
* @example
* int newAddress = sys_sbrk(10);
* // Increases the data space by 10 units and returns the new address.
*/
int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

/**
* This method suspends the calling process for a specified number of ticks.
* 
* @return 0 on success, -1 on failure
* @exception If the process is killed while sleeping, it will return -1
* 
* Example:
* 
* int main() {
*     int result = sys_sleep(5);
*     if(result == 0) {
*         printf("Process slept successfully for 5 ticks.\n");
*     } else {
*         printf("Process sleep failed.\n");
*     }
*     return 0;
* }
*/
int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

/**
* This method returns the system uptime in ticks.
* It acquires the tickslock to get the current ticks value, then releases the tickslock.
* 
* @return The system uptime in ticks.
* @exception None
* 
* Example:
* int uptime = sys_uptime();
*/
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
