//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
/**
* This method retrieves the file descriptor and file pointer associated with the given file descriptor index.
* 
* @param n The index of the file descriptor to retrieve.
* @param pfd Pointer to an integer where the file descriptor will be stored.
* @param pf Pointer to a struct file pointer where the file will be stored.
* @return 0 if successful, -1 if an error occurs.
* @throws If the file descriptor is out of bounds or the file associated with the descriptor is not found, it returns -1.
* 
* Example:
* int fd;
* struct file *f;
* if(argfd(3, &fd, &f) == 0) {
*     // Successfully retrieved file descriptor and file pointer
* } else {
*     // Error handling
* }
*/
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

/**
* This method allocates a file descriptor for the given file <paramref name="f"/> in the current process.
* It searches for an available file descriptor in the process's file descriptor table and assigns the given file to it.
* If a free file descriptor is found, it returns the assigned file descriptor. Otherwise, it returns -1.
*
* @param f The file for which a file descriptor needs to be allocated.
* @return The allocated file descriptor if successful, -1 if no free file descriptor is available.
* @exception None
*
* Example:
* struct file *newFile = createNewFile();
* int fd = fdalloc(newFile);
* if(fd != -1) {
*     printf("File descriptor %d allocated successfully.\n", fd);
* } else {
*     printf("Failed to allocate file descriptor.\n");
* }
*/
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

/**
* This method duplicates a file descriptor.
* It takes no parameters.
* It returns the duplicated file descriptor on success, or -1 on failure.
* 
* Exceptions:
* - Returns -1 if the file descriptor argument is invalid or if allocation fails.
* 
* Example:
* int new_fd = sys_dup();
*/
int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

/**
* This method reads data from a file specified by the file descriptor and stores it in the provided buffer.
* 
* @return Returns the number of bytes read on success, or -1 if an error occurs.
* 
* @exception If the file descriptor is invalid, or if there is an issue with reading from the file, -1 is returned.
* 
* @example
* <code>
* int bytes_read = sys_read();
* if (bytes_read == -1) {
*     printf("Error reading from file.\n");
* } else {
*     printf("Successfully read %d bytes from file.\n", bytes_read);
* }
* </code>
*/
int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

/**
* This method performs a write operation to a file.
* It takes the file descriptor, buffer pointer, and buffer size as arguments.
* If any of the arguments are invalid, it returns -1.
* 
* Exceptions:
* - Returns -1 if the file descriptor is invalid.
* - Returns -1 if the buffer size is negative.
* - Returns -1 if the buffer pointer is invalid.
* 
* Example:
* To write data to a file:
* int fd = open("example.txt", O_WRONLY);
* char buffer[100] = "Hello, World!";
* int bytes_written = sys_write(fd, buffer, strlen(buffer));
* close(fd);
*/
int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

/**
* This method closes a file descriptor in the operating system.
* It takes no parameters and returns an integer value.
* If successful, it returns 0. If an error occurs, it returns -1.
* This method accesses the file descriptor from the current process's file descriptor table and closes the corresponding file.
* It does not take any input arguments.
* This method does not throw any exceptions.
*
* Example:
* sys_close();
*/
int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

/**
* This method retrieves file status information for the specified file descriptor.
* It takes no parameters and returns an integer value.
* If successful, it returns 0; otherwise, it returns -1.
* This method internally calls functions argfd() and argptr() to retrieve file and stat information.
* If any errors occur during the process, it returns -1.
* Exceptions: This method may return -1 if there are errors in retrieving file or stat information.
* Example:
* int result = sys_fstat();
*/
int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

/**
* This method creates a hard link between two files specified by the paths <paramref name="old"/> and <paramref name="new"/>.
* If successful, it increments the link count of the target file and updates its metadata.
* If unsuccessful, it returns -1.
*
* Exceptions:
* - Returns -1 if unable to retrieve the paths of the old or new files.
* - Returns -1 if unable to find the target file specified by the old path.
* - Returns -1 if the target file is a directory.
* - Returns -1 if unable to create the link in the parent directory.
*
* Example:
* sys_link("/path/to/oldfile", "/path/to/newfile");
*/
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

/**
* This method checks if the directory <paramref name="dp"/> is empty by iterating through its entries.
* It reads each entry using the readi function and checks if the inode number is 0.
* If any entry has a non-zero inode number, the method returns 0 indicating the directory is not empty.
* If all entries have inode number 0, the method returns 1 indicating the directory is empty.
* @param dp The directory inode to check for emptiness.
* @return 1 if the directory is empty, 0 otherwise.
* @exception panic("isdirempty: readi") if there is an error reading the directory entries.
*
* Example:
* struct inode *directory_inode;
* int result = isdirempty(directory_inode);
* if (result == 1) {
*   printf("Directory is empty");
* } else {
*   printf("Directory is not empty");
* }
*/
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

/**
* This method is used to unlink a file or directory specified by the path provided.
* It checks if the path is valid and if the file/directory can be unlinked.
* If successful, it unlinks the file/directory and updates the necessary information.
* If unsuccessful, it returns -1.
*
* Exceptions:
* - Returns -1 if the path is invalid or if the file/directory cannot be unlinked.
*
* Example:
* int result = sys_unlink();
* if(result == 0) {
*     printf("File/Directory unlinked successfully.\n");
* } else {
*     printf("Error unlinking file/directory.\n");
* }
*/
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

/**            
* This method creates a new inode with the specified path, type, major, and minor attributes.            
*            
* @param path The path of the new inode to be created.            
* @param type The type of the new inode (e.g., T_FILE or T_DIR).            
* @param major The major attribute of the new inode.            
* @param minor The minor attribute of the new inode.            
* @return A pointer to the newly created inode if successful, otherwise NULL.            
* @throws If an error occurs during inode creation or directory linking, a panic exception is raised.            
*            
* Example:            
* struct inode *newInode = create("/newfile.txt", T_FILE, 1, 0);            
*/
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

/**
* This method opens a file specified by the path with the given open mode.
* It creates a new file if O_CREATE flag is set in the open mode.
* It returns a file descriptor on success, or -1 on failure.
* 
* @return int - Returns a file descriptor on success, or -1 on failure.
* @throws - This method may throw exceptions if arguments are invalid or file operations fail.
* 
* Example:
* int fd = sys_open();
* if(fd != -1) {
*     // File opened successfully, perform operations
* } else {
*     // Handle error
* }
*/
int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

/**
* This method creates a new directory at the specified path.
* 
* @throws -1 if the directory creation fails
* 
* Example:
* 
* int result = sys_mkdir();
* if(result == -1) {
*     printf("Failed to create directory\n");
* }
*/
int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

/**
* This method creates a new special file node in the file system.
* It takes three arguments: the path of the file, the major number, and the minor number.
* If successful, it returns 0; otherwise, it returns -1.
* 
* Exceptions:
* - If the path argument is invalid or cannot be accessed, it returns -1.
* - If either the major or minor number arguments are invalid, it returns -1.
* - If the creation of the special file node fails, it returns -1.
* 
* Example:
* sys_mknod("/dev/mydevice", 1, 0);
*/
int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

/**
* This method changes the current working directory to the specified path.
* It takes no parameters and returns an integer value.
* 
* Exceptions:
* - Returns -1 if there is an error in changing the directory.
* 
* Example:
* sys_chdir();
*/
int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

/**
* This method executes a system call with the specified path and arguments.
* 
* @throws -1 if unable to retrieve path or arguments
* 
* Example:
* sys_exec("/bin/program", ["/bin/program", "arg1", "arg2"])
*/
int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

/**
* This method creates a pipe, which is a communication mechanism that allows the flow of data between two processes. 
* It takes no parameters and returns an integer value.
* 
* Exceptions:
* - Returns -1 if there is an error in obtaining the file descriptors or allocating pipes.
* 
* Example:
* int fd[2];
* int result = sys_pipe(fd);
* if(result == 0) {
*     // Pipe creation successful
*     printf("Pipe created with file descriptors: %d, %d\n", fd[0], fd[1]);
* } else {
*     // Error handling
*     printf("Error creating pipe\n");
* }
*/
int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}
