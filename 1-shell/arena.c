#include "arena.h"
#include "mc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct arena;
struct arena {
	struct arena *next;
	mc *m;
};

static struct arena *cur_arena = NULL;
int dealloc_on_pop_all = 1;

void arena_push(void)
{
	struct arena *a;
	mc *m = mc_init();

	a = mc_calloc(m, 1, sizeof(struct arena));
	a->next = cur_arena;
	a->m = m;
	cur_arena = a;
}

void arena_pop_all(void)
{
	if (dealloc_on_pop_all) {
		while (cur_arena) {
			arena_pop();
		}
	} else {
		while (cur_arena) {
			struct arena *next = cur_arena->next;
			mc_unregister_all_mem(cur_arena->m);
			free(cur_arena);
			cur_arena = next;
		}
	}
}

size_t arena_amount(void)
{
	size_t res = 0;

	for (struct arena *cur = cur_arena; cur; cur = cur->next) {
		res++;
	}

	return res;
}

void arena_pop(void)
{
	assert(cur_arena);

	mc *m = cur_arena->m;

	cur_arena = cur_arena->next;
	// The old arena is saved in this `mc`
	mc_free_all_mem(m);
}

void arena_register_mem(void *pt, const free_fun fun)
{
	assert(cur_arena);
	mc_register_mem(cur_arena->m, pt, fun);
}

void *arena_calloc(size_t nmemb, size_t member_size)
{
	assert(cur_arena);
	return mc_calloc(cur_arena->m, nmemb, member_size);
}

void *arena_malloc(size_t nmemb, size_t member_size)
{
	assert(cur_arena);
	return mc_malloc(cur_arena->m, nmemb, member_size);
}
