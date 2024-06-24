// Create a zombie process that
// must be reparented at exit.

#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method creates a child process using the fork() system call and waits for the child process to exit before the parent process exits.
* 
* @throws None
* 
* Example:
* 
* int main(void)
* {
*   if(fork() > 0)
*     sleep(5);  // Let child exit before parent.
*   exit();
* }
*/
int
main(void)
{
  if(fork() > 0)
    sleep(5);  // Let child exit before parent.
  exit();
}
