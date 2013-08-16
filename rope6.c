#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "rle6.h"
#include "rope6.h"

/*******************
 *** Memory Pool ***
 *******************/

#define MP_CHUNK_SIZE 0x100000 // 1MB per chunk

typedef struct { // memory pool for fast and compact memory allocation (no free)
	int size, i, n_elems;
	int64_t top, max;
	uint8_t **mem;
} mempool_t;

static mempool_t *mp_init(int size)
{
	mempool_t *mp;
	mp = calloc(1, sizeof(mempool_t));
	mp->size = size;
	mp->i = mp->n_elems = MP_CHUNK_SIZE / size;
	mp->top = -1;
	return mp;
}

static void mp_destroy(mempool_t *mp)
{
	int64_t i;
	for (i = 0; i <= mp->top; ++i) free(mp->mem[i]);
	free(mp->mem); free(mp);
}

static inline void *mp_alloc(mempool_t *mp)
{
	if (mp->i == mp->n_elems) {
		if (++mp->top == mp->max) {
			mp->max = mp->max? mp->max<<1 : 1;
			mp->mem = realloc(mp->mem, sizeof(void*) * mp->max);
		}
		mp->mem[mp->top] = calloc(mp->n_elems, mp->size);
		mp->i = 0;
	}
	return mp->mem[mp->top] + (mp->i++) * mp->size;
}

/***************
 *** B+ rope ***
 ***************/

typedef struct r6_node_s {
	struct r6_node_s *p; // child; at the bottom level, $p points to a string with the first 4 bytes giving the number of runs (#runs)
	uint64_t l:54, n:9, is_bottom:1; // $n and $is_bottom are only set for the first node in a bucket
	int64_t c[6]; // marginal counts
} node_t;

struct rope6_s {
	int max_nodes, block_len; // both MUST BE even numbers
	int64_t c[6]; // marginal counts
	node_t *root;
	mempool_t *node, *leaf;
};

rope6_t *r6_init(int max_nodes, int block_len)
{
	rope6_t *rope;
	rope = calloc(1, sizeof(rope6_t));
	if (block_len < 32) block_len = 32;
	rope->max_nodes = (max_nodes+ 1)>>1<<1;
	rope->block_len = (block_len + 7) >> 3 << 3;
	rope->node = mp_init(sizeof(node_t) * rope->max_nodes);
	rope->leaf = mp_init(rope->block_len);
	rope->root = mp_alloc(rope->node);
	rope->root->n = 1;
	rope->root->is_bottom = 1;
	rope->root->p = mp_alloc(rope->leaf);
	return rope;
}

void r6_destroy(rope6_t *rope)
{
	mp_destroy(rope->node);
	mp_destroy(rope->leaf);
	free(rope);
}

static inline node_t *split_node(rope6_t *rope, node_t *u, node_t *v)
{ // split $v's child. $u is the first node in the bucket. $v and $u are in the same bucket. IMPORTANT: there is always enough room in $u
	int j, i = v - u;
	node_t *w; // $w is the sibling of $v
	if (u == 0) { // only happens at the root; add a new root
		u = v = mp_alloc(rope->node);
		v->n = 1; v->p = rope->root; // the new root has the old root as the only child
		memcpy(v->c, rope->c, 48);
		for (j = 0; j < 6; ++j) v->l += v->c[j];
		rope->root = v;
	}
	if (i != u->n - 1) // then make room for a new node
		memmove(v + 2, v + 1, sizeof(node_t) * (u->n - i - 1));
	++u->n; w = v + 1;
	memset(w, 0, sizeof(node_t));
	w->p = mp_alloc(u->is_bottom? rope->leaf : rope->node);
	if (u->is_bottom) { // we are at the bottom level; $v->p is a string instead of a node
		uint8_t *p = (uint8_t*)v->p, *q = (uint8_t*)w->p;
		rle_split(rope->block_len, p, q);
		rle_count(rope->block_len, q, w->c);
	} else { // $v->p is a node, not a string
		node_t *p = v->p, *q = w->p; // $v and $w are siblings and thus $p and $q are cousins
		p->n -= rope->max_nodes>>1;
		memcpy(q, p + p->n, sizeof(node_t) * (rope->max_nodes>>1));
		q->n = rope->max_nodes>>1; // NB: this line must below memcpy() as $q->n and $q->is_bottom are modified by memcpy()
		q->is_bottom = p->is_bottom;
		for (i = 0; i < q->n; ++i)
			for (j = 0; j < 6; ++j)
				w->c[j] += q[i].c[j];
	}
	for (j = 0; j < 6; ++j) // compute $w->l and update $v->c
		w->l += w->c[j], v->c[j] -= w->c[j];
	v->l -= w->l; // update $v->c
	return v;
}

int64_t r6_insert_symbol(rope6_t *rope, int a, int64_t x)
{ // insert $a after $x symbols in $rope and the returns the position of the next insertion
	node_t *u = 0, *v = 0, *p = rope->root; // $v is the parent of $p; $u and $v are at the same level and $u is the first node in the bucket
	int64_t y = 0, z, cnt[6];
	int i, is_split;
	for (i = 0, z = 0; i < a; ++i) z += rope->c[i];
	do { // top-down update. Searching and node splitting are done together in one pass.
		if (p->n == rope->max_nodes) { // node is full; split
			v = split_node(rope, u, v); // $v points to the parent of $p; when a new root is added, $v points to the root
			if (y + v->l < x) // if $v is not long enough after the split, we need to move both $p and its parent $v
				y += v->l, z += v->c[a], ++v, p = v->p;
		}
		u = p;
		if (v && x - y > v->l>>1) { // then search backwardly for the right node to descend
			p += p->n - 1; y += v->l; z += v->c[a];
			for (; y >= x; --p) y -= p->l, z -= p->c[a];
			++p;
		} else for (; y + p->l < x; ++p) y += p->l, z += p->c[a]; // then search forwardly
		assert(p - u < u->n);
		if (v) ++v->c[a], ++v->l; // we should not change p->c[a] because this may cause troubles when p's child is split
		v = p; p = p->p; // descend
	} while (!u->is_bottom);
	++rope->c[a]; // $rope->c should be updated after the loop as adding a new root needs the old $rope->c counts
	is_split = rle_insert(rope->block_len, (uint8_t*)p, x - y, a, 1, cnt, v->c);
	z += cnt[a] + 1;
	++v->c[a]; ++v->l; // this should be below rle_insert(); otherwise it won't work
	if (is_split) split_node(rope, u, v);
//	printf("%c\t%ld\t%ld\t%ld\n", "$ACGTN"[a], (long)x, (long)z, (long)(cnt[a] + 1));
	return z;
}

void r6_insert_string_core(rope6_t *rope, int l, uint8_t *str, uint64_t x)
{
	for (--l; l >= 0; --l)
		x = r6_insert_symbol(rope, str[l], x);
	r6_insert_symbol(rope, 0, x);
}

void r6_insert_string_io(rope6_t *rope, int l, uint8_t *str)
{
	r6_insert_string_core(rope, l, str, rope->c[0]);
}

static node_t *r6_count_to_leaf(const rope6_t *rope, int64_t x, int64_t cx[6], int64_t *rest)
{
	node_t *u, *v = 0, *p = rope->root;
	int64_t y = 0;
	int a;

	memset(cx, 0, 48);
	do {
		u = p;
		if (v && x - y > v->l>>1) {
			p += p->n - 1; y += v->l;
			for (a = 0; a != 6; ++a) cx[a] += v->c[a];
			for (; y >= x; --p) {
				y -= p->l;
				for (a = 0; a != 6; ++a) cx[a] -= p->c[a];
			}
			++p;
		} else {
			for (; y + p->l < x; ++p) {
				y += p->l;
				for (a = 0; a != 6; ++a) cx[a] += p->c[a];
			}
		}
		v = p; p = p->p;
	} while (!u->is_bottom);
	*rest = x - y;
	return v;
}

void r6_rank1a(const rope6_t *rope, int64_t x, int64_t ok[6])
{ // on return: ok[c] = |{i<x && a<c:B[i]=c}|; note that it is "i<x" not "i<=x"
	node_t *v;
	int64_t rest;
	v = r6_count_to_leaf(rope, x, ok, &rest);
	rle_rank2a(rope->block_len, (const uint8_t*)v->p, rest, rest, ok, 0, v->c);
}

void r6_insert_string_rlo(rope6_t *rope, int len, uint8_t *str)
{
	int64_t tl[6], tu[6], l, u;
	l = 0; u = rope->c[0];
	while (--len >= 0) {
		int a, c = str[len];
//		printf("%lld\t%lld\t", l, u); rle_print(rope->block_len, (uint8_t*)rope->root->p, 1);
		r6_rank1a(rope, l, tl);
		r6_rank1a(rope, u, tu);
		for (a = 0; a < c; ++a) l += tu[a] - tl[a];
		if (tl[c] < tu[c]) {
			int64_t cnt;
			r6_insert_symbol(rope, c, l);
			for (a = 0, cnt = 0; a < c; ++a) cnt += rope->c[a];
			l = cnt + tl[c] + 1; u = cnt + tu[c] + 1;
		} else {
			r6_insert_string_core(rope, len+1, str, l);
			return;
		}
	}
	r6_insert_symbol(rope, 0, l);
}

/*********************
 *** Rope iterator ***
 *********************/

struct r6itr_s {
	const rope6_t *rope;
	const node_t *pa[80];
	int k, ia[80];
};

r6itr_t *r6_itr_init(const rope6_t *rope)
{
	r6itr_t *i;
	i = calloc(1, sizeof(r6itr_t));
	i->rope = rope;
	for (i->pa[i->k] = rope->root; !i->pa[i->k]->is_bottom;) // descend to the leftmost leaf
		++i->k, i->pa[i->k] = i->pa[i->k - 1]->p;
	return i;
}

const uint8_t *r6_itr_next(r6itr_t *i, int *n)
{
	const uint8_t *ret;
	assert(i->k < 80); // a B+ tree should not be that tall
	if (i->k < 0) return 0;
	*n = i->rope->block_len;
	ret = (uint8_t*)i->pa[i->k][i->ia[i->k]].p;
	while (i->k >= 0 && ++i->ia[i->k] == i->pa[i->k]->n) i->ia[i->k--] = 0; // backtracking
	if (i->k >= 0)
		while (!i->pa[i->k]->is_bottom) // descend to the leftmost leaf
			++i->k, i->pa[i->k] = i->pa[i->k - 1][i->ia[i->k - 1]].p;
	return ret;
}
