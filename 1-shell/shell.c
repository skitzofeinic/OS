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
    // This code will be called once at startup
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
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
        // External command execution
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            execvp(program, argv);
            // execvp returns only if an error occurs
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                fprintf(stderr, "%s: command failed with exit status %d\n", program, WEXITSTATUS(status));
        } else {
            // Fork failed
            perror("fork");
        }
    }
}

void execute_piped_commands(node_t *first_command, node_t *second_command) {
    int pipefd[2];
    pid_t pid1, pid2;
    int status;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid1 == 0) {
        // Child process for the first command
        close(pipefd[0]); // Close unused read end of the pipe

        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
        close(pipefd[1]); // Close write end after redirection

        run_command(first_command); // Execute the first command
        exit(EXIT_SUCCESS);
    }

    pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid2 == 0) {
        // Child process for the second command
        close(pipefd[1]); // Close unused write end of the pipe

        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe
        close(pipefd[0]); // Close read end after redirection

        run_command(second_command); // Execute the second command
        exit(EXIT_SUCCESS);
    }

    // Parent process
    close(pipefd[0]); // Close unused read end of the pipe
    close(pipefd[1]); // Close unused write end of the pipe

    // Wait for both child processes to complete
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
}

void run_command(node_t *node) {
    arena_push(); // Create a new memory arena

    if (node == NULL) {
        arena_pop(); // Clean up memory arena
        return;
    }

    if (node->type == NODE_COMMAND) {
        execute_single_command(node);
    } else if (node->type == NODE_SEQUENCE) {
        run_command(node->sequence.first);
        run_command(node->sequence.second);
    } else if (node->type == NODE_PIPE) {
        execute_piped_commands(node->pipe.parts[0], node->pipe.parts[1]);
    }

    arena_pop(); // Clean up memory arena
}
