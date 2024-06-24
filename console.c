// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;
/**
* This method prints an integer <paramref name="xx"/> in the specified base <paramref name="base"/> with optional sign <paramref name="sign"/>.
* It handles exceptions by checking for negative numbers and appending a '-' sign if necessary.
* Example:
* int number = -123;
* int base = 10;
* int sign = 1;
* printint(number, base, sign);
*/

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    /**
    * This method prints formatted output to the console.
    * It takes a format string <paramref name="fmt"/> and variable number of arguments to be formatted and printed.
    * Supported format specifiers are:
    * - %d: Print integer argument in base 10.
    * - %x or %p: Print integer argument in base 16.
    * - %s: Print string argument.
    * - %%: Print a percent sign.
    * If the format string is null, it will panic with an error message "null fmt".
    * 
    * Exceptions:
    * - If the format string is null, it will panic with an error message "null fmt".
    * 
    * Example:
    * cprintf("Hello, %s! The answer is %d.\n", "World", 42);
    */
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  /**
  * This method triggers a system panic with the provided error message <paramref name="s"/>.
  * It disables interrupts, releases console lock, and prints the error message along with the caller's program counters.
  * After setting the 'panicked' flag to 1, it freezes the system.
  * 
  * @param s The error message to be displayed during the panic.
  * @throws None
  * 
  * Example:
  * 
  * // Trigger a panic with a custom error message
  * char *errorMessage = "Kernel panic - Page fault detected";
  * panic(errorMessage);
  */
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    /**
    * This method updates the character display on the screen based on the input character <paramref name="c"/>.
    * It handles newline characters, backspace, and regular characters to update the cursor position and display accordingly.
    * Exceptions: This method may throw a panic if the cursor position is underflowing or overflowing.
    * Example:
    * cgaputc('A'); // Updates the screen display with the character 'A'
    */
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

/**
* This method outputs a character to the console.
* If the system is in a panicked state, the method will disable interrupts and enter an infinite loop.
* If the input character is a backspace, it will erase the previous character on the console.
* Otherwise, it will output the character to the console and update the cursor position.
* 
* @param c The character to be outputted to the console.
* @throws None
* 
* Example:
* consputc('A');
*/
void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

/**
* This method reads characters from the console using the provided function pointer <paramref name="getc"/> and performs specific actions based on the input character.
* Exceptions: None
* Example:
* consoleintr(getc);
*/
void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

/**
* This method reads characters from the console input and stores them in the destination buffer <paramref name="dst"/> up to a maximum of <paramref name="n"/> characters.
* If the end-of-file character (C('D')) is encountered, the method will stop reading and return the number of characters read so far.
* This method may return -1 if the process is killed while reading from the console input.
* 
* @param ip Pointer to the inode structure associated with the console input
* @param dst Pointer to the destination buffer where characters will be stored
* @param n Maximum number of characters to read
* @return The number of characters successfully read from the console input
* @exception Returns -1 if the process is killed while reading from the console input
* 
* Example:
* int n = consoleread(ip, dst, 100);
*/
int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

/**
* This method writes the characters from the buffer <paramref name="buf"/> to the console output.
* It unlocks the inode, acquires the console lock, writes each character to the console using consputc function, and then releases the console lock.
* Finally, it locks the inode again before returning the number of characters written.
* 
* @param ip Pointer to the inode structure
* @param buf Pointer to the character buffer to be written
* @param n Number of characters to write from the buffer
* @return The number of characters successfully written to the console
* @exception This method does not handle any exceptions explicitly.
* 
* Example:
* struct inode *ip;
* char buffer[] = "Hello, World!";
* int num_chars = consolewrite(ip, buffer, strlen(buffer));
*/
int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

/**
* Initializes the console by setting up the console lock and enabling keyboard interrupts.
* 
* Exceptions:
* - None
* 
* Example:
* consoleinit();
*/
void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}

