#define  _GNU_SOURCE
#include <stdio.h>
#include "tokens.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h> 


const int MAX_LINE_LENGTH_CHARS = 256; //255 + 1 for null char

char ** prev; //Stores the previous command line

char ** tokens; // Store tokens for current command

void source_cmd(char ** cmd);

void help();

void cd(char * path);

void update_prev(char ** tokens);

void print_tokens(char ** tokens);

void iterate_over_tokens(char ** tokens);

void shell_exit();

void shell_prev();

void on_cmd_error(char * path);

void run_command(char * path, char ** args);

void run_one_sequence(char ** tokens, char * operator, char * redirect_file);

char ** copy_subsequence(char ** arr, int start, int end);

int handle_pipes(char ** tokens);

void pipe_child(char ** sequence_one, char ** sequence_two); 

/*
 * Main loop for getting input into the shell and parsing the tokens.
 */
int main(int argc, char ** argv) {

  char input[MAX_LINE_LENGTH_CHARS];

  printf("Welcome to mini-shell.\n");
  while (1) {
    printf("shell $ ");
    if (fgets(input, MAX_LINE_LENGTH_CHARS, stdin) == NULL) { //Get one line of user input
      shell_exit();
    }
    tokens = get_tokens(input); //Split input into tokens
    update_prev(tokens); //Store this command for future use by the prev built-in
    iterate_over_tokens(tokens); //Execute all subcommands in this command line
  }

  return 0;
}

/* 
 * Execute one command - check if it is a builtin and if not execute it using execvp.
 */
void run_one_sequence(char ** tokens, char * operator, char * redirect_file) {
  if (strcmp(tokens[0], "exit") == 0) {
      shell_exit();
  } else if (strcmp(tokens[0], ";") == 0) {
      return;
  } 
  int id = fork();
  if (id == 0) { //If child
    // Replace STDIN and out as appropriate
    if (strcmp(operator, "<") == 0) { // if input redirection
      assert(close(0) != -1);
      assert(open(redirect_file, O_RDONLY) == 0);
    } else if (strcmp(operator, ">") == 0) { // if output redirection
      assert(close(1) != -1);
      assert(open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644) == 1);
    }

    // Run command
    if (strcmp(tokens[0], "prev") == 0) {
      shell_prev();
    } else if (strcmp(tokens[0], "help") == 0) {
      help();
    } else if (strcmp(tokens[0], "source") == 0) {
      source_cmd(tokens);
    } else if (strcmp(tokens[0], "cd") == 0) {
      cd(tokens[1]);
    } else {
      execvp(tokens[0], tokens); // Execute command
      on_cmd_error(tokens[0]); // Run on error
    }

    free_tokens(tokens);

    exit(0);
  } else {
    wait(NULL); //If parent, wait for child process to exit
  }
}

/*
 * Iterate over an array of tokens, split into subsequences and recursivly execute if necessary.
 */
void iterate_over_tokens(char ** tokens) {
  char ** current = tokens;
  char ** subsequence;
  int sequence_start_pointer = 0;
  int current_index_pointer = 0;

  if (handle_pipes(tokens)) {
    return;
  }

  while ( * current != NULL) {
    if (strcmp( * current, "<") == 0) {
      subsequence = copy_subsequence(tokens, sequence_start_pointer, current_index_pointer);
      run_one_sequence(subsequence, "<", * (current + 1));
      free(subsequence);
      ++current;
      ++current_index_pointer; 
      sequence_start_pointer = current_index_pointer + 1;
    }
    else if (strcmp( * current, ">") == 0) {
      subsequence = copy_subsequence(tokens, sequence_start_pointer, current_index_pointer);
      run_one_sequence(subsequence, ">", * (current + 1));
      free(subsequence); 
      ++current;
      ++current_index_pointer; 
      sequence_start_pointer = current_index_pointer + 1;
    }
    else if (strcmp( * current, ";") == 0) {
      subsequence = copy_subsequence(tokens, sequence_start_pointer, current_index_pointer);
      run_one_sequence(subsequence, "", "");
      free(subsequence);
      sequence_start_pointer = current_index_pointer + 1;
    }
    ++current_index_pointer;
    ++current;
  }

  if (sequence_start_pointer < current_index_pointer) {
    subsequence = copy_subsequence(tokens, sequence_start_pointer, current_index_pointer);

    int len = 0;
    while (subsequence[len] != NULL) {
      ++len;
    }

    run_one_sequence(subsequence, "", "");
    free(subsequence); 
  }

}

/*
 * Happy path shell exit.
 */
void shell_exit() {
  printf("Bye bye.\n");
  exit(0);
}

/*
 * Print message on error.
 */
void on_cmd_error(char * path) {
  printf(path);
  printf(": command not found\n");
  exit(0);
}

/*
 * Make a new array containing a subsequence of an existing array.
 */
char ** copy_subsequence(char ** arr, int start, int end) {
  int length = end - start;
  if(start == end){
    ++length;
  }
  char ** subseq = malloc(sizeof(char * ) * length  * 2);
  int n = 0;
  for (int i = start; i <= end; i++) {
    subseq[n] = arr[i];
    ++n;
  }
  subseq[length] = NULL;
  return subseq;
}

/*
 * Execute the previous command.
 */
void shell_prev() {
  if(prev && prev[0]){
  print_tokens(prev);
  iterate_over_tokens(prev);
  }
}

/*
 * Print all strings in a token array.
 */
void print_tokens(char ** tokens) { 
  char ** current = tokens;
  while ( * current != NULL) {
    printf( * current);
    printf(" ");
    ++current;
  }
  printf("\n");
}

/* 
 * If a new line is not the string "prev", update the record of the previous command.
 */
void update_prev(char ** tokens) { 
  if (tokens[0]) {
    if (strcmp(tokens[0], "prev") != 0) {
      if (prev) {
        free_tokens(prev);
      }
      prev = tokens;
    }
  }
}

/*
 * Prints the help message for the shell.
 */
void help() {
  char * help_msg = "exit - close shell\ncd - change directory\nsource - execute a script\nprev - Prints the previous command line and executes it again\nhelp - Explains all the built-in commands\n";
  printf(help_msg);
}

/*
 * Reads commands directly from a file and executes them.
 */
void source_cmd(char ** cmd) {
  // Modified from lab from SO post on reading file line by line https://stackoverflow.com/a/3501681
  FILE * fp;
  char * line = NULL;
  char ** tokens;
  size_t len = 0;
  ssize_t read;

  fp = fopen(cmd[1], "r"); //open file with read only perms

  while ((read = getline( & line, & len, fp)) != -1) { //Iterate line by line over file
    tokens = get_tokens(line);
    iterate_over_tokens(tokens); //Send each tokenized input line to our main event loop
    free_tokens(tokens);
  }

  fclose(fp);
  free(line);
}

/*
 * Changes the current working directory of the shell process.
 */
void cd(char * path) {
  chdir(path);
}

/*
 * If a sequence of tokens has a pipe, splits the sequence into two and pipes them,
 * then returns 1. 
 * 
 * If there is no pipe, returns 0.
 */
int handle_pipes(char ** tokens) {
  int len = 0;
  while (tokens[len] != NULL) {
    ++len;
  }
  
  for (int i = 0; i < len; i++) {
    if (strcmp(tokens[i], "|") == 0) {

      char ** sequence_one = copy_subsequence(tokens, 0, i);       
      char ** sequence_two = copy_subsequence(tokens, i + 1, len);  

      int pid = fork();

      if (pid == 0) {
        pipe_child(sequence_one, sequence_two);        
      } else {
          free(sequence_one);
          free(sequence_two); 
          wait(NULL);
          return 1;
      }
    }
  }

  return 0;
}

/*
 * Replaces the running process with the execution of the command in sequence_one, whose output
 * is piped to the input of the execution of the command in sequence_two.
 */ 
void pipe_child(char ** sequence_one, char ** sequence_two) {
  int pipe_fds[2];
  assert(pipe(pipe_fds) == 0);

  int read_fd = pipe_fds[0];
  int write_fd = pipe_fds[1];
  
  int pid2 = fork();

  if (pid2 == 0) {
    assert(close(read_fd) != -1); // close read end of pipe
    assert(close(1) != -1);       // close stdout
    assert(dup(write_fd) == 1);   // make write end of pipe new output

    iterate_over_tokens(sequence_one);
    
    free_tokens(sequence_one);
    free_tokens(sequence_two);

    exit(0);
  } else {
    assert(close(write_fd) != -1); // close write end of pipe
    assert(close(0) != -1);        // close stdin
    assert(dup(read_fd) == 0);     // make read end of pipe new input

    iterate_over_tokens(sequence_two);
    
    free_tokens(sequence_one);
    free_tokens(sequence_two);

    wait(NULL);
    exit(0);
  }
}
