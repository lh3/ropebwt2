#include <stdlib.h>
#include <stdio.h>
#include "mrope.h"

mrope_t *mr_init(int max_nodes, int block_len)
{
	int a;
	mrope_t *r;
	r = calloc(1, sizeof(mrope_t));
	for (a = 0; a != 6; ++a)
		r->r[a] = rope_init(max_nodes, block_len);
	return r;
}

void mr_destroy(mrope_t *r)
{
	int a;
	for (a = 0; a != 6; ++a)
		rope_destroy(r->r[a]);
}

void mr_insert_string_io(mrope_t *r, const uint8_t *str)
{
	const uint8_t *p;
	int64_t x;
	int a;
	for (a = 0, x = 0; a != 6; ++a)
		x += r->r[a]->c[0];
	for (p = str, a = 0; *p; a = *p++) {
		x = rope_insert_run(r->r[a], x, *p, 1);
		while (--a >= 0) x += r->r[a]->c[*p];
	}
	rope_insert_run(r->r[a], x, *p, 1);
}

void mr_itr_first(const mrope_t *r, mritr_t *i)
{
	i->a = 0; i->r = r;
	rope_itr_first(i->r->r[0], &i->i);
}

const uint8_t *mr_itr_next_block(mritr_t *i, int *n)
{
	const uint8_t *s;
	*n = 0;
	if (i->a >= 6) return 0;
	while ((s = rope_itr_next_block(&i->i, n)) == 0) {
		if (++i->a == 6) return 0;
		rope_itr_first(i->r->r[i->a], &i->i);
	}
	return i->a == 6? 0 : s;
}
