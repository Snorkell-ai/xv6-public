#include "types.h"
#include "x86.h"
#include "traps.h"

// I/O Addresses of the two programmable interrupt controllers
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)

// Don't use the 8259A interrupt controllers.  Xv6 assumes SMP hardware.
/**
* Initializes the Programmable Interrupt Controller (PIC) by masking all interrupts.
* This method masks all interrupts for both PIC1 and PIC2.
* 
* @exception None
* 
* @example
* picinit();
*/
void
picinit(void)
{
  // mask all interrupts
  outb(IO_PIC1+1, 0xFF);
  outb(IO_PIC2+1, 0xFF);
}

//PAGEBREAK!
// Blank page.
