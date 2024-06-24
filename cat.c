#include "types.h"
#include "stat.h"
#include "user.h"

char buf[512];

/**
* This method reads data from a file descriptor <paramref name="fd"/> and writes it to standard output.
* It reads data in chunks and writes each chunk until the end of the file is reached.
* If there is an error while writing, it prints "cat: write error" and exits.
* If there is an error while reading, it prints "cat: read error" and exits.
* 
* @param fd The file descriptor from which data will be read.
* @throws None
* 
* Example:
* 
* int file_descriptor = open("example.txt", O_RDONLY);
* if(file_descriptor < 0){
*     printf(1, "cat: file open error\n");
*     exit();
* }
* cat(file_descriptor);
* close(file_descriptor);
*/
void
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(1, buf, n) != n) {
      printf(1, "cat: write error\n");
      exit();
    }
  }
  if(n < 0){
    printf(1, "cat: read error\n");
    exit();
  }
}

/**
* This method reads and outputs the contents of the files specified in the command line arguments.
* It takes in the number of command line arguments <paramref name="argc"/> and an array of strings <paramref name="argv"/>.
* If no file is specified, it reads from standard input.
* If a file cannot be opened, it prints an error message.
* It then reads the contents of each file using the file descriptor and outputs them to standard output.
* Finally, it closes the file descriptor and exits.
*
* @param argc The number of command line arguments.
* @param argv An array of strings containing the command line arguments.
*
* @exception If a file specified in the command line arguments cannot be opened, an error message is printed and the program exits.
*
* Example:
* If the command line arguments are: ./program file1.txt file2.txt
* The method will read the contents of file1.txt and file2.txt and output them to standard output.
*/
int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    cat(0);
    exit();
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(1, "cat: cannot open %s\n", argv[i]);
      exit();
    }
    cat(fd);
    close(fd);
  }
  exit();
}
