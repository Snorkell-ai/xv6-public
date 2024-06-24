// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE  512

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000;  // scratch space

  // Read 1st page off disk
  /**
  * This method loads and executes an ELF executable from disk.
  * It reads the ELF header, checks if it is a valid ELF executable, loads each program segment into memory, and calls the entry point.
  * This method does not return.
  *
  * @throws - No exceptions are explicitly handled within this method.
  *
  * Example:
  * bootmain();
  */
  readseg((uchar*)elf, 4096, 0);

  // Is this an ELF executable?
  if(elf->magic != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  eph = ph + elf->phnum;
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr;
    readseg(pa, ph->filesz, ph->off);
    if(ph->memsz > ph->filesz)
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  entry = (void(*)(void))(elf->entry);
  entry();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
/**
* This method waits for the disk to be ready before proceeding further.
* It continuously checks the status of the disk by reading from port 0x1F7 and waits until the status indicates the disk is ready.
* This function does not take any parameters or return any value.
* 
* Exceptions:
* - None
* 
* Example:
* waitdisk();
*/
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1);   // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
/**
* This method reads a sector from the disk and stores the data in the memory location pointed to by <paramref name="dst"/>.
* It reads the sector at the specified <paramref name="offset"/> on the disk.
* This method issues a read command to the disk controller and then reads the data into the memory location.
* 
* @param dst Pointer to the memory location where the sector data will be stored.
* @param offset The offset of the sector to be read on the disk.
* 
* @exception This method may throw exceptions if there are issues with disk access or data retrieval.
* 
* Example:
* void *data = malloc(SECTSIZE);
* readsect(data, 0x1000);
*/

  // Read data.
  waitdisk();
  insl(0x1F0, dst, SECTSIZE/4);
}

/**
* This method reads a specified number of bytes from a given offset in memory.
* It rounds down to the sector boundary, translates bytes to sectors, and reads sectors one by one.
* If performance is a concern, multiple sectors can be read at once.
* 
* @param pa Pointer to the memory location where the data will be read into.
* @param count Number of bytes to read.
* @param offset Offset in memory from where to start reading.
* @throws None
* 
* Example:
* readseg(memory_pointer, 512, 1024);
*/
void
readseg(uchar* pa, uint count, uint offset)
{
  uchar* epa;

  epa = pa + count;

  // Round down to sector boundary.
  pa -= offset % SECTSIZE;

  // Translate from bytes to sectors; kernel starts at sector 1.
  offset = (offset / SECTSIZE) + 1;

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
