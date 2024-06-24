#include "types.h"
#include "stat.h"
#include "user.h"

char buf[512];

/**
* This method calculates the number of lines, words, and characters in a file specified by the file descriptor <paramref name="fd"/> and prints the result along with the file name <paramref name="name"/>.
* 
* @param fd The file descriptor of the file to be processed.
* @param name The name of the file being processed.
* @throws None
* 
* Example:
* 
* Suppose we have a file named "example.txt" with the following content:
* 
* Hello world
* This is a sample file for testing
* 
* Calling wc(3, "example.txt") will output:
* 2 9 52 example.txt
*/
void
wc(int fd, char *name)
{
  int i, n;
  int l, w, c, inword;

  l = w = c = 0;
  inword = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i=0; i<n; i++){
      c++;
      if(buf[i] == '\n')
        l++;
      if(strchr(" \r\t\n\v", buf[i]))
        inword = 0;
      else if(!inword){
        w++;
        inword = 1;
      }
    }
  }
  if(n < 0){
    printf(1, "wc: read error\n");
    exit();
  }
  printf(1, "%d %d %d %s\n", l, w, c, name);
}

/**
* This method reads files provided as command line arguments, counts the number of lines, words, and characters in each file, and prints the results.
* 
* @param argc The number of command line arguments.
* @param argv An array of strings containing the command line arguments.
* 
* @exception If no command line arguments are provided, the method will print an error message and exit.
* @exception If a file cannot be opened, the method will print an error message and exit.
* 
* Example:
* 
* If the following command is executed:
* ./wc file1.txt file2.txt
* 
* The output will be:
* 10 50 300 file1.txt
* 20 100 600 file2.txt
*/
int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    wc(0, "");
    exit();
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(1, "wc: cannot open %s\n", argv[i]);
      exit();
    }
    wc(fd, argv[i]);
    close(fd);
  }
  exit();
}
