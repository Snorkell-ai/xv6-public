#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method creates a symbolic link between two files specified by the command line arguments.
* 
* @param argc The number of command line arguments passed to the program.
* @param argv An array of strings containing the command line arguments.
* 
* @exception If the number of command line arguments is not equal to 3, it prints an error message and exits.
* @exception If linking the files specified by the command line arguments fails, it prints an error message.
* 
* Example:
* 
* If the program is executed with the following command line arguments:
* ./program file1 file2
* 
* It will attempt to create a symbolic link between file1 and file2. If successful, it will exit without any output. If linking fails, it will print an error message.
*/
int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf(2, "Usage: ln old new\n");
    exit();
  }
  if(link(argv[1], argv[2]) < 0)
    printf(2, "link %s %s: failed\n", argv[1], argv[2]);
  exit();
}
