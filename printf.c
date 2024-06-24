#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method writes a single character <paramref name="c"/> to the file descriptor <paramref name="fd"/>.
* 
* @param fd The file descriptor to write to.
* @param c The character to write.
* @throws None
* 
* Example:
* <code>
* putc(1, 'A');
* </code>
*/
static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

/**
* This method prints an integer value to the specified file descriptor <paramref name="fd"/> in the given base <paramref name="base"/>.
* If <paramref name="sgn"/> is set to 1 and the input value <paramref name="xx"/> is negative, the output will include a negative sign.
* The function uses the provided base to convert the integer value into a string representation.
* 
* @param fd The file descriptor where the output will be printed.
* @param xx The integer value to be printed.
* @param base The base for the conversion (e.g., 10 for decimal).
* @param sgn Flag indicating whether to include the sign for negative values (1 for yes, 0 for no).
* 
* @exception None
* 
* Example:
* int num = -123;
* printint(1, num, 10, 1); // Output: -123
*/
static void
printint(int fd, int xx, int base, int sgn)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

/**
* This method implements a simplified version of the printf function in C, allowing for formatted output to a specified file descriptor.
* 
* @param fd The file descriptor where the output will be written.
* @param fmt The format string containing the format specifiers and text to be printed.
* @param ... Additional arguments corresponding to the format specifiers in the format string.
* 
* @exception This method does not perform any input validation, so passing incorrect format specifiers or arguments can lead to unexpected behavior or crashes.
* 
* Example:
* 
* int num = 42;
* char* str = "Hello, World!";
* printf(1, "The answer is %d and the message is: %s\n", num, str);
*/
void
printf(int fd, const char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;

  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(fd, *ap, 10, 1);
        ap++;
      } else if(c == 'x' || c == 'p'){
        printint(fd, *ap, 16, 0);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, *ap);
        ap++;
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
}
