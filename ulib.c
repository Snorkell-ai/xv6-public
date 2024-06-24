#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

/**
* This method copies the string <paramref name="t"/> to the memory location pointed by <paramref name="s"/>.
* It returns a pointer to the beginning of the copied string.
* 
* @param s Pointer to the destination memory location where the string will be copied.
* @param t Pointer to the source string that will be copied.
* @return Pointer to the beginning of the copied string.
* @throws None
* 
* Example:
* char dest[20];
* char source[] = "Hello, World!";
* char *result = strcpy(dest, source);
*/
char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

/**
* This method compares two strings <paramref name="p"/> and <paramref name="q"/> lexicographically.
* It returns an integer less than, equal to, or greater than zero if the first string is found to be less than, equal to, or greater than the second string, respectively.
* 
* @param p The first string to be compared.
* @param q The second string to be compared.
* @return An integer less than, equal to, or greater than zero if the first string is found to be less than, equal to, or greater than the second string, respectively.
* 
* @exception None
* 
* Example:
* const char *str1 = "hello";
* const char *str2 = "world";
* int result = strcmp(str1, str2);
* // result will be a negative value since "hello" is lexicographically less than "world"
*/
int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

/**
* This method calculates the length of the input string <paramref name="s"/>.
* It iterates through the characters of the string until it reaches the null terminator.
* 
* @param s The input string for which the length is to be calculated.
* @return The length of the input string.
* @throws None
* 
* Example:
* const char *str = "Hello, World!";
* uint length = strlen(str);
* // After execution, length will contain the value 13
*/
uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

/**
* This method fills the first <paramref name="n"/> bytes of the memory area pointed to by <paramref name="dst"/> with the constant byte <paramref name="c"/>.
* 
* @param dst Pointer to the memory area to be filled.
* @param c   Value to be set. The value is passed as an int, but it is converted to unsigned char before being written.
* @param n   Number of bytes to be set to the value.
* 
* @return Pointer to the memory area <paramref name="dst"/>.
* 
* @exception None
* 
* Example:
* 
* void* ptr = malloc(10 * sizeof(int));
* memset(ptr, 0, 10 * sizeof(int));
*/
void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

/**
* This method searches for the first occurrence of the character <paramref name="c"/> in the string <paramref name="s"/>.
* If the character is found, it returns a pointer to the location of the character in the string.
* If the character is not found, it returns NULL.
* 
* @param s The input string to search within.
* @param c The character to search for within the string.
* @return A pointer to the first occurrence of the character in the string, or NULL if not found.
* 
* @exception None
* 
* @example
* const char *inputString = "Hello, World!";
* char searchChar = 'o';
* char *result = strchr(inputString, searchChar);
* if (result != NULL) {
*     printf("Character '%c' found at position: %ld\n", searchChar, result - inputString);
* } else {
*     printf("Character '%c' not found in the input string.\n", searchChar);
* }
*/
char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

/**
* This method reads a line of input from the standard input and stores it in the buffer provided.
* The input is terminated by a newline character or carriage return, and the buffer is null-terminated.
* 
* @param buf The buffer where the input line will be stored.
* @param max The maximum number of characters that can be read into the buffer.
* @return A pointer to the buffer containing the input line.
* @throws None
* 
* Example:
* char input[100];
* char* line = gets(input, 100);
*/
char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

/**
* This method retrieves information about the file pointed to by <paramref name="n"/> and stores it in the structure pointed to by <paramref name="st"/>.
* If successful, it returns 0; otherwise, it returns -1.
* 
* @param n The path to the file for which information is to be retrieved.
* @param st A pointer to a struct stat where the file information will be stored.
* 
* @return 0 if successful, -1 if an error occurs.
* 
* @exception If an error occurs during opening, retrieving file information, or closing the file, an appropriate error code is returned.
* 
* Example:
* struct stat fileStat;
* int result = stat("example.txt", &fileStat);
* if (result == 0) {
*     // File information successfully retrieved
* } else {
*     // Error occurred
* }
*/
int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

/**
* This method converts the input string <paramref name="s"/> to an integer using the atoi algorithm.
* 
* @param s The input string to be converted to an integer.
* @return The integer value converted from the input string.
* @throws None
* 
* Example:
* <code>
* const char *input = "12345";
* int result = atoi(input);
* // The value of result will be 12345
* </code>
*/
int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

/**
* This method copies a block of memory from the source address <paramref name="vsrc"/> to the destination address <paramref name="vdst"/> with a size of <paramref name="n"/> bytes.
* 
* @param vdst Pointer to the destination address where the content is to be copied.
* @param vsrc Pointer to the source address from where the content is to be copied.
* @param n Number of bytes to be copied.
* @return Pointer to the destination address.
* @throws None
*
* Example:
* void* destination = malloc(10 * sizeof(char));
* const char* source = "Hello";
* memmove(destination, source, 5);
*/
void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}
