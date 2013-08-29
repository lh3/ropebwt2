#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "rle.h"
#include "rope.h"

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

rope_t *rope_init(int max_nodes, int block_len)
{
	rope_t *rope;
	rope = calloc(1, sizeof(rope_t));
	if (block_len < 32) block_len = 32;
	rope->max_nodes = (max_nodes+ 1)>>1<<1;
	rope->block_len = (block_len + 7) >> 3 << 3;
	rope->node = mp_init(sizeof(rpnode_t) * rope->max_nodes);
	rope->leaf = mp_init(rope->block_len);
	rope->root = mp_alloc(rope->node);
	rope->root->n = 1;
	rope->root->is_bottom = 1;
	rope->root->p = mp_alloc(rope->leaf);
	return rope;
}

void rope_destroy(rope_t *rope)
{
	mp_destroy(rope->node);
	mp_destroy(rope->leaf);
	free(rope);
}

static inline rpnode_t *split_node(rope_t *rope, rpnode_t *u, rpnode_t *v)
{ // split $v's child. $u is the first node in the bucket. $v and $u are in the same bucket. IMPORTANT: there is always enough room in $u
	int j, i = v - u;
	rpnode_t *w; // $w is the sibling of $v
	if (u == 0) { // only happens at the root; add a new root
		u = v = mp_alloc(rope->node);
		v->n = 1; v->p = rope->root; // the new root has the old root as the only child
		memcpy(v->c, rope->c, 48);
		for (j = 0; j < 6; ++j) v->l += v->c[j];
		rope->root = v;
	}
	if (i != u->n - 1) // then make room for a new node
		memmove(v + 2, v + 1, sizeof(rpnode_t) * (u->n - i - 1));
	++u->n; w = v + 1;
	memset(w, 0, sizeof(rpnode_t));
	w->p = mp_alloc(u->is_bottom? rope->leaf : rope->node);
	if (u->is_bottom) { // we are at the bottom level; $v->p is a string instead of a node
		uint8_t *p = (uint8_t*)v->p, *q = (uint8_t*)w->p;
		rle_split(p, q);
		rle_count(q, w->c);
	} else { // $v->p is a node, not a string
		rpnode_t *p = v->p, *q = w->p; // $v and $w are siblings and thus $p and $q are cousins
		p->n -= rope->max_nodes>>1;
		memcpy(q, p + p->n, sizeof(rpnode_t) * (rope->max_nodes>>1));
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

int64_t rope_insert_run(rope_t *rope, int64_t x, int a, int64_t rl)
{ // insert $a after $x symbols in $rope and the returns rank(a, x)
	rpnode_t *u = 0, *v = 0, *p = rope->root; // $v is the parent of $p; $u and $v are at the same level and $u is the first node in the bucket
	int64_t y = 0, z = 0, cnt[6];
	int n_runs;
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
		if (v) v->c[a] += rl, v->l += rl; // we should not change p->c[a] because this may cause troubles when p's child is split
		v = p; p = p->p; // descend
	} while (!u->is_bottom);
	rope->c[a] += rl; // $rope->c should be updated after the loop as adding a new root needs the old $rope->c counts
	n_runs = rle_insert((uint8_t*)p, x - y, a, rl, cnt, v->c);
	z += cnt[a];
	v->c[a] += rl; v->l += rl; // this should be after rle_insert(); otherwise rle_insert() won't work
	if (n_runs + RLE_MIN_SPACE > rope->block_len) split_node(rope, u, v);
	return z;
}

void rope_insert_string_io(rope_t *rope, const uint8_t *str)
{
	const uint8_t *p;
	int64_t x = rope->c[0];
	int a;
	for (p = str; *p; ++p) {
		x = rope_insert_run(rope, x, *p, 1) + 1;
		for (a = 0; a < *p; ++a) x += rope->c[a];
	}
	rope_insert_run(rope, x, 0, 1);
}

static rpnode_t *rope_count_to_leaf(const rope_t *rope, int64_t x, int64_t cx[6], int64_t *rest)
{
	rpnode_t *u, *v = 0, *p = rope->root;
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

void rope_rank2a(const rope_t *rope, int64_t x, int64_t y, int64_t *cx, int64_t *cy)
{
	rpnode_t *v;
	int64_t rest;
	v = rope_count_to_leaf(rope, x, cx, &rest);
	if (y < x || cy == 0) {
		rle_rank1a((const uint8_t*)v->p, rest, cx, v->c);
	} else if (rest + (y - x) <= v->l) {
		memcpy(cy, cx, 48);
		rle_rank2a((const uint8_t*)v->p, rest, rest + (y - x), cx, cy, v->c);
	} else {
		rle_rank1a((const uint8_t*)v->p, rest, cx, v->c);
		v = rope_count_to_leaf(rope, y, cy, &rest);
		rle_rank1a((const uint8_t*)v->p, rest, cy, v->c);
	}
}

void rope_insert_string_rlo(rope_t *rope, const uint8_t *str, int is_comp)
{
	int64_t tl[6], tu[6], l, u;
	const uint8_t *p;
	l = 0; u = rope->c[0];
	for (p = str; *p; ++p) {
		int a, c = *p;
		if (l != u) {
			int64_t cnt;
			rope_rank2a(rope, l, u, tl, tu);
			if (is_comp) for (a = 5; a > c; --a) l += tu[a] - tl[a];
			else for (a = 0; a < c; ++a) l += tu[a] - tl[a];
			rope_insert_run(rope, l, c, 1);
			for (a = 0, cnt = 0; a < c; ++a) cnt += rope->c[a];
			l = cnt + tl[c] + 1; u = cnt + tu[c] + 1;
		} else {
			l = rope_insert_run(rope, l, c, 1) + 1;
			for (a = 0; a < c; ++a) l += rope->c[a];
			u = l;
		}
	}
	rope_insert_run(rope, l, 0, 1);
}

/*******************************
 *** Insert multiple strings ***
 *******************************/

typedef struct {
	uint64_t u, v, w;
} triple64_t;

#define rstype_t triple64_t
#define rskey(x) ((x).u)

#define RS_MIN_SIZE 64

typedef struct {
	rstype_t *b, *e;
} rsbucket_t;

void rs_sort(rstype_t *beg, rstype_t *end, int n_bits, int s)
{
	rstype_t *i;
	int size = 1<<n_bits, m = size - 1;
	rsbucket_t *k, b[size], *be = b + size;

	for (k = b; k != be; ++k) k->b = k->e = beg;
	for (i = beg; i != end; ++i) ++b[rskey(*i)>>s&m].e; // count radix
	for (k = b + 1; k != be; ++k) // set start and end of each bucket
		k->e += (k-1)->e - beg, k->b = (k-1)->e;
	for (k = b; k != be;) { // in-place classification based on radix
		if (k->b != k->e) { // the bucket is not full
			rsbucket_t *l;
			if ((l = b + (rskey(*k->b)>>s&m)) != k) { // destination different
				rstype_t tmp = *k->b, swap;
				do { // swap until we find an element in $k
					swap = tmp; tmp = *l->b; *l->b++ = swap;
					l = b + (rskey(tmp)>>s&m);
				} while (l != k);
				*k->b++ = tmp;
			} else ++k->b;
		} else ++k;
	}
	for (b->b = beg, k = b + 1; k != be; ++k) k->b = (k-1)->e; // reset k->b
	if (s) { // if $s is non-zero, we need to sort buckets
		s = s > n_bits? s - n_bits : 0;
		for (k = b; k != be; ++k)
			if (k->e - k->b > RS_MIN_SIZE) rs_sort(k->b, k->e, n_bits, s);
			else if (k->e - k->b > 1) // then use an insertion sort
				for (i = k->b + 1; i < k->e; ++i)
					if (rskey(*i) < rskey(*(i - 1))) {
						rstype_t *j, tmp = *i;
						for (j = i; j > k->b && rskey(tmp) < rskey(*(j-1)); --j)
							*j = *(j - 1);
						*j = tmp;
					}
	}
}

typedef const uint8_t *cstr_t;

void rope_insert_multi(rope_t *rope, int64_t len, const uint8_t *s)
{
	int64_t k, m, d;
	cstr_t *ptr;
	triple64_t *a;

	assert(len > 0 && s[len-1] == 0);
	{ // initialize m and *ptr
		cstr_t p, q, end = s + len;
		for (p = s, m = 0; p != end; ++p) // count #sentinels
			if (*p == 0) ++m;
		ptr = malloc(m * sizeof(cstr_t));
		for (p = q = s, k = 0; p != end; ++p) // find the start of each string
			if (*p == 0) ptr[k++] = q, q = p + 1;
	}

	a = malloc(m * sizeof(triple64_t));
	for (k = 0; k != m; ++k)
		a[k].u = 0, a[k].v = rope->c[0], a[k].w = k;

	for (d = 0; m; ++d) {
		int64_t beg, max = 0, c[6], i;
		int b, l;
		for (k = 0; k != m; ++k) { // set the base to insert
			triple64_t *p = &a[k];
			p->u = (p->u & ~7ULL) | ptr[p->w][d];
			max = max > p->u? max : p->u;
		}
		for (k = max, l = 0; k; k >>= 1, ++l);
		rs_sort(a, &a[m], 8, l > 7? l - 7 : 0);
		for (k = 1, beg = 0; k <= m; ++k) {
			if (k == m || a[k].u>>3 != a[k-1].u>>3) {
				int64_t x, i, l = a[beg].u>>3, u = a[beg].v, tl[6], tu[6];
				memset(c, 0, 48);
				for (i = beg; i < k; ++i) ++c[a[i].u&7];
				if (l == u) {
					memset(tl, 0, 48);
					memset(tu, 0, 48);
				} else rope_rank2a(rope, l, u, tl, tu);
				if (c[0]) rope_insert_run(rope, l, 0, c[0]);
				for (b = 1, x = l + c[0] + (tu[0] - tl[0]); b != 6; ++b) {
					int64_t y, size = tu[b] - tl[b];
					if (c[b] == 0) continue;
					y = rope_insert_run(rope, x, b, c[b]);
					tl[b] = y, tu[b] = y + size;
					x += c[b] + size;
				}
				for (i = beg; i < k; ++i) {
					triple64_t *p = &a[i];
					p->u = tl[p->u&7]<<3 | (p->u&7);
					p->v = tu[p->u&7];
				}
				beg = k;
			}
		}

		for (k = i = 0; k != m; ++k) // squeeze out sentinels
			if (a[k].u&7) a[i++] = a[k];
		m = i;
		for (b = 1, c[0] = m; b != 6; ++b)
			c[b] = c[b-1] + rope->c[b-1];
		for (k = 0; k != m; ++k) {
			triple64_t *p = &a[k];
			p->u += c[p->u&7]<<3; p->v += c[p->u&7];
		}
	}

	free(ptr); free(a);
}

/*********************
 *** Rope iterator ***
 *********************/

void rope_itr_first(const rope_t *rope, rpitr_t *i)
{
	memset(i, 0, sizeof(rpitr_t));
	i->rope = rope;
	for (i->pa[i->d] = rope->root; !i->pa[i->d]->is_bottom;) // descend to the leftmost leaf
		++i->d, i->pa[i->d] = i->pa[i->d - 1]->p;
}

const uint8_t *rope_itr_next_block(rpitr_t *i, int *n)
{
	const uint8_t *ret;
	assert(i->d < ROPE_MAX_DEPTH); // a B+ tree should not be that tall
	if (i->d < 0) return 0;
	*n = i->rope->block_len;
	ret = (uint8_t*)i->pa[i->d][i->ia[i->d]].p;
	while (i->d >= 0 && ++i->ia[i->d] == i->pa[i->d]->n) i->ia[i->d--] = 0; // backtracking
	if (i->d >= 0)
		while (!i->pa[i->d]->is_bottom) // descend to the leftmost leaf
			++i->d, i->pa[i->d] = i->pa[i->d - 1][i->ia[i->d - 1]].p;
	return ret;
}
