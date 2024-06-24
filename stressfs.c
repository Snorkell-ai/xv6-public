// Demonstrate that moving the "acquire" in iderw after the loop that
// appends to the idequeue results in a race.

// For this to work, you should also add a spin within iderw's
// idequeue traversal loop.  Adding the following demonstrated a panic
// after about 5 runs of stressfs in QEMU on a 2.1GHz CPU:
//    for (i = 0; i < 40000; i++)
//      asm volatile("");

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

/**
* This method demonstrates a stress test scenario by creating multiple child processes to write and read data to a file in a loop.
* It starts by initializing variables and data, then forks multiple child processes to perform write operations concurrently.
* After writing data to the file, it reads the data back and waits for all child processes to finish before exiting.
* This method is intended for stress testing file system operations under heavy load.
*
* @exception This method may encounter exceptions related to file operations, such as file not found or permission denied.
*
* Example:
* main(argc, argv) {
*   int fd, i;
*   char path[] = "stressfs0";
*   char data[512];
*
*   printf(1, "stressfs starting\n");
*   memset(data, 'a', sizeof(data));
*
*   for(i = 0; i < 4; i++)
*     if(fork() > 0)
*       break;
*
*   printf(1, "write %d\n", i);
*
*   path[8] += i;
*   fd = open(path, O_CREATE | O_RDWR);
*   for(i = 0; i < 20; i++)
*     write(fd, data, sizeof(data));
*   close(fd);
*
*   printf(1, "read\n");
*
*   fd = open(path, O_RDONLY);
*   for (i = 0; i < 20; i++)
*     read(fd, data, sizeof(data));
*   close(fd);
*
*   wait();
*
*   exit();
* }
*/
int
main(int argc, char *argv[])
{
  int fd, i;
  char path[] = "stressfs0";
  char data[512];

  printf(1, "stressfs starting\n");
  memset(data, 'a', sizeof(data));

  for(i = 0; i < 4; i++)
    if(fork() > 0)
      break;

  printf(1, "write %d\n", i);

  path[8] += i;
  fd = open(path, O_CREATE | O_RDWR);
  for(i = 0; i < 20; i++)
//    printf(fd, "%d\n", i);
    write(fd, data, sizeof(data));
  close(fd);

  printf(1, "read\n");

  fd = open(path, O_RDONLY);
  for (i = 0; i < 20; i++)
    read(fd, data, sizeof(data));
  close(fd);

  wait();

  exit();
}
