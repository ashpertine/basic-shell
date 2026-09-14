#include <limits.h>
#include <pwd.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define READ_TOK_LIMIT 64 // in number of tokens
#define CURR_DIR_LIMIT 32 // in characters

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int shell_cd(char **args);
int shell_help(char **args);
int shell_exit(char **args);

char *builtin_cmds[] = {"cd", "help", "exit"};
int (*builtin_func[])(char **) = {&shell_cd, &shell_help, &shell_exit};

size_t len_builtin_cmds() {
  return sizeof(builtin_cmds) / sizeof(builtin_cmds[0]);
}

char *get_prompt(void) {
  char *curr_dir = malloc(sizeof(char) * (PATH_MAX + 2));
  char *home_dir = getpwuid(getuid())->pw_dir;
  char *caret = "> ";

  if (getcwd(curr_dir, PATH_MAX) == NULL) {
    perror("getcwd() error");
    exit(EXIT_FAILURE);
  }

  size_t curr_dir_len = strlen(curr_dir);

  for (size_t j = 0; home_dir[j] != '\0' && curr_dir[j] == home_dir[j]; j++) {
    if (home_dir[j + 1] == '\0' &&
        (curr_dir[j + 1] == '\0' || curr_dir[j + 1] == '/')) {
      curr_dir[j] = '~';
      memmove(curr_dir, curr_dir + j, curr_dir_len - j + 1);
      break;
    }
  }

  curr_dir = strcat(curr_dir, caret);

  return curr_dir;
}

int shell_cd(char **args) {
  if (strcmp(args[0], "cd") != 0) {
    perror("basic-shell");
    exit(EXIT_FAILURE);
  }

  char *target_dir = args[1];
  if (target_dir == NULL) {
    struct passwd *info = getpwuid(getuid());
    target_dir = info->pw_dir;
  }

  if (chdir(target_dir) != 0) {
    perror("basic-shell");
  }

  return 1;
}

int shell_help(char **args) {
  printf("A basic shell, inspired by Stephen Brennan's LSH.\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (size_t i = 0; i < len_builtin_cmds(); i++) {
    printf("  %s\n", builtin_cmds[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

int shell_exit(char **args) { exit(EXIT_SUCCESS); }

char *sh_read_line(void) {
  char *prompt = NULL;
  char *line = NULL;

  prompt = get_prompt(); // now in heap
  line = readline(prompt);
  if (line && *line) {
    add_history(line);
  }

  free(prompt);
  prompt = NULL;

  return line;
}

char **sh_parse_line(char *line) {
  size_t read_token_limit = READ_TOK_LIMIT;
  size_t position = 0;
  char *curr_arg;
  const char *delim = " \t\n\r\a";

  char **args = malloc(sizeof(char *) * read_token_limit);

  if (args == NULL) {
    fprintf(stderr, "error allocating memory for parse line");
    exit(EXIT_FAILURE);
  }

  curr_arg = strtok(line, delim);
  while (curr_arg != NULL) {
    if (position >= read_token_limit) {
      read_token_limit *= 2;
      args = realloc(args, sizeof(char *) * read_token_limit);

      if (args == NULL) {
        fprintf(stderr, "error reallocating memory for parse line");
        exit(EXIT_FAILURE);
      }
    }

    args[position] = curr_arg;
    if (curr_arg != NULL)
      position++;
    curr_arg = strtok(NULL, delim);
  }

  // The NULL terminator is mandatory.
  // Without it, the function doesn't know where argv ends and will read
  // garbage memory for extra "arguments."
  args[position] = NULL;
  return args;
}

int launch_process(char **args) {
  pid_t pid;
  pid_t w_pid;
  int status;
  pid = fork();

  if (pid == 0) {
    status = execvp(args[0], args);
    if (status == -1) {
      perror("basic-shell");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("basic-shell");
    exit(EXIT_FAILURE);
  } else {
    do {
      w_pid = waitpid(pid, &status, 0);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

int execute_cmd(char **args) {
  if (args[0] == NULL) {
    return 1;
  }

  for (size_t i = 0; i < len_builtin_cmds(); i++) {
    if (strcmp(args[0], builtin_cmds[i]) == 0) {
      return builtin_func[i](args);
    }
  }

  return launch_process(args);
}

int main() {
  char *line;
  char **args;
  int status = 1;

  while (status) {
    line = sh_read_line();
    args = sh_parse_line(line);
    status = execute_cmd(args);

    free(line);
    line = NULL;
    free(args);
    args = NULL;
  }

  return EXIT_SUCCESS;
}
