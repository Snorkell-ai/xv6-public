// Shell.

#include "types.h"
#include "user.h"
#include "fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit();

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit();
    exec(ecmd->argv[0], ecmd->argv);
    printf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      printf(2, "open %s failed\n", rcmd->file);
      exit();
    }
    runcmd(rcmd->cmd);
/**
* This method executes the command specified in the input struct <paramref name="cmd"/>.
* It handles different types of commands such as EXEC, REDIR, LIST, PIPE, and BACK.
* If the command type is EXEC, it executes the command with arguments.
* If the command type is REDIR, it redirects input/output as specified.
* If the command type is LIST, it executes commands sequentially.
* If the command type is PIPE, it creates a pipe for communication between commands.
* If the command type is BACK, it runs the command in the background.
* 
* @param cmd The struct containing information about the command to be executed.
* @throws Exception if there are errors in executing the command or handling pipes.
* 
* Example:
* struct execcmd ecmd;
* ecmd.argv[0] = "ls";
* ecmd.argv[1] = "-l";
* ecmd.argv[2] = NULL;
* runcmd((struct cmd*)&ecmd);
*/

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit();
}

int
getcmd(char *buf, int nbuf)
{
  printf(2, "$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        printf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    /**
    * This method reads a command from the user input and stores it in the provided buffer.
    * 
    * @param buf Pointer to the buffer where the command will be stored.
    * @param nbuf Size of the buffer.
    * @return Returns 0 on success, -1 if EOF is encountered.
    * @throws None
    * 
    * Example:
    * char command[100];
    * int result = getcmd(command, 100);
    * if(result == 0) {
    *     printf("Command entered: %s\n", command);
    * } else {
    *     printf("EOF encountered.\n");
    * }
    */
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait();
  }
  exit();
}

void
panic(char *s)
{
  printf(2, "%s\n", s);
  /**
  * This method represents the main function of the program. It ensures that three file descriptors are open by opening the "console" file. It then reads and runs input commands, such as changing directories using the 'cd' command. The 'chdir' function is called by the parent process, not the child process. If the 'cd' command fails, an error message is displayed. The program also forks a new process to run commands and waits for the child process to finish before exiting.
  *
  * @exception If there is an error opening the "console" file or changing directories, an error message is displayed.
  *
  * Example:
  * main();
  */
  exit();
}

/**
* This method creates a new process by forking the current process.
* If the fork operation fails, it raises an exception with the message "fork".
* 
/**
* This method displays the error message <paramref name="s"/> and terminates the program.
* 
* @param s The error message to be displayed.
* @exception This method does not return a value as it terminates the program.
* 
* Example:
* 
* panic("Error: Out of memory");
*/
* @return The process ID of the newly created child process.
* 
* @throws Exception if the fork operation fails.
* 
* Example:
* int childProcessID = fork1();
*/
int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
/**
* This method creates and returns a new 'cmd' struct for executing a command.
* 
* @return A pointer to the newly created 'cmd' struct.
* @throws None
* 
* Example:
* struct cmd* command = execcmd();
*/
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  /**
  * This method creates a redirection command by redirecting the output of a subcommand to a file or file descriptor.
  * It takes in a subcommand, file name, error file name, mode, and file descriptor as parameters.
  * The redirection command struct is allocated memory and initialized with the provided parameters.
  * Returns a pointer to the created redirection command struct.
  * 
  * @param subcmd Pointer to the subcommand struct
  * @param file Name of the file for redirection
  * @param efile Name of the error file for redirection
  * @param mode Mode of redirection
  * @param fd File descriptor for redirection
  * @return Pointer to the created redirection command struct
  * @throws None
  * 
  * Example:
  * struct cmd *subcmd = createSubcommand();
  * char *file = "output.txt";
  * char *efile = "error.txt";
  * int mode = 1;
  * int fd = 2;
  * struct cmd *redirCommand = redircmd(subcmd, file, efile, mode, fd);
  */
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

/**
* This method creates a new pipe command by combining two existing commands.
* 
* @param left The left command to be executed in the pipeline.
* @param right The right command to be executed in the pipeline.
* @return A pointer to the newly created pipe command.
* @throws MemoryAllocationException if memory allocation fails.
* 
* Example:
* struct cmd *left_cmd = create_cmd();
* struct cmd *right_cmd = create_cmd();
* struct cmd *pipe_command = pipecmd(left_cmd, right_cmd);
*/
struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

/**
* This method creates a new list command by combining two existing commands.
* It takes two input commands <paramref name="left"/> and <paramref name="right"/> and returns a new list command.
* This function allocates memory for the new command and initializes its type as LIST.
* Any exceptions related to memory allocation should be handled appropriately.
*
* Example:
* struct cmd *cmd1 = createcmd();
* struct cmd *cmd2 = createcmd();
* struct cmd *listCommand = listcmd(cmd1, cmd2);
*/
struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing
/**
* This method creates a new back command with the given subcommand.
* 
* @param subcmd The subcommand to be set for the back command.
* @return A pointer to the newly created back command.
* @throws None
* 
* Example:
* struct cmd *subcmd = createSubCommand();
* struct cmd *backCommand = backcmd(subcmd);
*/

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  /**
  * This method retrieves the next token from the input string.
  * 
  * @param ps Pointer to the input string.
  * @param es Pointer to the end of the input string.
  * @param q Pointer to store the starting position of the token.
  * @param eq Pointer to store the ending position of the token.
  * @return The next token retrieved from the input string.
  * 
  * Exceptions:
  * - None
  * 
  * Example:
  * char *inputString = "example string";
  * char *start, *end;
  * char token = gettoken(&inputString, inputString + strlen(inputString), &start, &end);
  */
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
/**
* This method checks the next character in the input string pointed to by <paramref name="ps"/> and returns 1 if it matches any of the characters in the string <paramref name="toks"/>.
* If the character is a whitespace character, it skips over it and moves to the next character.
* 
* @param ps Pointer to the input string.
* @param es Pointer to the end of the input string.
* @param toks String containing characters to match against.
* 
* @return Returns 1 if the next character matches any character in <paramref name="toks"/>, 0 otherwise.
* 
* @exception None
* 
* Example:
* char *input_string = "abc def";
* char *ptr = input_string;
* char *end_ptr = input_string + 7; // Points to the end of the string
* char *tokens = " ";
* int result = peek(&ptr, end_ptr, tokens); // result will be 0 as 'a' is not in ' '
*/
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

/**
* This method parses a command string <paramref name="s"/> and returns a struct cmd pointer.
* It internally calls other functions like parseline, peek, panic, and nulterminate to parse the command string.
* If there are any leftovers in the input string after parsing, it prints an error message and panics with "syntax" error.
* Finally, it null terminates the parsed command before returning it.
* 
* @param s The command string to be parsed
* @return A struct cmd pointer representing the parsed command
* @throws "syntax" if there are leftovers in the input string after parsing
* 
* Example:
* struct cmd *parsedCmd = parsecmd("ls -l");
*/
struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    printf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

/**
* This method parses a command line string and returns a struct cmd pointer.
* It first calls the parsepipe method to parse the command line for pipes.
* If an ampersand '&' is found, it creates a background command using backcmd.
* If a semicolon ';' is found, it creates a list command using listcmd to combine commands.
* @param ps Pointer to the start of the command line string.
* @param es Pointer to the end of the command line string.
* @return A struct cmd pointer representing the parsed command line.
* @throws Exception if there are any issues with parsing the command line.
*
* Example:
* char *command = "ls -l | grep txt &";
* char *end = command + strlen(command);
* struct cmd *parsedCmd = parseline(&command, end);
*/
struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

/**
* This method parses a command pipeline from the input string <paramref name="ps"/> up to the end of the string <paramref name="es"/>.
* It returns a pointer to the parsed command structure.
* If a pipe symbol '|' is encountered, it recursively parses the next command in the pipeline.
* 
* @param ps Pointer to the input string.
* @param es Pointer to the end of the input string.
* @return Pointer to the parsed command structure.
* @throws Exception if there is an error in parsing the command pipeline.
* 
* Example:
* 
* char *input = "command1 | command2 | command3";
* char *end = input + strlen(input);
* struct cmd *parsedCmd = parsepipe(&input, end);
*/
struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    /**
    * This method parses redirections in the command structure.
    * It iterates through the input string and handles redirections based on the tokens encountered.
    * Supported redirection tokens are '<', '>', and '+', where '<' represents input redirection, '>' represents output redirection, and '+' represents appending output redirection.
    * If a file is missing for redirection, it will throw an exception with the message "missing file for redirection".
    * 
    * @param cmd The command structure to be modified with redirection information.
    * @param ps Pointer to the input string.
    * @param es Pointer to the end of the input string.
    * @return The modified command structure after parsing redirections.
    * 
    * @throws Exception if a file is missing for redirection.
    * 
    * Example:
    * struct cmd* command = parseredirs(initial_cmd, &input_string_ptr, end_of_string_ptr);
    */
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

/**
* This method parses and executes a command provided as input string pointers <paramref name="ps"/> and <paramref name="es"/>.
* It handles parsing the command arguments and redirections, and returns a struct cmd pointer.
* Exceptions: 
/**
* This method parses a block of code starting from the specified pointer <paramref name="ps"/> and ending at the pointer <paramref name="es"/>.
* It expects the block to be enclosed within parentheses and will parse the block accordingly.
* It returns a struct cmd pointer representing the parsed block.
*
* Exceptions:
* - If the opening parenthesis is not found, it will throw a panic with the message "parseblock".
* - If the closing parenthesis is missing in the syntax, it will throw a panic with the message "syntax - missing )".
*
* Example:
* struct cmd *cmd = parseblock(&ps, es);
*/
* - If there is a syntax error in the input command, it will panic with an error message "syntax".
* - If there are too many arguments provided in the command, it will panic with an error message "too many args".
* Example:
* struct cmd* cmd = parseexec(ps, es);
*/
struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

/**
* This method null terminates the given command structure to ensure proper handling of strings.
* It iterates through the different types of commands and null terminates them accordingly.
* 
* @param cmd The command structure to be null terminated.
* @return The null terminated command structure.
* @exception None
* 
* Example:
* struct cmd* command = createCommand();
* command = nulterminate(command);
*/
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
