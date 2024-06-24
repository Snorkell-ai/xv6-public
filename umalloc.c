#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

/**
* This method frees the memory allocated for a block of memory pointed to by <paramref name="ap"/>.
* It combines adjacent free blocks to prevent fragmentation and optimize memory usage.
* 
* @param ap A pointer to the block of memory to be freed.
* @throws None
* 
* Example:
* void *ptr = malloc(100 * sizeof(int)); // Allocate memory
* free(ptr); // Free the allocated memory
*/
void
free(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

/**
* This method allocates memory for a new block of size <paramref name="nu"/> using the morecore algorithm.
* If the requested size <paramref name="nu"/> is less than 4096, it is adjusted to 4096.
* If memory allocation using sbrk fails, it returns NULL.
* The allocated memory block is initialized with a Header structure and its size is set to <paramref name="nu"/>.
* The memory block is then freed immediately after initialization.
* Returns a pointer to the newly allocated memory block.
* @param nu The requested size of the memory block.
* @return A pointer to the allocated memory block, or NULL if allocation fails.
* @exception Returns NULL if memory allocation using sbrk fails.
*
* Example:
* Header* newBlock = morecore(8192);
*/
static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

/**
* This method allocates memory for a block of size <paramref name="nbytes"/> and returns a pointer to the allocated memory.
* It internally manages a free list of memory blocks to optimize memory allocation.
* 
* @param nbytes The number of bytes to allocate memory for.
* @return A pointer to the allocated memory block.
* @exception Returns NULL if memory allocation fails.
* 
* Example:
* void* ptr = malloc(100);
*/
void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}
