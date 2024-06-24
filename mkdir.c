#include "types.h"
#include "stat.h"
#include "user.h"

/**
* This method takes command line arguments and creates directories based on the input provided.
* 
* @param argc The number of command line arguments.
* @param argv An array of strings containing the command line arguments.
* 
* @exception If the number of command line arguments is less than 2, it will print an error message and exit.
* @exception If a directory creation fails, it will print an error message and stop creating directories.
* 
* Example:
* If the command line input is "mkdir folder1 folder2 folder3", this method will attempt to create directories named "folder1", "folder2", and "folder3".
* 
* Note: This method does not handle cases where the directory already exists or other potential errors related to directory creation.
*/
int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    printf(2, "Usage: mkdir files...\n");
    exit();
  }

  for(i = 1; i < argc; i++){
    if(mkdir(argv[i]) < 0){
      printf(2, "mkdir: %s failed to create\n", argv[i]);
      break;
    }
  }

  exit();
}
