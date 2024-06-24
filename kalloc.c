// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
/**
* This method initializes the kernel memory lock and frees the memory range between the specified start and end addresses.
* 
* @param vstart Pointer to the start address of the memory range
* @param vend Pointer to the end address of the memory range
* @throws None
* 
* Example:
* void *start = (void*)0x1000;
* void *end = (void*)0x2000;
* kinit1(start, end);
*/
/**
* This method frees the memory in the range specified by the input pointers <paramref name="vstart"/> and <paramref name="vend"/>.
* It iterates over the memory range in page-sized increments and frees each page using the kfree function.
* 
* @param vstart Pointer to the start of the memory range to be freed.
* @param vend Pointer to the end of the memory range to be freed.
* 
* @exception This method does not handle any exceptions.
/**
* Initializes the kernel memory starting from the address specified by <paramref name="vstart"/> up to the address specified by <paramref name="vend"/>.
* This function calls the freerange method to free memory within the specified range and sets the kernel memory lock to be used.
* 
* @param vstart Pointer to the start address of the kernel memory.
* @param vend Pointer to the end address of the kernel memory.
* @exception None
* 
* Example:
* void *start = (void*)0x1000;
* void *end = (void*)0x2000;
* kinit2(start, end);
*/
* 
* Example:
* 
* void *start = some_start_pointer;
* void *end = some_end_pointer;
* freerange(start, end);
*/
void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
/**
* This method frees the memory allocated for the input pointer <paramref name="v"/> after performing necessary checks.
* It ensures that the memory address is valid and within the allocated range before freeing it.
* If the memory address is not valid, it triggers a panic.
* After freeing the memory, it fills the freed memory with junk to prevent dangling references.
* Finally, it updates the free list in the memory management system.
* This method should be used with caution to avoid memory leaks and undefined behavior.
* 
* @param v Pointer to the memory block to be freed.
* @throws panic if the memory address is invalid or out of range.
* 
* Example:
* 
* char *ptr = (char *)malloc(sizeof(char) * 100);
* kfree(ptr);
*/
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

/**
* This method allocates memory by returning a pointer to a free memory block.
* It checks if the kernel memory lock is in use, acquires the lock if necessary, retrieves a free memory block from the freelist,
* updates the freelist, releases the lock if it was acquired, and returns a pointer to the allocated memory block.
* 
* @return A pointer to the allocated memory block.
* 
* @exception This method may throw exceptions if there are issues with acquiring or releasing the kernel memory lock.
* 
* Example:
* char* ptr = kalloc();
*/
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

