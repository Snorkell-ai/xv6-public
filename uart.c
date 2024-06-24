// Intel 8250 serial port (UART).

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define COM1    0x3f8

static int uart;    // is there a uart?

/**
* This method initializes the UART communication by configuring the settings such as baud rate, data bits, stop bits, and parity.
* It also enables receive interrupts and checks for the presence of a serial port.
* If a serial port is detected, it acknowledges pre-existing interrupt conditions and enables interrupts.
* Finally, it sends a message "xv6..." over the UART communication.
* Exceptions: This method does not handle any exceptions explicitly.
* Example:
* uartinit();
*/
void
uartinit(void)
{
  char *p;

  // Turn off the FIFO
  outb(COM1+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(COM1+3, 0x80);    // Unlock divisor
  outb(COM1+0, 115200/9600);
  outb(COM1+1, 0);
  outb(COM1+3, 0x03);    // Lock divisor, 8 data bits.
  outb(COM1+4, 0);
  outb(COM1+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if(inb(COM1+5) == 0xFF)
    return;
  uart = 1;

  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(COM1+2);
  inb(COM1+0);
  ioapicenable(IRQ_COM1, 0);

  // Announce that we're here.
  for(p="xv6...\n"; *p; p++)
    uartputc(*p);
}

/**
* This method sends a character <paramref name="c"/> to the UART (Universal Asynchronous Receiver-Transmitter) device.
* It waits for the UART to be ready before sending the character.
* 
* @param c The character to be sent to the UART.
* 
* @exception If the UART device is not initialized (<paramref name="uart"/> is not set), the function will return without sending the character.
* 
* Example:
* 
* <code>
* uartputc('A');
* </code>
*/
void
uartputc(int c)
{
  int i;

  if(!uart)
    return;
  for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
    microdelay(10);
  outb(COM1+0, c);
}

/**
* This method reads a character from the UART (Universal Asynchronous Receiver/Transmitter) input buffer.
* If the UART is not initialized or if there is no data available in the buffer, it returns -1.
* 
* @return The character read from the UART input buffer, or -1 if there is an error or no data available.
* 
* @exception If the UART is not initialized, it returns -1.
* @exception If there is no data available in the UART input buffer, it returns -1.
* 
* Example:
* 
* int receivedChar = uartgetc();
* if(receivedChar != -1) {
*     printf("Received character: %c\n", receivedChar);
* } else {
*     printf("Error reading character from UART.\n");
* }
*/
static int
uartgetc(void)
{
  if(!uart)
    return -1;
  if(!(inb(COM1+5) & 0x01))
    return -1;
  return inb(COM1+0);
}

/**
* This method handles UART interrupts by calling the consoleintr function with uartgetc as a parameter.
* 
* @exception This method does not throw any exceptions.
* 
* @example
* uartintr();
*/
void
uartintr(void)
{
  consoleintr(uartgetc);
}
