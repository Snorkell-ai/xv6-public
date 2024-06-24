// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"

#define N  1000

/**
* This method prints the specified string <paramref name="s"/> to the file descriptor <paramref name="fd"/>.
* It uses the write system call to write the string to the file descriptor.
* 
* @param fd The file descriptor to write to.
* @param s The string to be printed.
* @param ... Additional arguments that can be used with the format specifier in the string.
* 
* @throws None
* 
* Example:
* 
* int fd = open("output.txt", O_WRONLY | O_CREAT, 0644);
* if (fd != -1) {
*     printf(fd, "Hello, World!\n");
*     close(fd);
* }
*/
void
printf(int fd, const char *s, ...)
{
  write(fd, s, strlen(s));
}

/**
* This method performs a fork test by creating child processes using the fork system call.
* It iterates N times, creating a child process each time, and then waits for all child processes to finish.
* If the fork system call fails or if the wait system call stops early, the method will exit with an error message.
* If the wait system call receives too many child processes, the method will exit with an error message.
* The method concludes by printing "fork test OK" if all processes were created and waited for successfully.
*
* @throws - This method does not throw any exceptions.
*
* Example:
* forktest();
*/
void
forktest(void)
{
  int n, pid;

  printf(1, "fork test\n");

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0)
      exit();
  }

  if(n == N){
    printf(1, "fork claimed to work N times!\n", N);
    exit();
  }

  for(; n > 0; n--){
    if(wait() < 0){
      printf(1, "wait stopped early\n");
      exit();
    }
  }

  if(wait() != -1){
    printf(1, "wait got too many\n");
    exit();
  }

  printf(1, "fork test OK\n");
}

/**
* This method executes the forktest function and then exits the program.
* 
* @throws None
* 
* Example:
* main();
*/
int
main(void)
{
  forktest();
  exit();
}
