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
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        run_command(first_command);
        exit(EXIT_SUCCESS);
    }

    pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid2 == 0) {
        close(pipefd[1]);

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        run_command(second_command);
        exit(EXIT_SUCCESS);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
}

void run_command(node_t *node) {
    arena_push();

    if (node == NULL) {
        arena_pop();
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

    arena_pop();
}
