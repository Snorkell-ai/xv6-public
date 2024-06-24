// The I/O APIC manages hardware interrupts for an SMP system.
// http://www.intel.com/design/chipsets/datashts/29056601.pdf
// See also picirq.c.

#include "types.h"
#include "defs.h"
#include "traps.h"

#define IOAPIC  0xFEC00000   // Default physical address of IO APIC

#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define INT_DISABLED   0x00010000  // Interrupt disabled
#define INT_LEVEL      0x00008000  // Level-triggered (vs edge-)
#define INT_ACTIVELOW  0x00002000  // Active low (vs high)
#define INT_LOGICAL    0x00000800  // Destination is CPU id (vs APIC ID)

volatile struct ioapic *ioapic;

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
  uint reg;
  uint pad[3];
  uint data;
};

/**
* This method reads the value from the specified register in the IOAPIC.
* It takes an integer parameter <paramref name="reg"/> representing the register to read from.
* It then assigns the value of the specified register in the IOAPIC to the data field and returns it.
* 
* @param reg The register number to read from in the IOAPIC.
* @return The data read from the specified register in the IOAPIC.
* @exception None
* 
* Example:
* 
* uint data = ioapicread(0x10);
*/
static uint
ioapicread(int reg)
{
  ioapic->reg = reg;
  return ioapic->data;
}

/**
* This method writes data to the specified register in the IOAPIC.
* 
* @param reg The register to write data to.
* @param data The data to be written to the register.
* 
* @exception None
* 
* Example:
* ioapicwrite(0x10, 0xFF);
*/
static void
ioapicwrite(int reg, uint data)
{
  ioapic->reg = reg;
  ioapic->data = data;
}

/**
* This method initializes the IOAPIC by setting all interrupts to be edge-triggered, active high, disabled, and not routed to any CPUs.
* It reads the maximum number of interrupts supported by the IOAPIC and the IOAPIC ID to verify if it's a MP system.
* If the ID is not equal to ioapicid, it prints a message indicating that it's not a MP system.
* 
* @throws None
* 
* Example:
* ioapicinit();
*/
void
ioapicinit(void)
{
  int i, id, maxintr;

  ioapic = (volatile struct ioapic*)IOAPIC;
  maxintr = (ioapicread(REG_VER) >> 16) & 0xFF;
  id = ioapicread(REG_ID) >> 24;
  if(id != ioapicid)
    cprintf("ioapicinit: id isn't equal to ioapicid; not a MP\n");

  // Mark all interrupts edge-triggered, active high, disabled,
  // and not routed to any CPUs.
  for(i = 0; i <= maxintr; i++){
    ioapicwrite(REG_TABLE+2*i, INT_DISABLED | (T_IRQ0 + i));
    ioapicwrite(REG_TABLE+2*i+1, 0);
  }
}

/**
* This method enables a specific interrupt <paramref name="irq"/> for a given CPU <paramref name="cpunum"/> by configuring it as edge-triggered, active high, and routed to the specified CPU.
* It utilizes the IOAPIC registers to set the interrupt configuration.
* 
* @param irq The interrupt number to be enabled.
* @param cpunum The CPU number to which the interrupt is routed.
* 
* @exception None
* 
* Example:
* ioapicenable(5, 1);
*/
void
ioapicenable(int irq, int cpunum)
{
  // Mark interrupt edge-triggered, active high,
  // enabled, and routed to the given cpunum,
  // which happens to be that cpu's APIC ID.
  ioapicwrite(REG_TABLE+2*irq, T_IRQ0 + irq);
  ioapicwrite(REG_TABLE+2*irq+1, cpunum << 24);
}
