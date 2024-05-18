#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define MAX_ARGS 5
#define DELIM " \t\r\n\a"

pid_t last_bg_pid = 0; // Global variable to store the PID of the last background process

void bring_to_foreground() {
    if (last_bg_pid != 0) {
        // Move the process group to the foreground
        tcsetpgrp(STDIN_FILENO, getpgid(last_bg_pid));
        // Wait for the process to finish
        waitpid(last_bg_pid, NULL, WUNTRACED);
        // Move control back to the shell
        tcsetpgrp(STDIN_FILENO, getpgid(getpid()));
        // Clear the last background process PID
        last_bg_pid = 0;
    } else {
        printf("No background process to bring to foreground.\n");
    }
}

pid_t execute_command(char **args, int background, char *input_redirection, char *output_redirection, int append_mode) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        int fd_in, fd_out;
        
        // Allocate enough space for args array in the child process
        char *child_args[MAX_ARGS + 2];
        for (int i = 0; i < MAX_ARGS + 1; ++i) {
            child_args[i] = args[i];
        }
        child_args[MAX_ARGS + 1] = NULL; // Null-terminate the args array

        // Handle input redirection
        if (input_redirection != NULL) {
            fd_in = open(input_redirection, O_RDONLY);
            if (fd_in == -1) {
                perror("shell24");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        // Handle output redirection
        if (output_redirection != NULL) {
            if (append_mode)
                fd_out = open(output_redirection, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                fd_out = open(output_redirection, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1) {
                perror("shell24");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        // Execute the command
        if (execvp(child_args[0], child_args) == -1) {
            perror("shell24");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS); // Ensure child process exits successfully
    } else if (pid < 0) {
        // Error forking
        perror("shell24");
        return EXIT_FAILURE; // Return failure status
    } else {
        // Parent process
        if (!background) {
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        } else {
            printf("[+] Started background process with PID: %d\n", pid);
        }
    }
    return pid; // Return success status
}

void concatenate_text_files(char **files) {
    FILE *fp;
    char buffer[MAX_LINE];
    int i = 0;

    while (files[i] != NULL) {
        if (strcmp(files[i], "#") == 0) {
            // Skip the '#' token
            ++i;
            continue;
        }
        printf("File: %s\n", files[i]); // Print the filename
        
        fp = fopen(files[i], "r");
        if (fp == NULL) {
            fprintf(stderr, "shell24: Cannot open file %s\n", files[i]);
            return;
        }

        while (fgets(buffer, MAX_LINE, fp) != NULL) {
            printf("%s", buffer);
        }
        fclose(fp);
        ++i;
        
    }
}

void execute_newt() {
    pid_t pid = fork();
    if (pid == 0) {
            // Child process: Execute shell24 in a new terminal window
            printf("Launching new terminal instance!\n");
            //execlp("x-terminal-emulator", "x-terminal-emulator", "-e", "/home/beheras/Desktop/a3", (char *)NULL);//nomachine
            execlp("open", "open", "-a", "Terminal", "/Users/sasmita/Documents/C_Program/asp", (char *)NULL);//mac
            _exit(EXIT_FAILURE); // Exit if exec fails
        } else if (pid > 0) {
            // Parent process
            return;
        } else {
            // Fork failed
            perror("Failed to fork a new process");
        }
}
/*pid_t execute_newt() {
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        // Child process
        // Allocate enough space for args array in the child process
        char *child_args[MAX_ARGS + 2];
        child_args[0] = "shell24"; // Set the first argument to "shell24"
        child_args[1] = NULL; // Null-terminate the args array

        // Execute the new terminal instance
        printf("Launching new terminal instance!\n");
        execlp("x-terminal-emulator", "x-terminal-emulator", "-e", "/home/beheras/Desktop/a3", (char *)NULL);
        
        // If execlp fails, exit the child process
        perror("Failed to execute new terminal instance");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("shell24");
        return EXIT_FAILURE; // Return failure status
    } else {
        // Parent process
        return pid;
    }
}*/


void parse_and_execute(char *line) {
    char *args[MAX_ARGS + 2]; // +1 for NULL at the end, +1 for potential background symbol
    char *token;
    int argc = 0;
    int background = 0;
    char *input_redirection = NULL;
    char *output_redirection = NULL;
    int append_mode = 0;

    token = strtok(line, DELIM);
    while (token != NULL) {
        if (argc >= MAX_ARGS) {
            fprintf(stderr, "shell24: too many arguments\n");
            return;
        }

        if (strcmp(token, "<") == 0) {
            // Input redirection
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "shell24: syntax error near unexpected token `<'\n");
                return;
            }
            input_redirection = token;
        } else if (strcmp(token, ">") == 0) {
            // Output redirection (overwrite)
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "shell24: syntax error near unexpected token `>'\n");
                return;
            }
            output_redirection = token;
        } else if (strcmp(token, ">>") == 0) {
            // Output redirection (append)
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "shell24: syntax error near unexpected token `>>'\n");
                return;
            }
            output_redirection = token;
            append_mode = 1;
        } else if (strcmp(token, "&&") == 0) {
            // Conditional execution (AND)
            // Execute the previous command if it was successful
            pid_t result = execute_command(args, background, input_redirection, output_redirection, append_mode);
            if (!result) {
                // If the previous command failed, stop execution
                return;
            }
            // Reset arguments and redirections for the next command
            argc = 0;
            input_redirection = NULL;
            output_redirection = NULL;
            append_mode = 0;
        } else if (strcmp(token, "||") == 0) {
            // Conditional execution (OR)
            // Execute the previous command if it failed
            pid_t result = execute_command(args, background, input_redirection, output_redirection, append_mode);
            if (result) {
                // If the previous command succeeded, stop execution
                return;
            }
            // Reset arguments and redirections for the next command
            argc = 0;
            input_redirection = NULL;
            output_redirection = NULL;
            append_mode = 0;
        } else if (strcmp(token, "newt") == 0) {
            // Execute new terminal instance
            execute_newt();
            return;
        } else if (strcmp(token, "#") == 0) {
            // Text file (.txt) concatenation
           /*char *files[MAX_ARGS + 1]; // +1 for NULL at the end
            int file_index = 0;
            token = strtok(NULL, DELIM);
            while (token != NULL) {
                if (file_index >= MAX_ARGS) {
                    fprintf(stderr, "shell24: too many files for concatenation\n");
                    return;
                }
                files[file_index++] = token;
                token = strtok(NULL, DELIM);
            }
            files[file_index] = NULL; // Null-terminate the list of files
            concatenate_text_files(files);
            return;*/ 
            concatenate_text_files(args);
            return;
        } else if (strcmp(token, "&") == 0) {
            // Background processing
            background = 1;
            break; // Stop parsing further and execute the command in the background
        } else if (strcmp(token, "fg") == 0) {
            // Bring the last background process into the foreground
            bring_to_foreground();
            return;
        }else {
            // Normal argument
            args[argc++] = token;
        }

        token = strtok(NULL, DELIM);
    }

    if (argc == 0) { // Empty command
        return;
    }

    // Execute the final command
    execute_command(args, background, input_redirection, output_redirection, append_mode);
}

int main() {
    char line[MAX_LINE];
    char *input;

    while (1) {
        printf("shell24$ ");
        fflush(stdout);

        input = fgets(line, MAX_LINE, stdin);
        if (!input) {
            if (feof(stdin)) { // Check for EOF
                break;
            }
            continue; // Otherwise, ignore the input and continue
        }

        char *next_cmd = strtok(line, ";");
        while (next_cmd != NULL) {
            parse_and_execute(next_cmd);
            next_cmd = strtok(NULL, ";");
        }
    }

    return 0;
}
