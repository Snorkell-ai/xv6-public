#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method takes command line arguments and kills the processes with the specified PIDs.
* 
* @param argc The number of command line arguments.
* @param argv An array of strings containing the command line arguments.
* 
* @exception If the number of command line arguments is less than 2, a usage message is printed and the program exits.
* 
* Example:
* main(3, ["program_name", "123", "456"])
* This will kill the processes with PIDs 123 and 456.
*/
int
main(int argc, char **argv)
{
  int i;

  if(argc < 2){
    printf(2, "usage: kill pid...\n");
    exit();
  }
  for(i=1; i<argc; i++)
    kill(atoi(argv[i]));
  exit();
}
