//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

/**
* This method initializes the file system table by calling the function initlock with the file system table lock and name "ftable".
* 
* @throws None
* 
* Example:
* fileinit();
*/
void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

/**
* This method allocates a file structure from the file table.
* It acquires the lock on the file table, iterates through the file table to find an available file structure with reference count 0,
* sets the reference count to 1 for the found file structure, releases the lock, and returns the file structure.
* If no available file structure is found, it releases the lock and returns 0.
* 
* @return struct file* - Pointer to the allocated file structure, or 0 if no available file structure is found.
* 
* @exception - This method does not throw any exceptions.
* 
* Example:
* struct file* newFile = filealloc();
*/
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

/**
* This method duplicates the file structure <paramref name="f"/> by incrementing its reference count.
* It acquires the ftable lock before performing the operation to ensure thread safety.
* If the reference count of the input file is less than 1, it panics with the message "filedup".
* After incrementing the reference count, it releases the ftable lock.
* Returns the duplicated file structure.
*
* @param f The file structure to be duplicated.
* @return The duplicated file structure.
* @throws panic if the reference count of the input file is less than 1.
*
* Example:
* struct file *newFile = filedup(existingFile);
*/
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

/**
* This method closes the file pointed to by the input file structure <paramref name="f"/>.
* It decrements the reference count of the file and releases the file table lock if the reference count is greater than 0.
* If the reference count becomes 0, it updates the file structure, sets the reference count to 0, and releases the file table lock.
* If the file type is a pipe, it closes the pipe. If the file type is an inode, it puts the inode and ends the operation.
*
* @param f Pointer to the file structure to be closed.
* @throws panic if the reference count of the file is less than 1.
* @throws panic if an unsupported file type is encountered.
*
* Example:
* struct file myFile;
* // Initialize myFile
* fileclose(&myFile);
*/
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

/**
* This method retrieves the file status information for the specified file <paramref name="f"/> and stores it in the structure <paramref name="st"/>.
* If the file type is FD_INODE, it locks the inode associated with the file, retrieves the status information using stati function, and then unlocks the inode.
* Returns 0 upon successful retrieval of file status information, -1 otherwise.
* 
* @param f Pointer to the struct file representing the file for which status information is to be retrieved.
* @param st Pointer to the struct stat where the file status information will be stored.
* @return Returns 0 if successful, -1 if unsuccessful.
* @exception If f or st is NULL, or if f->type is not FD_INODE, an exception may occur.
* 
* Example:
* struct file myFile;
* struct stat myStat;
* int result = filestat(&myFile, &myStat);
* if(result == 0){
*     // File status information retrieved successfully
* } else {
*     // Error occurred while retrieving file status information
* }
*/
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

/**
* This method reads data from a file pointed to by the struct file pointer <paramref name="f"/> and stores it in the memory location pointed to by <paramref name="addr"/>.
* The maximum number of bytes to read is specified by <paramref name="n"/>.
* 
* @param f Pointer to the struct file representing the file to be read from.
* @param addr Pointer to the memory location where the read data will be stored.
* @param n Maximum number of bytes to read.
* @return Returns the number of bytes successfully read, or -1 if the file is not readable or an error occurs.
* @throws panic("fileread") if an unexpected condition occurs during the file reading process.
* 
* Example:
* <code>
* struct file *file_ptr;
* char buffer[100];
* int bytes_read = fileread(file_ptr, buffer, 100);
* if (bytes_read == -1) {
*     printf("Error reading file.\n");
* } else {
*     printf("Successfully read %d bytes from the file.\n", bytes_read);
* }
* </code>
*/
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

/**
* This method writes data from the memory address <paramref name="addr"/> to the file <paramref name="f"/>.
* 
* @param f Pointer to the file structure where data will be written.
* @param addr Pointer to the memory address containing the data to be written.
* @param n Number of bytes to write.
* @return Returns the number of bytes successfully written, or -1 if an error occurs.
* @throws -1 if the file is not writable.
* @throws -1 if the file type is FD_PIPE.
* @throws -1 if an error occurs during writing to an inode file.
*
* Example:
* struct file *f;
* char *data = "Hello, World!";
* int bytes_written = filewrite(f, data, strlen(data));
*/
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

