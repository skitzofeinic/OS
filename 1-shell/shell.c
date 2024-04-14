#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "arena.h"
#include "front.h"
#include "parser/ast.h"
#include "shell.h"

void my_free_tree(void *pt)
{
    free_tree((node_t *)pt);
}

void initialize(void)
{
    // This code will be called once at startup
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

void run_command(node_t *node)
{
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
    }

    arena_pop(); // Clean up memory arena
}
