#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

/**            
* This method fetches an integer value from the specified memory address <paramref name="addr"/> and stores it in the memory location pointed to by <paramref name="ip"/>.            
* It retrieves the integer value stored at the memory address <paramref name="addr"/> and assigns it to the integer pointer <paramref name="ip"/>.            
* If the specified memory address is out of bounds or if there is an issue with memory access, the method returns -1.            
* 
* @param addr The memory address from which to fetch the integer value.            
* @param ip A pointer to an integer where the fetched value will be stored.            
* @return 0 if the integer value is successfully fetched and stored, -1 if there is an error.            
* @exception This method may return -1 if the specified memory address is out of bounds or if there is a memory access issue.            
* 
* Example:            
* int val;            
* int result = fetchint(0x1000, &val); // Fetches the integer value at memory address 0x1000 and stores it in 'val'.            
*/
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

/**
* This method fetches a string from the specified memory address.
* It takes the memory address <paramref name="addr"/> and a pointer to a character pointer <paramref name="pp"/>.
* It checks if the memory address is within the current process's size limit.
* If the address is valid, it assigns the memory address to the character pointer.
* It then iterates through the memory block to find the end of the string (marked by a null character).
* If a null character is found, it returns the length of the string.
* If no null character is found, it returns -1 indicating an error.
* 
* @param addr The memory address from which to fetch the string.
* @param pp Pointer to a character pointer where the fetched string will be stored.
* @return The length of the fetched string if successful, -1 if an error occurs.
* @throws None
*
* Example:
* <code>
* char *str;
* int result = fetchstr(0x12345678, &str);
* if(result != -1) {
*     printf("Fetched string: %s\n", str);
* } else {
*     printf("Error fetching string\n");
* }
* </code>
*/
int
fetchstr(uint addr, char **pp)
{
  char *s, *ep;
  struct proc *curproc = myproc();

  if(addr >= curproc->sz)
    return -1;
  *pp = (char*)addr;
  ep = (char*)curproc->sz;
  for(s = *pp; s < ep; s++){
    if(*s == 0)
      return s - *pp;
  }
  return -1;
}

/**
* This method retrieves an integer argument from the specified index in the stack frame.
* It calculates the memory address of the argument based on the current process's stack pointer and the argument index.
* 
* @param n The index of the integer argument to retrieve.
* @param ip Pointer to store the retrieved integer value.
* @return Returns the retrieved integer value.
* @throws Exception if the process or stack pointer is invalid.
* 
* Example:
* int val;
* int index = 2;
* int result = argint(index, &val);
* // After this call, 'val' will contain the integer argument at index 2 in the stack frame.
*/
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

/**
* This method retrieves the argument at index n from the current process's memory space and stores it in the provided pointer pp.
* 
* @param n The index of the argument to retrieve.
* @param pp Pointer to store the retrieved argument.
* @param size Size of the argument to retrieve.
* @return 0 if successful, -1 if an error occurs.
* @throws -1 if argint function fails to retrieve the argument at index n.
* @throws -1 if size is negative or if the argument is out of bounds in the current process's memory space.
* 
* Example:
* int argIndex = 2;
* char *argValue;
* int argSize = 4;
* int result = argptr(argIndex, &argValue, argSize);
* if(result == 0) {
*     printf("Argument at index %d: %s\n", argIndex, argValue);
* } else {
*     printf("Error retrieving argument at index %d\n", argIndex);
* }
*/
int
argptr(int n, char **pp, int size)
{
  int i;
  struct proc *curproc = myproc();
 
  if(argint(n, &i) < 0)
    return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
    return -1;
  *pp = (char*)i;
  return 0;
}

/**
* This method takes an integer <paramref name="n"/> and a pointer to a character array <paramref name="pp"/>.
* It calls the argint function with n and the address of addr, and if the return value is less than 0, it returns -1.
* Otherwise, it calls the fetchstr function with the address addr and the pointer pp.
* 
* @param n The integer value to be passed as input.
* @param pp The pointer to a character array where the result will be stored.
* 
* @return Returns -1 if argint function returns a value less than 0, otherwise returns the result of fetchstr function.
* 
* @exception This method may throw exceptions if argint or fetchstr functions encounter errors during execution.
* 
* Example:
* int n = 5;
* char *pp;
* int result = argstr(n, &pp);
*/
int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

/**
* This method changes the current working directory to the one specified by the user.
* 
* @exception If the specified directory does not exist or is inaccessible, an error may be thrown.
* 
* Example:
* 
* int result = sys_chdir();
* if(result == 0) {
*     printf("Directory changed successfully.\n");
* } else {
*     printf("Error changing directory.\n");
* }
*/
extern int sys_chdir(void);
/**
* This method closes a file descriptor associated with the current process.
* 
* @exception This method may throw an exception if there is an error while closing the file descriptor.
* 
* @example
* <code>
* int file_descriptor = 5;
* int result = sys_close(file_descriptor);
* if(result == -1) {
*     printf("Error closing file descriptor\n");
* }
* </code>
*/
extern int sys_close(void);
/**
* This method duplicates an open file descriptor. It returns a new file descriptor that refers to the same open file description as the original file descriptor.
* 
* @return Returns the new file descriptor on success, or -1 on failure.
* 
* @exception This method may fail if the system call to duplicate the file descriptor encounters an error.
* 
* @example
* <code>
* int new_fd = sys_dup();
* if (new_fd == -1) {
*     printf("Error duplicating file descriptor\n");
* } else {
*     printf("File descriptor duplicated successfully: %d\n", new_fd);
* }
* </code>
*/
extern int sys_dup(void);
/**
* This method executes a system command specified by the user.
* 
* @exception If the system command execution fails, an error message will be displayed.
* 
* Example:
* 
* int result = sys_exec();
* if(result == 0) {
*     printf("System command executed successfully.\n");
* } else {
*     printf("Error executing system command.\n");
* }
*/
extern int sys_exec(void);
/**
* This method is used to exit the system.
* 
* @exception This method does not throw any exceptions.
* 
* @example
* <code>
* int result = sys_exit();
* </code>
*/
extern int sys_exit(void);
/**
* This method creates a new child process that duplicates the calling process. 
* It returns the process ID of the child process to the parent process and 0 to the child process.
* If an error occurs, it returns -1.
*
* @return Returns the process ID of the child process to the parent process and 0 to the child process. Returns -1 if an error occurs.
*
* @throws Exception if the system call to fork fails or if there are no available resources to create a new process.
*
* @example
* <code>
* int child_pid = sys_fork();
* if (child_pid == 0) {
*     // Code for child process
* } else if (child_pid > 0) {
*     // Code for parent process
* } else {
*     // Error handling
* }
* </code>
*/
extern int sys_fork(void);
/**
* This method calls the system function sys_fstat to retrieve information about a specified file.
* It does not take any input parameters and returns an integer value.
* If the function encounters an error while retrieving file information, it may return a negative value.
*
* Example:
* int fileStat = sys_fstat();
*/
extern int sys_fstat(void);
/**
* This method retrieves the process ID of the current process.
* 
* @return The process ID of the current process.
* @exception None
* 
* Example:
* 
* int pid = sys_getpid();
*/
extern int sys_getpid(void);
/**
* This method is used to send a signal to a process specified by the process ID.
* It is important to note that this function does not have any parameters or return values.
* If the specified process ID does not exist or the calling process does not have permission to send the signal, an exception may be raised.
*
* Example:
* sys_kill();
*/
extern int sys_kill(void);
/**
* This method creates a new link to an existing file or directory.
* It returns an integer value indicating the success or failure of the operation.
* 
* @exception If the link operation fails, an error code is returned.
* 
* @example
* <code>
* int result = sys_link();
* if(result == 0) {
*     printf("Link created successfully.\n");
* } else {
*     printf("Error creating link. Error code: %d\n", result);
* }
* </code>
*/
extern int sys_link(void);
/**
* This method creates a new directory in the file system.
* 
* @return Returns an integer value indicating the success or failure of creating the directory.
* @exception If the directory creation fails, an exception may be thrown.
* 
* Example:
* 
* int result = sys_mkdir();
* if(result == 0) {
*     printf("Directory created successfully.\n");
* } else {
*     printf("Failed to create directory.\n");
* }
*/
extern int sys_mkdir(void);
/**
* This method creates a new special file.
* It is used to create a new device special file or FIFO special file.
* This method may throw exceptions if the user does not have the necessary permissions to create the special file.
*
* Example:
* int result = sys_mknod();
*/
extern int sys_mknod(void);
/**
* This method opens a system file for reading or writing.
* 
* @exception If the system file cannot be opened, an error code is returned.
* 
* @example
* <code>
* int file_descriptor = sys_open();
* if (file_descriptor == -1) {
*     printf("Error opening system file\n");
* }
* </code>
*/
extern int sys_open(void);
/**
* This method creates a new pipe and returns two file descriptors, one for reading and one for writing.
* 
* @return Returns 0 on success, -1 on failure.
* 
* @exception This method may fail if there are no available file descriptors or if the system call fails.
* 
* @example
* <code>
* int pipe_fd[2];
* if (sys_pipe(pipe_fd) == -1) {
*     perror("Pipe creation failed");
*     exit(EXIT_FAILURE);
* }
* </code>
*/
extern int sys_pipe(void);
/**
* This method reads data from the system input and returns an integer value.
* 
* @return An integer value read from the system input.
* 
* @exception This method may throw exceptions if there are errors in reading from the system input.
* 
* @example
* <code>
* int data = sys_read();
* printf("Data read from system input: %d\n", data);
* </code>
*/
extern int sys_read(void);
/**
* This method is used to request additional memory from the operating system.
* It returns the address of the new block of memory or -1 if an error occurs.
* 
* @return int - The address of the new block of memory or -1 if an error occurs.
* 
* @exception None
* 
* @example
* int new_memory_address = sys_sbrk();
*/
extern int sys_sbrk(void);
/**
* This method suspends the execution of the current thread for a specified amount of time.
* 
* @return Returns 0 on success, -1 on failure.
* 
* @exception This method may throw exceptions if there are issues with suspending the thread.
* 
* @example
* <code>
* int result = sys_sleep();
* if(result == 0) {
*     printf("Thread suspended successfully.\n");
* } else {
*     printf("Failed to suspend thread.\n");
* }
* </code>
*/
extern int sys_sleep(void);
/**
* This method is used to delete a file specified by the path.
* It calls the system function sys_unlink() to delete the file.
* 
* @exception This method may throw an exception if the file specified by the path cannot be deleted.
* 
* @example
* <code>
* int result = sys_unlink();
* if(result == 0) {
*     printf("File deleted successfully.\n");
* } else {
*     printf("Error deleting file.\n");
* }
* </code>
*/
extern int sys_unlink(void);
/**
* This method suspends the execution of the calling process until one of its child processes exits or a signal is received.
* 
* @return Returns the process ID of the terminated child process, or -1 if an error occurs.
* 
* @exception If an error occurs during the waiting process, this method may return -1 and set the errno variable to indicate the error.
* 
* @example
* <code>
* int child_status;
* int child_pid = sys_wait(&child_status);
* if(child_pid == -1) {
*     perror("Error while waiting for child process");
* } else {
*     printf("Child process with PID %d has terminated with status %d\n", child_pid, child_status);
* }
* </code>
*/
extern int sys_wait(void);
/**
* This method is used to perform a system call to write data.
* It does not take any parameters and returns an integer value.
* This method may throw exceptions if there are issues with the system call.
* 
* Example:
* int result = sys_write();
* // This will perform a system call to write data and store the result in 'result'.
*/
extern int sys_write(void);
/**
* This method returns the system uptime in seconds.
* 
* @return An integer value representing the system uptime in seconds.
* 
* @exception None
* 
* @example
* <code>
* int uptime = sys_uptime();
* printf("System uptime: %d seconds\n", uptime);
* </code>
*/
extern int sys_uptime(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

/**
* This method executes a system call based on the value stored in the register of the current process.
* It retrieves the system call number from the current process's register, checks if it is within a valid range,
* and then executes the corresponding system call function. If the system call number is not recognized,
* it prints an error message indicating an unknown system call.
* Exceptions: If the system call number is out of range or if the corresponding system call function is not defined,
* the method sets the return value in the register of the current process to -1.
* Example:
* syscall();
*/
void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
