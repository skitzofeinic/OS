#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "arena.h"
#include "front.h"
#include "parser/ast.h"
#include "shell.h"
#include <signal.h>

void my_free_tree(void *pt)
{
    free_tree((node_t *)pt);
}

void initialize(void)
{
    signal(SIGINT, SIG_IGN);
}

void shell_exit(void)
{
    // This code will be called on exit
}

void execute_single_command(node_t *node) {
    if (node == NULL || node->type != NODE_COMMAND)
        return;

    char *program = node->command.program;
    char **argv = node->command.argv;

    // Handle built-in commands
    if (strcmp(program, "exit") == 0) {
        int exit_code = 0;
        if (argv[1] != NULL)
            exit_code = atoi(argv[1]);
        exit(exit_code);
    }

    if (strcmp(program, "cd") == 0) {
        if (argv[1] != NULL) {
            if (chdir(argv[1]) == -1)
                perror("cd");
        } else {
            fprintf(stderr, "cd: missing argument\n");
        }
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            execvp(program, argv);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                fprintf(stderr, "%s: command failed with exit status %d\n", program, WEXITSTATUS(status));
        } else {
            perror("fork");
        }
    }
}

void run_command(node_t *node) {
    arena_push(); // Create a new memory arena

    if (node == NULL) {
        arena_pop(); // Clean up memory arena
        return;
    }

    if (node->type == NODE_COMMAND) {
        execute_single_command(node);
    }
    
    if (node->type == NODE_SEQUENCE) {
        run_command(node->sequence.first);
        run_command(node->sequence.second);
    }
    
    if (node->type == NODE_PIPE) {
        int num_commands = node->pipe.n_parts;
        int pipefd[num_commands - 1][2];
        pid_t pids[num_commands];
        int status;

        for (int i = 0; i < num_commands - 1; i++) {
            if (pipe(pipefd[i]) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        for (int i = 0; i < num_commands; i++) {
            pids[i] = fork();
            if (pids[i] == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pids[i] == 0) {
                // Child process
                if (i != 0) {
                    dup2(pipefd[i - 1][0], STDIN_FILENO); // Redirect stdin from previous pipe
                    close(pipefd[i - 1][0]); // Close read end of previous pipe
                }
                if (i != num_commands - 1) {
                    dup2(pipefd[i][1], STDOUT_FILENO); // Redirect stdout to next pipe
                    close(pipefd[i][1]); // Close write end of current pipe
                }
                for (int j = 0; j < num_commands - 1; j++) {
                    close(pipefd[j][0]); // Close read ends of all other pipes
                    close(pipefd[j][1]); // Close write ends of all other pipes
                }
                run_command(node->pipe.parts[i]);
                exit(EXIT_SUCCESS);
            }
        }

        // Parent process
        for (int i = 0; i < num_commands - 1; i++) {
            close(pipefd[i][0]); // Close read ends of all pipes
            close(pipefd[i][1]); // Close write ends of all pipes
        }

        // Wait for all child processes to complete
        for (int i = 0; i < num_commands; i++) {
            waitpid(pids[i], &status, 0);
        }
    }

    arena_pop(); // Clean up memory arena
}
