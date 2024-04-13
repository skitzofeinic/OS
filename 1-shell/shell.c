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
    // This code will be called on exit,
    // this might not be required but could be useful
}


void run_command(node_t *node)
{
	// Create a new memory arena to handle this function in. Please see the
	// hints and `arena.h` on how to use them, as you can very easily create
	// memory leaks if you are not careful.
	arena_push();

	// (for testing:)
	print_tree(node);

	// Pop the current memory arena, this frees all memory in the current arena.
	arena_pop();
}
