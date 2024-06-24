#include "types.h"
#include "x86.h"
#include "defs.h"
#include "kbd.h"

/**
* This method reads a character from the keyboard input and returns the corresponding ASCII value.
* It handles key presses, releases, and special keys like shift, control, and caps lock.
* 
* @return The ASCII value of the character read from the keyboard input. Returns -1 if no valid character is read.
* 
* @exception If an error occurs while reading the keyboard input or processing the key, -1 is returned.
* 
* Example:
* int key = kbdgetc();
* if(key != -1) {
*     printf("Key pressed: %c\n", key);
* } else {
*     printf("Error reading keyboard input.\n");
* }
*/
int
kbdgetc(void)
{
  static uint shift;
  static uchar *charcode[4] = {
    normalmap, shiftmap, ctlmap, ctlmap
  };
  uint st, data, c;

  st = inb(KBSTATP);
  if((st & KBS_DIB) == 0)
    return -1;
  data = inb(KBDATAP);

  if(data == 0xE0){
    shift |= E0ESC;
    return 0;
  } else if(data & 0x80){
    // Key released
    data = (shift & E0ESC ? data : data & 0x7F);
    shift &= ~(shiftcode[data] | E0ESC);
    return 0;
  } else if(shift & E0ESC){
    // Last character was an E0 escape; or with 0x80
    data |= 0x80;
    shift &= ~E0ESC;
  }

  shift |= shiftcode[data];
  shift ^= togglecode[data];
  c = charcode[shift & (CTL | SHIFT)][data];
  if(shift & CAPSLOCK){
    if('a' <= c && c <= 'z')
      c += 'A' - 'a';
    else if('A' <= c && c <= 'Z')
      c += 'a' - 'A';
  }
  return c;
}

/**
* This method handles keyboard interrupts by calling the consoleintr function with the kbdgetc function as a parameter.
* 
* @exception This method does not throw any exceptions.
* 
* @example
* kbdintr();
* // This will handle keyboard interrupts by calling the consoleintr function with the kbdgetc function.
*/
void
kbdintr(void)
{
  consoleintr(kbdgetc);
}
