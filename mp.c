// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mp.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"

struct cpu cpus[NCPU];
int ncpu;
uchar ioapicid;

/**
* This method calculates the sum of elements in the input array <paramref name="addr"/> up to a specified length <paramref name="len"/>.
* 
* @param addr Pointer to the array of unsigned characters.
* @param len Length of the array to be considered for sum calculation.
* @return The sum of elements in the array up to the specified length.
* @throws None
* 
* Example:
* uchar arr[] = {1, 2, 3, 4, 5};
* int length = 5;
* uchar result = sum(arr, length);
* // After execution, result will contain the sum of elements {1, 2, 3, 4, 5} which is 15.
*/
static uchar
sum(uchar *addr, int len)
{
  int i, sum;

  sum = 0;
  for(i=0; i<len; i++)
    sum += addr[i];
  return sum;
}

/**
* This method searches for a specific structure within a memory region starting at address <paramref name="a"/> with a length of <paramref name="len"/>.
* It iterates through the memory region checking for the structure "_MP_" and verifying its integrity using the sum function.
* If a valid structure is found, it returns a pointer to that structure, otherwise it returns 0.
* 
* @param a The starting address of the memory region to search.
* @param len The length of the memory region to search.
* @return A pointer to the found structure if successful, otherwise 0.
* @throws None
* 
* Example:
* <code>
* struct mp* result = mpsearch1(0x1000, 256);
* if(result != 0) {
*     // Found a valid structure, process it
* } else {
*     // Structure not found
* }
* </code>
*/
static struct mp*
mpsearch1(uint a, int len)
{
  uchar *e, *p, *addr;

  addr = P2V(a);
  e = addr+len;
  for(p = addr; p < e; p += sizeof(struct mp))
    if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

/**            
* This method searches for a specific memory region based on the BIOS Data Area (BDA) information.            
* It retrieves the base memory address from the BDA and calculates the physical address based on the segment values.            
* If a valid memory region is found, it returns the corresponding memory structure.            
* If no valid memory region is found, it returns a default memory structure based on predefined values.            
* 
* @return struct mp* - A pointer to the memory structure found or a default memory structure.            
* @throws NoMemoryRegionFoundException - If no valid memory region is found based on the BDA information.            
* 
* Example:            
* struct mp* memoryRegion = mpsearch();            
*/
static struct mp*
mpsearch(void)
{
  uchar *bda;
  uint p;
  struct mp *mp;

  bda = (uchar *) P2V(0x400);
  if((p = ((bda[0x0F]<<8)| bda[0x0E]) << 4)){
    if((mp = mpsearch1(p, 1024)))
      return mp;
  } else {
    p = ((bda[0x14]<<8)|bda[0x13])*1024;
    if((mp = mpsearch1(p-1024, 1024)))
      return mp;
  }
  return mpsearch1(0xF0000, 0x10000);
}

/**
* This method initializes and configures the MP configuration structure based on the provided MP pointer.
* It checks if the MP pointer is valid and the physical address is not zero before proceeding with the configuration.
* It verifies the MP configuration signature and version to ensure compatibility.
* Additionally, it performs a checksum validation on the configuration data.
* If all checks pass, it assigns the MP pointer to the provided pointer and returns the configured MP configuration structure.
* If any of the checks fail, it returns 0 to indicate an error in configuration.
*
* @param pmp Pointer to a pointer to the MP structure.
* @return Pointer to the configured MP configuration structure if successful, otherwise 0.
*
* @exception Returns 0 if the MP pointer is invalid or the physical address is zero.
* @exception Returns 0 if the MP configuration signature is not "PCMP" or the version is not 1 or 4.
* @exception Returns 0 if the checksum validation of the configuration data fails.
*
* Example:
* struct mp *mp;
* struct mpconf *conf = mpconfig(&mp);
* if (conf != 0) {
*     // Configuration successful, use conf and mp
* } else {
*     // Configuration failed
* }
*/
static struct mpconf*
mpconfig(struct mp **pmp)
{
  struct mpconf *conf;
  struct mp *mp;

  if((mp = mpsearch()) == 0 || mp->physaddr == 0)
    return 0;
  conf = (struct mpconf*) P2V((uint) mp->physaddr);
  if(memcmp(conf, "PCMP", 4) != 0)
    return 0;
  if(conf->version != 1 && conf->version != 4)
    return 0;
  if(sum((uchar*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

/**
* This method initializes the multiprocessing environment by configuring the system based on the detected hardware components.
* It checks if the system is expected to run on a Symmetric Multiprocessing (SMP) architecture and sets up necessary data structures accordingly.
* It handles processor information, I/O APIC information, and other relevant configurations.
* If the system is not suitable for multiprocessing, it triggers a panic.
* Additionally, it includes a specific handling for the IMCR register on real hardware.
* Note: This method assumes certain hardware configurations and may not be compatible with all systems.
*/
void
mpinit(void)
{
  uchar *p, *e;
  int ismp;
  struct mp *mp;
  struct mpconf *conf;
  struct mpproc *proc;
  struct mpioapic *ioapic;

  if((conf = mpconfig(&mp)) == 0)
    panic("Expect to run on an SMP");
  ismp = 1;
  lapic = (uint*)conf->lapicaddr;
  for(p=(uchar*)(conf+1), e=(uchar*)conf+conf->length; p<e; ){
    switch(*p){
    case MPPROC:
      proc = (struct mpproc*)p;
      if(ncpu < NCPU) {
        cpus[ncpu].apicid = proc->apicid;  // apicid may differ from ncpu
        ncpu++;
      }
      p += sizeof(struct mpproc);
      continue;
    case MPIOAPIC:
      ioapic = (struct mpioapic*)p;
      ioapicid = ioapic->apicno;
      p += sizeof(struct mpioapic);
      continue;
    case MPBUS:
    case MPIOINTR:
    case MPLINTR:
      p += 8;
      continue;
    default:
      ismp = 0;
      break;
    }
  }
  if(!ismp)
    panic("Didn't find a suitable machine");

  if(mp->imcrp){
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
}
