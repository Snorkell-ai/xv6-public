#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method deletes the specified files provided as arguments in the command line.
* 
* @param argc The number of arguments provided in the command line.
* @param argv An array of strings containing the arguments provided in the command line.
* 
* @exception If the number of arguments is less than 2, it will print an error message and exit.
* @exception If unlink operation fails for any file, it will print an error message and stop further deletions.
* 
* Example:
* If the command line input is: ./executable file1.txt file2.txt
* This method will attempt to delete file1.txt and file2.txt. If any deletion fails, it will stop further deletions.
*/
int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    printf(2, "Usage: rm files...\n");
    exit();
  }

  for(i = 1; i < argc; i++){
    if(unlink(argv[i]) < 0){
      printf(2, "rm: %s failed to delete\n", argv[i]);
      break;
    }
  }

  exit();
}
