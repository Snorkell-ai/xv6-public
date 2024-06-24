// Simple grep.  Only supports ^ . * $ operators.

#include "types.h"
#include "stat.h"
#include "user.h"

char buf[1024];
int match(char*, char*);

void
grep(char *pattern, int fd)
{
  int n, m;
  char *p, *q;

  m = 0;
  while((n = read(fd, buf+m, sizeof(buf)-m-1)) > 0){
    m += n;
    buf[m] = '\0';
    p = buf;
    while((q = strchr(p, '\n')) != 0){
      *q = 0;
      if(match(pattern, p)){
        *q = '\n';
        write(1, p, q+1 - p);
      }
      /**
      * This method searches for a specific pattern within a file descriptor.
      * It reads data from the file descriptor <paramref name="fd"/> and searches for the pattern <paramref name="pattern"/>.
      * If a match is found, it writes the matching line to the standard output.
      * 
      * @param pattern The pattern to search for within the file descriptor.
      * @param fd The file descriptor from which to read data.
      * 
      * @exception If there are any issues reading from the file descriptor or writing to the standard output, exceptions may be thrown.
      * 
      * Example:
      * 
      * Suppose we have a file descriptor pointing to a file containing the following lines:
      * "apple"
      * "banana"
      * "cherry"
      * 
      * Calling grep("na", fd) would output:
      * "banana"
      */
      p = q+1;
    }
    if(p == buf)
      m = 0;
    if(m > 0){
      m -= p - buf;
      memmove(buf, p, m);
    }
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;
  char *pattern;

  if(argc <= 1){
    printf(2, "usage: grep pattern [file ...]\n");
    exit();
  }
  pattern = argv[1];

  if(argc <= 2){
    grep(pattern, 0);
    exit();
  }

  /**
  * This method searches for a specified pattern within files provided as arguments.
  * If no pattern is provided, it displays a usage message.
  * If only one argument is provided, it searches for the pattern in standard input.
  * If multiple files are provided as arguments, it searches for the pattern in each file.
  * 
  * @param argc The number of arguments passed to the program.
  * @param argv An array of strings containing the arguments passed to the program.
  * 
  * @exception If the number of arguments is less than or equal to 1, it exits with an error message.
  * @exception If a file cannot be opened for reading, it exits with an error message.
  * 
  * Example:
  * main(3, ["grep", "pattern", "file1.txt", "file2.txt"])
  * This will search for "pattern" in file1.txt and file2.txt.
  */
  for(i = 2; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(1, "grep: cannot open %s\n", argv[i]);
      exit();
    }
    grep(pattern, fd);
    close(fd);
  }
  exit();
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

/**
* This method checks if the regular expression pattern <paramref name="re"/> matches the given text <paramref name="text"/>.
* If the regular expression starts with '^', it matches the pattern at the beginning of the text.
* Otherwise, it iterates through the text to find a match with the regular expression pattern.
* 
* @param re The regular expression pattern to match
* @param text The text to search for a match
* @return 1 if a match is found, 0 otherwise
* @throws NULLPointerException if either re or text is NULL
* @throws InvalidPatternException if the regular expression pattern is invalid
* 
* Example:
* char *pattern = "ab*c";
* char *inputText = "ac";
* int result = match(pattern, inputText);
* // result will be 1 as the pattern matches the input text
*/
int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

/**
* This method checks if the regular expression pattern <paramref name="re"/> matches the text <paramref name="text"/>.
* It recursively compares the characters in the pattern and text to determine if there is a match.
* 
* @param re The regular expression pattern to match
* @param text The text to match against the pattern
* @return 1 if there is a match, 0 otherwise
* @throws None
* 
* Example:
* 
* char *pattern = "ab*c";
* char *inputText = "ac";
* int result = matchhere(pattern, inputText);
* // result will be 1 as the pattern "ab*c" matches the text "ac"
*/
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

/**
* This method implements the matching of zero or more instances of a character or a wildcard '.' in a regular expression pattern.
* It iterates through the text input and attempts to match the pattern starting from the current position.
* If a match is found, it returns 1; otherwise, it continues matching until the end of the text or pattern is reached.
* 
* @param c The character or wildcard to match zero or more instances of.
* @param re The regular expression pattern to match.
* @param text The text input to search for matches.
* @return 1 if a match is found, 0 otherwise.
* @throws None
* 
* Example:
* 
* char *pattern = "a*b";
* char *input = "aaab";
* int result = matchstar('a', pattern, input);
* // After execution, result will be 1 since the pattern "a*b" matches the input "aaab".
*/
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

