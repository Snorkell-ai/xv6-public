#include "types.h"
#include "x86.h"

/**
* This method sets the first <paramref name="n"/> bytes of the memory area pointed to by <paramref name="dst"/> to the specified <paramref name="c"/> value.
* 
* @param dst Pointer to the memory area to be filled.
* @param c Value to be set. Only the least significant byte of <paramref name="c"/> is used.
* @param n Number of bytes to be set to the value <paramref name="c"/>.
* 
* @exception None
* 
* Example:
* void* ptr = malloc(10 * sizeof(int));
* memset(ptr, 0, 10);
*/
void*
memset(void *dst, int c, uint n)
{
  if ((int)dst%4 == 0 && n%4 == 0){
    c &= 0xFF;
    stosl(dst, (c<<24)|(c<<16)|(c<<8)|c, n/4);
  } else
    stosb(dst, c, n);
  return dst;
}

/**
* This method compares the memory blocks pointed to by <paramref name="v1"/> and <paramref name="v2"/> up to <paramref name="n"/> bytes.
* It returns an integer less than, equal to, or greater than zero if the first <paramref name="n"/> bytes of <paramref name="v1"/> are found to be less than, equal to, or greater than the first <paramref name="n"/> bytes of <paramref name="v2"/>.
*
* @param v1 Pointer to the first memory block to compare.
* @param v2 Pointer to the second memory block to compare.
* @param n Number of bytes to compare.
* @return An integer less than, equal to, or greater than zero if the first <paramref name="n"/> bytes of <paramref name="v1"/> are found to be less than, equal to, or greater than the first <paramref name="n"/> bytes of <paramref name="v2"/>.
* @throws None
*
* Example:
* <code>
* const char str1[] = "Hello";
* const char str2[] = "World";
* int result = memcmp(str1, str2, 5);
* // result will be a negative value since 'H' is less than 'W' in ASCII
* </code>
*/
int
memcmp(const void *v1, const void *v2, uint n)
{
  const uchar *s1, *s2;

  s1 = v1;
  s2 = v2;
  while(n-- > 0){
    if(*s1 != *s2)
      return *s1 - *s2;
    s1++, s2++;
  }

  return 0;
}

/**
* This method copies a block of memory from a source location to a destination location, handling overlapping memory regions.
* 
* @param dst Pointer to the destination memory location where the content is to be copied.
* @param src Pointer to the source memory location from where the content is to be copied.
* @param n Number of bytes to copy.
* 
* @return Pointer to the destination memory location.
* 
* @exception This method does not handle any specific exceptions.
* 
* Example:
* 
* void* source = malloc(10 * sizeof(char));
* void* destination = malloc(10 * sizeof(char));
* 
* // Fill source with data
* strcpy(source, "Hello");
* 
* // Copy data from source to destination
* memmove(destination, source, 5);
*/
void*
memmove(void *dst, const void *src, uint n)
{
  const char *s;
  char *d;

  s = src;
  d = dst;
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else
    while(n-- > 0)
      *d++ = *s++;

  return dst;
}

/**
* This method copies a block of memory from the source address <paramref name="src"/> to the destination address <paramref name="dst"/> for a total of <paramref name="n"/> bytes.
* It is important to note that this function does not check for buffer overflows, so care must be taken to ensure that the memory regions do not overlap.
*
* @param dst The destination address where the memory will be copied to.
* @param src The source address from where the memory will be copied.
* @param n The number of bytes to be copied from the source to the destination.
* @return A pointer to the destination address after the memory has been copied.
* @throws None
*
* Example:
* void* destination = malloc(10 * sizeof(int));
* int source[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
* memcpy(destination, source, 10 * sizeof(int));
*/
void*
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

/**
* This method compares the first n characters of two strings pointed to by <paramref name="p"/> and <paramref name="q"/>.
* If the characters are equal, it continues to the next character until n becomes 0 or one of the strings ends.
* If n becomes 0, it returns 0 indicating the strings are equal up to n characters.
* If the strings are not equal, it returns the difference between the ASCII values of the first differing characters.
* 
* @param p Pointer to the first string to compare.
* @param q Pointer to the second string to compare.
* @param n Number of characters to compare.
* 
* @return Returns an integer less than, equal to, or greater than zero if the first n characters of p are found, respectively, to be less than, to match, or be greater than the first n characters of q.
* 
* @exception None
* 
* Example:
* 
* const char *str1 = "hello";
* const char *str2 = "world";
* int result = strncmp(str1, str2, 3);
* // result will be 0 as the first 3 characters 'hel' are equal in both strings.
*/
int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

/**
* This method copies the first n characters of the string t to the string s.
* If the length of t is less than n, null characters are appended to s until a total of n characters have been written.
* 
* @param s The destination string where the characters will be copied to.
* @param t The source string from which characters will be copied.
* @param n The maximum number of characters to be copied.
* 
* @return A pointer to the destination string s.
* 
* @exception None
* 
* Example:
* char dest[10];
* char source[] = "Hello";
* strncpy(dest, source, 3);
* // After this call, dest will contain "Hel\0\0\0\0"
*/
char*
strncpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  while(n-- > 0 && (*s++ = *t++) != 0)
    ;
  while(n-- > 0)
    *s++ = 0;
  return os;
}

/**
* This method safely copies the string <paramref name="t"/> to the buffer <paramref name="s"/> with a maximum length of <paramref name="n"/> characters.
* If the length <paramref name="n"/> is less than or equal to 0, the function returns the original string <paramref name="s"/>.
* If the length <paramref name="n"/> is greater than 0, it copies characters from <paramref name="t"/> to <paramref name="s"/> until it reaches the maximum length or encounters a null terminator.
* The copied string in <paramref name="s"/> is null-terminated.
* 
* @param s The destination buffer where the string will be copied.
* @param t The source string that will be copied.
* @param n The maximum number of characters to copy from <paramref name="t"/> to <paramref name="s"/>.
* @return A pointer to the original buffer <paramref name="s"/>.
*/
char*
safestrcpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  if(n <= 0)
    return os;
  while(--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

/**
* This method calculates the length of the input string <paramref name="s"/> by iterating through each character until the null terminator is reached.
* 
* @param s The input string for which the length needs to be calculated.
* @return The length of the input string <paramref name="s"/>.
* @throws None
* 
* Example:
* 
* const char *str = "Hello, World!";
* int length = strlen(str);
* // After execution, length will contain the value 13
*/
int
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

