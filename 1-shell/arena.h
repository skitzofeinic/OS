#ifndef ARENA_H
#define ARENA_H
#include "mc.h"

// Should we free all memory in all arena's when we call `arena_pop_all`? Set
// this variable to 0 to disable freeing. Please see the hints file for why this
// variable exists and why it is important.
extern int dealloc_on_pop_all;

// Create a new memory arena.
void arena_push(void);

// Pop the current memory arena and free all memory in it.
void arena_pop(void);

// Pop all memory arena's. This function means that all memory allocated by
// arena_* functions is freed.
void arena_pop_all(void);

// Get the amount of arena's.
size_t arena_amount(void);

// Register the memory given in `pt` in the current arena. It will be freed with
// the function given by `fun` when the arena is popped.
void arena_register_mem(void *pt, const free_fun fun);

// Allocate a new piece of memory in the current arena. This function works the
// same as `calloc(3)`
void *arena_calloc(size_t nmemb, size_t member_size);

// Allocate a new piece of memory in the current arena. This function works the
// same as `malloc(3)`. It calculates the size needed by doing `nmemb *
// member_size`, but is checks if the amount needed does not overflow.
void *arena_malloc(size_t nmemb, size_t member_size);

#endif /* ARENA_H */
