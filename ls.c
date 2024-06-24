#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

/**
* This method extracts the file name from the given file path.
* 
* @param path The file path from which the file name needs to be extracted.
* @return A pointer to the extracted file name.
* @exception If the length of the file name exceeds the maximum directory size, it returns the original pointer without padding.
* 
* Example:
* char* filePath = "/home/user/example.txt";
* char* fileName = fmtname(filePath);
* // fileName will contain "example.txt"
*/
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

/**
* This method lists the contents of the specified directory path.
* It takes a string <paramref name="path"/> as input, representing the directory path to be listed.
* If the directory cannot be opened or its status cannot be determined, appropriate error messages are displayed.
* For each file or subdirectory within the specified directory, information such as name, type, inode number, and size is printed.
* If the path exceeds the buffer size, an error message is displayed.
* This method does not handle exceptions explicitly but relies on system calls for file operations.
* Example:
* ls("/path/to/directory");
*/
void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

/**
* This method executes the ls command on the specified directories or files provided as arguments.
* 
* @param argc The number of arguments passed to the program.
* @param argv An array of strings containing the arguments passed to the program.
* 
* @exception If no arguments are provided, the method will execute ls(".") and exit.
* 
* Example:
* main(3, ["program", "dir1", "dir2"])
* This will execute ls("dir1") and ls("dir2") sequentially.
*/
int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit();
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit();
}
