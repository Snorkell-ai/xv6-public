#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method takes command line arguments and prints them with spaces in between, except for the last argument which is followed by a new line.
* 
* Exceptions:
* - None
* 
* Example:
* If the command line arguments are "hello", "world", "example", the output will be: "hello world example\n"
*/
int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++)
    printf(1, "%s%s", argv[i], i+1 < argc ? " " : "\n");
  exit();
}
