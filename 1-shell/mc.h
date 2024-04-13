#ifndef MC_H
#define MC_H
#include <unistd.h>

typedef struct m_node m_node;
typedef struct mc mc;

// Function type used as free function. Please note that function is called with
// a `void` pointer as only argument.
typedef void (*free_fun)(void *pt);

// Create a new mc.
mc *mc_init();

// Register the pointer `pt` with function `fun` in the given mc `m`.
void mc_register_mem(mc *m, void *pt, const free_fun fun);

// Free all memory in the given `m` by calling their accompanying functions with
// `pt` as the sole argument. `m` itself is also freed.
void mc_free_all_mem(mc *m);

// Free the given pointer `pt` in `m` by calling its accompanying function.
void mc_free_mem(mc *m, void *pt);

// Unregistered all memory in `m`. This DOES NOT free this memory.
void mc_unregister_all_mem(mc *m);

// Unregister the given pointer `pt` from `m`. This does not free this
// memory. This function is useful for code like this:
//
// pt = mc_malloc(m, 1, sizeof(sturct ...));
// if (unlikely) {
//     free(pt);
//     mc_unregister_mem(m, pt);
//     return 0;
// } else {
//     save_in_struct(st, pt);
//     return 1;
// }
m_node *mc_unregister_mem(mc *m, const void *pt);

// Allocate new memory and store it in `m` using `free` as the free
// function. Allocated memory contains garbage, pointer to this garbage is
// returned.
void *mc_malloc(mc *m, size_t nmemb, size_t member_size);

// Allocate new memory and store it in `m` using `free` as the free
// function. Allocated memory contains zero's, pointer to these zero's is
// returned.
void *mc_calloc(mc *m, size_t nmemb, size_t member_size);

#endif
