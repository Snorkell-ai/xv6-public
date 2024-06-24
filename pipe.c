#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

/**
* This method allocates memory for a pipe and initializes the file pointers to point to the allocated pipe.
* If successful, it sets up the pipe for reading and writing operations.
* If any allocation fails, it cleans up and returns an error code.
*
* @param f0 Pointer to the first file structure to be initialized.
* @param f1 Pointer to the second file structure to be initialized.
* @return 0 on success, -1 on failure.
*
* @exception If memory allocation for the pipe or files fails, it returns -1 and cleans up any allocated resources.
*
* Example:
* struct file *file0, *file1;
* int result = pipealloc(&file0, &file1);
* if (result == 0) {
*     // Pipe allocation successful, use file0 and file1 for reading and writing operations
* } else {
*     // Error handling for failed pipe allocation
* }
*/
int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if((p = (struct pipe*)kalloc()) == 0)
    goto bad;
  p->readopen = 1;
  p->writeopen = 1;
  p->nwrite = 0;
  p->nread = 0;
  initlock(&p->lock, "pipe");
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;
  return 0;

//PAGEBREAK: 20
 bad:
  if(p)
    kfree((char*)p);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

/**
* This method closes the specified pipe <paramref name="p"/> based on the provided flag <paramref name="writable"/>.
* If <paramref name="writable"/> is true, it sets the writeopen flag of the pipe to 0 and wakes up any waiting readers.
* If <paramref name="writable"/> is false, it sets the readopen flag of the pipe to 0 and wakes up any waiting writers.
* If both readopen and writeopen flags are 0 after the operation, the pipe memory is deallocated.
* Exceptions: None
* Example:
* struct pipe myPipe;
* pipeclose(&myPipe, 1);
*/
void
pipeclose(struct pipe *p, int writable)
{
  acquire(&p->lock);
  if(writable){
    p->writeopen = 0;
    wakeup(&p->nread);
  } else {
    p->readopen = 0;
    wakeup(&p->nwrite);
  }
  if(p->readopen == 0 && p->writeopen == 0){
    release(&p->lock);
    kfree((char*)p);
  } else
    release(&p->lock);
}

/**
* This method writes data to a pipe structure <paramref name="p"/> from the provided address <paramref name="addr"/> with a specified length <paramref name="n"/>.
* It acquires the lock of the pipe, then iterates through the data to be written.
* If the pipe is full, it waits until space is available or until the read end of the pipe is closed or the process is killed.
* Once space is available, it writes data to the pipe and updates the write index.
* It wakes up any processes waiting to read from the pipe and releases the lock.
* Returns the number of bytes successfully written to the pipe.
* In case of failure due to a closed read end or a killed process, it returns -1.
* Exceptions: This method may return -1 if the read end of the pipe is closed or if the process is killed.
*
* Example:
* struct pipe mypipe;
* char data[] = "Hello, World!";
* int bytes_written = pipewrite(&mypipe, data, strlen(data));
* if (bytes_written == -1) {
*     printf("Failed to write to pipe.\n");
* } else {
*     printf("%d bytes successfully written to the pipe.\n", bytes_written);
* }
*/
int
pipewrite(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock);
  for(i = 0; i < n; i++){
    while(p->nwrite == p->nread + PIPESIZE){  //DOC: pipewrite-full
      if(p->readopen == 0 || myproc()->killed){
        release(&p->lock);
        return -1;
      }
      wakeup(&p->nread);
      sleep(&p->nwrite, &p->lock);  //DOC: pipewrite-sleep
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i];
  }
  wakeup(&p->nread);  //DOC: pipewrite-wakeup1
  release(&p->lock);
  return n;
}

/**
* This method reads data from a pipe <paramref name="p"/> into the provided address <paramref name="addr"/> with a maximum length of <paramref name="n"/>.
* It waits until there is data available in the pipe to read, unless the writing end of the pipe is closed or the process is killed.
* If the process is killed while waiting, it returns -1.
* It copies the data from the pipe into the address buffer, up to a maximum of <paramref name="n"/> bytes.
* It wakes up the writing end of the pipe after reading data.
* Returns the number of bytes read.
* Exceptions: 
* - Returns -1 if the process is killed while waiting for data to be available in the pipe.
* Example:
* <code>
* struct pipe mypipe;
* char buffer[100];
* int bytes_read = piperead(&mypipe, buffer, 50);
* </code>
*/
int
piperead(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock);
  while(p->nread == p->nwrite && p->writeopen){  //DOC: pipe-empty
    if(myproc()->killed){
      release(&p->lock);
      return -1;
    }
    sleep(&p->nread, &p->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(p->nread == p->nwrite)
      break;
    addr[i] = p->data[p->nread++ % PIPESIZE];
  }
  wakeup(&p->nwrite);  //DOC: piperead-wakeup
  release(&p->lock);
  return i;
}
