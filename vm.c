#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

/**
* This method initializes the segment descriptors for the CPU.
* It maps "logical" addresses to virtual addresses using an identity map.
* It sets up different segment descriptors for kernel code, kernel data, user code, and user data.
* It loads the segment descriptors into the GDT (Global Descriptor Table) and updates the GDTR (Global Descriptor Table Register).
* This method should be called during system initialization to set up the memory segmentation.
* 
* @exception This method may throw exceptions if there are issues with setting up the segment descriptors or loading them into the GDT.
* 
* @example
* seginit();
*/
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

/**
* This method walks through the page directory <paramref name="pgdir"/> to find the page table entry for the virtual address <paramref name="va"/>.
* If the page table entry is present, it returns the corresponding page table. If not present and <paramref name="alloc"/> is true, it allocates a new page table.
* If a new page table is allocated, it initializes it with zeroed PTE_P bits and sets permissions.
* 
* @param pgdir The page directory to walk through.
* @param va The virtual address for which to find the page table entry.
* @param alloc Flag indicating whether to allocate a new page table if not present.
* @return A pointer to the page table entry for the virtual address.
* @exception Returns 0 if allocation fails or if the page table entry is not present and allocation is not allowed.
*
* Example:
* pde_t *pgdir;
* const void *va;
* int alloc = 1;
* pte_t *result = walkpgdir(pgdir, va, alloc);
*/
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

/**
* This method maps physical memory pages to virtual memory pages in the page directory <paramref name="pgdir"/>.
* 
* @param pgdir The page directory where the mapping will be done.
* @param va The starting virtual address for mapping.
* @param size The size of the memory region to be mapped.
* @param pa The physical address to be mapped.
* @param perm The permissions for the mapping.
* @return 0 if successful, -1 if an error occurs during mapping.
* @throws panic if attempting to remap a page that is already present in the page table.
* 
* Example:
* <code>
* pde_t *pgdir;
* void *va;
* uint size;
* uint pa;
* int perm;
* int result = mappages(pgdir, va, size, pa, perm);
* if(result == 0) {
*     printf("Memory pages mapped successfully.\n");
* } else {
*     printf("Error mapping memory pages.\n");
* }
* </code>
*/
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
/**            
* This method sets up the kernel page table and returns a pointer to the page directory entry.            
* It allocates memory for the page directory, initializes it, and maps the kernel memory regions.            
* If successful, it returns a pointer to the page directory; otherwise, it returns 0.            
* Exceptions: It may panic with the message "PHYSTOP too high" if the physical memory limit is exceeded.            
* Example:            
* pde_t* pgdir = setupkvm();            
*/
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

/**
* This method allocates memory for the kernel page directory by calling the setupkvm function and then switches to the kernel page directory using switchkvm.
* 
* @exception None
* 
* @example
* kvmalloc();
*/
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

/**
* This method switches to the kernel page table by loading the value of the physical address of the kernel page directory into the CR3 register.
* This function does not throw any exceptions.
* Example:
* switchkvm();
*/
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

/**
* This method switches the address space to the process's address space for the given process <paramref name="p"/>.
* It performs necessary checks for the process and its kernel stack before switching the address space.
* This method is essential for context switching in the operating system.
*
* @param p Pointer to the process structure for which the address space needs to be switched.
* @throws panic if the process pointer <paramref name="p"/> is NULL.
* @throws panic if the kernel stack of the process <paramref name="p"/> is NULL.
* @throws panic if the page directory of the process <paramref name="p"/> is NULL.
*
* Example:
* struct proc *my_process = get_current_process();
* switchuvm(my_process);
*/
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

/**
* Initializes a user virtual memory space in the page directory <paramref name="pgdir"/> with the provided initial data <paramref name="init"/> of size <paramref name="sz"/>.
* 
* This method allocates memory, checks if the size is not greater than a page size, initializes the memory with zeros, maps the pages in the page directory, and copies the initial data into the allocated memory.
* 
* @param pgdir Pointer to the page directory
* @param init Pointer to the initial data
* @param sz Size of the initial data
* @throws panic if the size is greater than or equal to a page size
* 
* Example:
* 
* <code>
* pde_t *pgdir;
* char *init_data = "example";
* uint size = strlen(init_data);
* inituvm(pgdir, init_data, size);
* </code>
*/
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

/**
* This method loads the user virtual memory <paramref name="addr"/> into the page directory <paramref name="pgdir"/> from the inode <paramref name="ip"/> at the specified offset <paramref name="offset"/> with size <paramref name="sz"/>.
* It ensures that the address is page aligned and that the address exists in the page directory.
* If successful, it returns 0; otherwise, it returns -1.
* @param pgdir The page directory to load into.
* @param addr The address of the user virtual memory.
* @param ip The inode containing the data.
* @param offset The offset within the inode.
* @param sz The size of the data to load.
* @return 0 if successful, -1 if unsuccessful.
* @throws panic if the address is not page aligned or if the address does not exist in the page directory.
*
* Example:
* int result = loaduvm(pgdir, addr, ip, offset, sz);
* if(result == 0) {
*     printf("User virtual memory loaded successfully.");
* } else {
*     printf("Failed to load user virtual memory.");
* }
*/
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

/**
* This method allocates memory for a process in the virtual memory space.
* It takes in the page directory <paramref name="pgdir"/>, the old size <paramref name="oldsz"/>, and the new size <paramref name="newsz"/>.
* If the new size is greater than or equal to KERNBASE, it returns 0.
* If the new size is less than the old size, it returns the old size.
* It then allocates memory in pages from the old size to the new size using kalloc().
* If memory allocation fails, it prints an error message and deallocates the memory.
* It then maps the allocated memory to the page directory with appropriate permissions.
* If mapping fails, it prints an error message, deallocates memory, and returns 0.
* @param pgdir The page directory for the process.
* @param oldsz The old size of the process's memory.
* @param newsz The new size to be allocated.
* @return Returns the new size if successful, 0 otherwise.
* @exception If memory allocation or mapping fails, it handles the error and returns 0.
*
* Example:
* uint oldsz = 4096;
* uint newsz = 8192;
* pde_t *pgdir = get_pgdir();
* int result = allocuvm(pgdir, oldsz, newsz);
* if(result == 0) {
*     printf("Memory allocation failed");
* } else {
*     printf("Memory allocated successfully from %d to %d", oldsz, newsz);
* }
*/
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

/**
* This method deallocates memory pages in the virtual memory of the process. It iterates over the page directory entries and frees the physical memory associated with each page that falls within the range specified by <paramref name="newsz"/> and <paramref name="oldsz"/>.
* If an exception occurs during the deallocation process, it may trigger a panic with the message "kfree".
*
* @param pgdir The page directory of the process.
* @param oldsz The old size of the memory region.
* @param newsz The new size of the memory region.
* @return The updated size after deallocating memory pages.
*
* @exception If an invalid page table entry is encountered or if the physical address associated with a page is 0, it may trigger a panic with the message "kfree".
*
* Example:
* <code>
* pde_t *pgdir;
* uint oldsz = 4096;
* uint newsz = 2048;
* int result = deallocuvm(pgdir, oldsz, newsz);
* </code>
*/
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

/**
* This method frees the virtual memory allocated for a page directory.
* It takes a pointer to the page directory <paramref name="pgdir"/> as input.
* If the input page directory is NULL, it triggers a panic.
* It deallocates user virtual memory starting from KERNBASE.
* For each page directory entry, if the entry is present (PTE_P bit set), it frees the corresponding virtual address.
* Finally, it frees the page directory itself.
* @param pgdir Pointer to the page directory to be freed.
* @throws panic if the input page directory is NULL.
* @example
* <code>
* pde_t *pgdir = some_pgdir;
* freevm(pgdir);
* </code>
*/
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

/**
* This method clears the user access permission bit in the page table entry for the specified user virtual address.
* It takes a page directory entry pointer <paramref name="pgdir"/> and a user virtual address <paramref name="uva"/> as input.
* If the page table entry for the virtual address is not found, it triggers a panic.
* Exceptions: This method may trigger a panic if the page table entry for the specified virtual address is not found.
* Example:
* pde_t *pgdir;
* char *uva;
* clearpteu(pgdir, uva);
*/
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

/**
* This method copies the user virtual memory <paramref name="pgdir"/> to a new page directory <paramref name="d"/> with the specified size <paramref name="sz"/>.
* It iterates through the virtual memory pages and copies each page to the new page directory.
* If any errors occur during the copying process, it frees the allocated memory and returns 0.
* 
* @param pgdir The original page directory to copy from.
* @param sz The size of the virtual memory to copy.
* @return A pointer to the new page directory if successful, otherwise 0.
* @throws panic if the page table entry does not exist or if the page is not present.
* @throws panic if memory allocation fails during the copying process.
* @throws panic if mapping pages to the new directory fails.
* 
* Example:
* pde_t *new_pgdir = copyuvm(pgdir, sz);
*/
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

/**
* This method converts a user virtual address (uva) to a kernel virtual address (ka) by traversing the page directory (pgdir).
* It first retrieves the page table entry (pte) corresponding to the uva.
* If the page table entry is not present or is not marked as present (PTE_P), the method returns 0.
* If the page table entry is not marked as user accessible (PTE_U), the method returns 0.
* Otherwise, it returns the kernel virtual address corresponding to the physical address extracted from the page table entry.
*
* @param pgdir The page directory to traverse.
* @param uva The user virtual address to convert.
* @return The kernel virtual address corresponding to the user virtual address, or 0 if an exception occurs.
*
* @exception If the page table entry is not present (PTE_P) or not user accessible (PTE_U), the method returns 0.
*
* Example:
* pde_t *pgdir = get_page_directory();
* char *uva = (char*)0x12345678;
* char *ka = uva2ka(pgdir, uva);
*/
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

/**
* This method copies data from a virtual address <paramref name="va"/> to a physical address <paramref name="p"/> in the given page directory <paramref name="pgdir"/>.
* It iterates through the data to be copied, ensuring that the data is correctly aligned and within the page size.
* If the physical address corresponding to the virtual address is not found, it returns -1.
* 
* @param pgdir The page directory where the data will be copied.
* @param va The virtual address from where the data will be copied.
* @param p The pointer to the physical address where the data will be copied.
* @param len The length of the data to be copied.
* 
* @return 0 if the data is successfully copied, -1 if an error occurs.
* 
* @exception If the physical address corresponding to the virtual address is not found, it returns -1.
* 
* Example:
* <code>
* pde_t *pgdir;
* uint va;
* void *p;
* uint len;
* int result = copyout(pgdir, va, p, len);
* if(result == 0) {
*     printf("Data copied successfully.\n");
* } else {
*     printf("Error copying data.\n");
* }
* </code>
*/
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

