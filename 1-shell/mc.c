#include "mc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct m_node {
	struct m_node *next;
	void *pt;
	free_fun fun;
};

struct mc {
	m_node *header;
	size_t n;
};

mc *mc_init()
{
	mc *res = malloc(sizeof(mc));
	if (NULL == res)
		exit(EXIT_FAILURE);
	res->n = 0;
	res->header = NULL;
	return res;
}

static void assert_new_pt(mc *m, void *pt)
{
#ifndef NDEBUG
	m_node *cur = m->header;
	for (size_t i = 0; i < m->n; i++, cur = cur->next) {
		assert(pt != cur->pt);
	}
#else
	(void)m;
	(void)pt;
#endif
}

static void *alloc_mem(mc *m, size_t nmemb, size_t member_size, int use_calloc)
{
	void *res;

	if (nmemb == 0 || member_size == 0)
		return NULL;

	if (use_calloc) {
		res = calloc(nmemb, member_size);
	} else {
		size_t size;
		size = nmemb * member_size;
		assert(size / nmemb == member_size);
		res = malloc(size);
	}

	mc_register_mem(m, res, &free);
	return res;
}

void *mc_calloc(mc *m, size_t nmemb, size_t member_size)
{
	return alloc_mem(m, nmemb, member_size, 1);
}

void *mc_malloc(mc *m, size_t nmemb, size_t member_size)
{
	return alloc_mem(m, nmemb, member_size, 0);
}

void mc_register_mem(mc *m, void *pt, const free_fun fun)
{
	m_node *new_node = malloc(sizeof(m_node));

	if (NULL == new_node) {
		mc_free_all_mem(m);
		fun(pt);
		exit(EXIT_FAILURE);
	}

	// Make sure pointer is unique to prevent double free errors.
	assert_new_pt(m, pt);

	new_node->fun = fun;
	new_node->pt = pt;
	new_node->next = m->header;
	m->header = new_node;
	m->n++;
}

void mc_free_all_mem(mc *m)
{
	m_node *cur = m->header, *next;
	for (size_t i = 0; i < m->n; i++) {
		next = cur->next;
		cur->fun(cur->pt);
		free(cur);
		cur = next;
	}
	free(m);
}

void mc_unregister_all_mem(mc *m)
{
	m_node *cur = m->header, *next;
	for (size_t i = 0; i < m->n; i++) {
		next = cur->next;
		free(cur);
		cur = next;
	}
	free(m);
}

m_node *mc_unregister_mem(mc *m, const void *pt)
{
	m_node *cur = m->header->next, *prev = m->header;
	m->n--;
	if (pt == prev->pt) {
		m->header = cur;
		return prev;
	}
	for (size_t i = 0; i < m->n; i++) {
		if (pt == cur->pt) {
			prev->next = cur->next;
			return cur;
		}
		prev = cur;
		cur = cur->next;
	}
	assert(0);
}

void mc_free_mem(mc *m, void *pt)
{
	m_node *to_free = mc_unregister_mem(m, pt);
	to_free->fun(to_free->pt);
	free(to_free);
}
