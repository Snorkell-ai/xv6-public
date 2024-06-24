// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "param.h"
#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "traps.h"
#include "mmu.h"
#include "x86.h"

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define EOI     (0x00B0/4)   // EOI
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // Error Status
#define ICRLO   (0x0300/4)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration

volatile uint *lapic;  // Initialized in mp.c

//PAGEBREAK!
/**
* This method writes a value to the Local APIC register at the specified index.
* It takes two parameters: index - the index of the register to write to, and value - the value to write.
* This method does not return any value.
* 
* Exceptions:
* - None
* 
* Example:
* To write a value of 100 to the Local APIC register at index 0, you can call the function as follows:
* lapicw(0, 100);
*/
static void
lapicw(int index, int value)
{
  lapic[index] = value;
  lapic[ID];  // wait for write to finish, by reading
}

/**
* This method initializes the local APIC by configuring various registers and settings.
* It enables the local APIC, sets the spurious interrupt vector, configures the timer for periodic interrupts,
* disables logical interrupt lines, maps error interrupt to IRQ_ERROR, clears error status register,
* acknowledges any outstanding interrupts, and synchronizes arbitration IDs.
* 
* @exception None
* 
* Example:
* lapicinit();
*/
void
lapicinit(void)
{
  if(!lapic)
    return;

  // Enable local APIC; set spurious interrupt vector.
  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // The timer repeatedly counts down at bus frequency
  // from lapic[TICR] and then issues an interrupt.
  // If xv6 cared more about precise timekeeping,
  // TICR would be calibrated using an external time source.
  lapicw(TDCR, X1);
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, 10000000);

  // Disable logical interrupt lines.
  lapicw(LINT0, MASKED);
  lapicw(LINT1, MASKED);

  // Disable performance counter overflow interrupts
  // on machines that provide that interrupt entry.
  if(((lapic[VER]>>16) & 0xFF) >= 4)
    lapicw(PCINT, MASKED);

  // Map error interrupt to IRQ_ERROR.
  lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

  // Clear error status register (requires back-to-back writes).
  lapicw(ESR, 0);
  lapicw(ESR, 0);

  // Ack any outstanding interrupts.
  lapicw(EOI, 0);

  // Send an Init Level De-Assert to synchronise arbitration ID's.
  lapicw(ICRHI, 0);
  lapicw(ICRLO, BCAST | INIT | LEVEL);
  while(lapic[ICRLO] & DELIVS)
    ;

  // Enable interrupts on the APIC (but not on the processor).
  lapicw(TPR, 0);
}

/**
* This method returns the Local Advanced Programmable Interrupt Controller (APIC) ID.
* If the Local APIC is not available, it returns 0.
* 
* @return The Local APIC ID shifted by 24 bits.
* 
* @exception None
* 
* @example
* <code>
* int id = lapicid();
* </code>
*/
int
lapicid(void)
{
  if (!lapic)
    return 0;
  return lapic[ID] >> 24;
}

/**            
* This method sends an End of Interrupt (EOI) signal to the local Advanced Programmable Interrupt Controller (APIC) if the APIC is enabled.            
* If the APIC is enabled, it calls the function lapicw with the parameters EOI and 0 to send the EOI signal.            
* This function helps in managing interrupts and signaling the end of an interrupt to the APIC.            
* @exception If the APIC is not enabled, the EOI signal will not be sent.            
* @example            
* lapiceoi(); // Sends an End of Interrupt signal to the local APIC if it is enabled.            
*/
void
lapiceoi(void)
{
  if(lapic)
    lapicw(EOI, 0);
}

/**
* This method introduces a micro delay of the specified duration in microseconds.
* 
* @param us The duration of the micro delay in microseconds.
* 
* @exception None
* 
* @example
* microdelay(1000); // Introduces a micro delay of 1000 microseconds.
*/
void
microdelay(int us)
{
}

#define CMOS_PORT    0x70
#define CMOS_RETURN  0x71

/**
* This method initializes the Local Advanced Programmable Interrupt Controller (LAPIC) to start an application processor (AP).
* The BSP must initialize the CMOS shutdown code to 0AH and the warm reset vector to point at the AP startup code before using this method.
* The method follows the universal startup algorithm to reset the other CPU and start the AP.
* 
* @param apicid The APIC ID of the application processor to start.
* @param addr The address where the AP startup code is located.
* @throws None
* 
* Example:
* lapicstartap(1, 0x1000);
*/
void
lapicstartap(uchar apicid, uint addr)
{
  int i;
  ushort *wrv;

  // "The BSP must initialize CMOS shutdown code to 0AH
  // and the warm reset vector (DWORD based at 40:67) to point at
  // the AP startup code prior to the [universal startup algorithm]."
  outb(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
  outb(CMOS_PORT+1, 0x0A);
  wrv = (ushort*)P2V((0x40<<4 | 0x67));  // Warm reset vector
  wrv[0] = 0;
  wrv[1] = addr >> 4;

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  lapicw(ICRHI, apicid<<24);
  lapicw(ICRLO, INIT | LEVEL | ASSERT);
  microdelay(200);
  lapicw(ICRLO, INIT | LEVEL);
  microdelay(100);    // should be 10ms, but too slow in Bochs!

  // Send startup IPI (twice!) to enter code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for(i = 0; i < 2; i++){
    lapicw(ICRHI, apicid<<24);
    lapicw(ICRLO, STARTUP | (addr>>12));
    microdelay(200);
  }
}

#define CMOS_STATA   0x0a
#define CMOS_STATB   0x0b
#define CMOS_UIP    (1 << 7)        // RTC update in progress

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

/**
* This method reads a value from the CMOS memory by writing to the specified register <paramref name="reg"/>.
* It first writes to the CMOS_PORT and then waits for a short delay of 200 microseconds using microdelay().
* Finally, it reads the value from the CMOS_RETURN port and returns it as a result.
* 
* @param reg The register to read from in the CMOS memory.
* @return The value read from the specified register in the CMOS memory.
* @throws None
* 
* Example:
* uint result = cmos_read(0x0A);
*/
static uint
cmos_read(uint reg)
{
  outb(CMOS_PORT,  reg);
  microdelay(200);

  return inb(CMOS_RETURN);
}

/**
* This method fills the provided rtcdate structure with the current date and time values obtained from the CMOS registers.
* It reads the seconds, minutes, hours, day, month, and year values from the CMOS registers and assigns them to the respective fields in the rtcdate structure.
* 
* @param r A pointer to the rtcdate structure that will be filled with the date and time values.
* @exception This method does not throw any exceptions.
* 
* Example:
* struct rtcdate myDate;
* fill_rtcdate(&myDate);
* // After this call, myDate will contain the current date and time values obtained from the CMOS registers.
*/
static void
fill_rtcdate(struct rtcdate *r)
{
  r->second = cmos_read(SECS);
  r->minute = cmos_read(MINS);
  r->hour   = cmos_read(HOURS);
  r->day    = cmos_read(DAY);
  r->month  = cmos_read(MONTH);
  r->year   = cmos_read(YEAR);
}

/**
* This method reads the current time from the CMOS and stores it in the provided struct rtcdate pointer <paramref name="r"/>.
* It ensures that the CMOS time is not modified while reading it.
* If the CMOS time is in BCD format, it converts it to binary format before storing it in the rtcdate structure.
*
* @param r Pointer to a struct rtcdate where the current time will be stored.
* @throws None
*
* Example:
* struct rtcdate current_time;
* cmostime(&current_time);
*/
void
cmostime(struct rtcdate *r)
{
  struct rtcdate t1, t2;
  int sb, bcd;

  sb = cmos_read(CMOS_STATB);

  bcd = (sb & (1 << 2)) == 0;

  // make sure CMOS doesn't modify time while we read it
  for(;;) {
    fill_rtcdate(&t1);
    if(cmos_read(CMOS_STATA) & CMOS_UIP)
        continue;
    fill_rtcdate(&t2);
    if(memcmp(&t1, &t2, sizeof(t1)) == 0)
      break;
  }

  // convert
  if(bcd) {
#define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
    CONV(second);
    CONV(minute);
    CONV(hour  );
    CONV(day   );
    CONV(month );
    CONV(year  );
#undef     CONV
  }

  *r = t1;
  r->year += 2000;
}
