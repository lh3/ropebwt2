#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include "mrope.h"

/*******************************
 *** Single-string insertion ***
 *******************************/

mrope_t *mr_init(int max_nodes, int block_len, int sorting_order)
{
	int a;
	mrope_t *r;
	assert(sorting_order >= 0 && sorting_order <= 2);
	r = calloc(1, sizeof(mrope_t));
	r->so = sorting_order;
	for (a = 0; a != 6; ++a)
		r->r[a] = rope_init(max_nodes, block_len);
	return r;
}

void mr_destroy(mrope_t *r)
{
	int a;
	for (a = 0; a != 6; ++a)
		if (r->r[a]) rope_destroy(r->r[a]);
}

int64_t mr_insert_string_io(mrope_t *r, const uint8_t *str)
{
	const uint8_t *p;
	int64_t x;
	int a;
	for (a = 0, x = 0; a != 6; ++a)
		x += r->r[a]->c[0];
	for (p = str, a = 0; *p; a = *p++) {
		x = rope_insert_run(r->r[a], x, *p, 1, 0);
		while (--a >= 0) x += r->r[a]->c[*p];
	}
	return rope_insert_run(r->r[a], x, *p, 1, 0);
}

int64_t mr_insert_string_rlo(mrope_t *r, const uint8_t *str, int is_comp)
{
	int b;
	int64_t tl[6], tu[6], l, u;
	const uint8_t *p;
	for (l = u = 0, b = 0; b != 6; ++b) u += r->r[b]->c[0];
	for (p = str, b = 0; *p; b = *p++) {
		int a;
		if (l != u) {
			int64_t cnt = 0;
			rope_rank2a(r->r[b], l, u, tl, tu);
			if (is_comp && *p != 5) {
				for (a = 4; a > *p; --a) l += tu[a] - tl[a];
				l += tu[0] - tl[0];
			} else for (a = 0; a < *p; ++a) l += tu[a] - tl[a];
			rope_insert_run(r->r[b], l, *p, 1, 0);
			while (--b >= 0) cnt += r->r[b]->c[*p];
			l = cnt + tl[*p]; u = cnt + tu[*p];
		} else {
			l = rope_insert_run(r->r[b], l, *p, 1, 0);
			while (--b >= 0) l += r->r[b]->c[*p];
			u = l;
		}
	}
	return rope_insert_run(r->r[b], l, 0, 1, 0);
}

int64_t mr_insert1(mrope_t *mr, const uint8_t *str)
{
	if (mr->so == MR_SO_RLO) return mr_insert_string_rlo(mr, str, 0);
	else if (mr->so == MR_SO_RCLO) return mr_insert_string_rlo(mr, str, 1);
	else return mr_insert_string_io(mr, str);
}

/**********************
 *** Mrope iterator ***
 **********************/

void mr_itr_first(mrope_t *r, mritr_t *i, int to_free)
{
	i->a = 0; i->r = r; i->to_free = to_free;
	rope_itr_first(i->r->r[0], &i->i);
}

const uint8_t *mr_itr_next_block(mritr_t *i)
{
	const uint8_t *s;
	if (i->a >= 6) return 0;
	while ((s = rope_itr_next_block(&i->i)) == 0) {
		if (i->to_free) {
			rope_destroy(i->r->r[i->a]);
			i->r->r[i->a] = 0;
		}
		if (++i->a == 6) return 0;
		rope_itr_first(i->r->r[i->a], &i->i);
	}
	return i->a == 6? 0 : s;
}

/***********
 *** I/O ***
 ***********/

void mr_dump(mrope_t *mr, FILE *fp)
{
	int i;
	fwrite("RB\2", 1, 3, fp);
	fwrite(&mr->so, 1, 1, fp);
	for (i = 0; i < 6; ++i)
		rope_dump(mr->r[i], fp);
}

mrope_t *mr_restore(FILE *fp)
{
	mrope_t *mr;
	uint8_t magic[4];
	int i;
	fread(magic, 1, 4, fp);
	mr = calloc(1, sizeof(mrope_t));
	mr->so = magic[3];
	for (i = 0; i < 6; ++i)
		mr->r[i] = rope_restore(fp);
	return mr;
}

void mr_print_tree(const mrope_t *mr)
{
	int a;
	for (a = 0; a < 6; ++a)
		rope_print_node(mr->r[a]->root);
	putchar('\n');
}

/*****************************************
 *** Inserting multiple strings in RLO ***
 *****************************************/

typedef struct {
	uint64_t l;
	uint64_t u:61, c:3;
	const uint8_t *p;
} triple64_t;

typedef const uint8_t *cstr_t;

#define rope_comp6(c) ((c) >= 1 && (c) <= 4? 5 - (c) : (c))

static void mr_insert_multi_aux(rope_t *rope, int64_t m, triple64_t *a, int is_comp)
{
	int64_t k, beg;
	rpcache_t cache;
	memset(&cache, 0, sizeof(rpcache_t));
	for (k = 0; k != m; ++k) // set the base to insert
		a[k].c = *a[k].p++;
	for (k = 1, beg = 0; k <= m; ++k) {
		if (k == m || a[k].u != a[k-1].u) {
			int64_t x, i, l = a[beg].l, u = a[beg].u, tl[6], tu[6], c[6];
			int start, end, step, b;
			if (l == u) {
				memset(tl, 0, 48);
				memset(tu, 0, 48);
			} else rope_rank2a(rope, l, u, tl, tu);
			memset(c, 0, 48);
			for (i = beg; i < k; ++i) ++c[a[i].c];
			// insert sentinel
			if (c[0]) rope_insert_run(rope, l, 0, c[0], &cache);
			// insert A/C/G/T
			x =  l + c[0] + (tu[0] - tl[0]);
			if (is_comp) start = 4, end = 0, step = -1;
			else start = 1, end = 5, step = 1;
			for (b = start; b != end; b += step) {
				int64_t size = tu[b] - tl[b];
				if (c[b]) {
					tl[b] = rope_insert_run(rope, x, b, c[b], &cache);
					tu[b] = tl[b] + size;
				}
				x += c[b] + size;
			}
			// insert N
			if (c[5]) {
				tu[5] -= tl[5];
				tl[5] = rope_insert_run(rope, x, 5, c[5], &cache);
				tu[5] += tl[5];
			}
			// update a[]
			for (i = beg; i < k; ++i) {
				triple64_t *p = &a[i];
				p->l = tl[p->c], p->u = tu[p->c];
			}
			beg = k;
		}
	}
}

typedef struct {
	volatile int *n_fin_workers;
	volatile int to_run;
	int to_exit;
	mrope_t *mr;
	int b, is_comp;
	int64_t m;
	triple64_t *a;
} worker_t;

static void *worker(void *data)
{
	worker_t *w = (worker_t*)data;
	struct timespec req, rem;
	req.tv_sec = 0; req.tv_nsec = 1000000;
	while (!w->to_exit) {
		while (!__sync_bool_compare_and_swap(&w->to_run, 1, 0)) nanosleep(&req, &rem); // wait for the signal from the master thread
		if (w->m) mr_insert_multi_aux(w->mr->r[w->b], w->m, w->a, w->is_comp);
		__sync_add_and_fetch(w->n_fin_workers, 1);
	}
	return 0;
}

void mr_insert_multi(mrope_t *mr, int64_t len, const uint8_t *s, int is_thr)
{
	int64_t k, m, n0;
	int b, is_srt = (mr->so != 0), is_comp = (mr->so == 2);
	volatile int n_fin_workers = 0;
	triple64_t *a[2], *curr, *prev, *swap;
	pthread_t *tid = 0;
	worker_t *w = 0;

	assert(len > 0 && s[len-1] == 0);
	{ // split into short strings
		cstr_t p, q, end = s + len;
		for (p = s, m = 0; p != end; ++p) // count #sentinels
			if (*p == 0) ++m;
		curr = a[0] = malloc(m * sizeof(triple64_t));
		prev = a[1] = malloc(m * sizeof(triple64_t));
		for (p = q = s, k = 0; p != end; ++p) // find the start of each string
			if (*p == 0) prev[k++].p = q, q = p + 1;
	}

	for (k = n0 = 0; k < 6; ++k) n0 += mr->r[k]->c[0];
	for (k = 0; k != m; ++k) {
		if (is_srt) prev[k].l = 0, prev[k].u = n0;
		else prev[k].l = prev[k].u = n0 + k;
		prev[k].c = 0;
	}
	mr_insert_multi_aux(mr->r[0], m, prev, is_comp); // insert the first (actually the last) column

	if (is_thr) {
		tid = alloca(4 * sizeof(pthread_t));
		w = alloca(4 * sizeof(worker_t));
		memset(w, 0, 4 * sizeof(worker_t));
		for (b = 0; b < 4; ++b) {
			w[b].mr = mr, w[b].b = b + 1, w[b].is_comp = is_comp;
			w[b].n_fin_workers = &n_fin_workers;
		}
		for (b = 0; b < 4; ++b) pthread_create(&tid[b], 0, worker, &w[b]);
	}

	n0 = 0;
	while (m) {
		int64_t c[6], ac[6];
		triple64_t *q[6];

		memset(c, 0, 48);
		for (k = n0; k != m; ++k) ++c[prev[k].c]; // counting
		for (q[0] = curr + n0, b = 1; b < 6; ++b) q[b] = q[b-1] + c[b-1];
		if (n0 + c[0] < m) {
			for (k = n0; k != m; ++k) *q[prev[k].c]++ = prev[k]; // sort
			for (b = 0; b < 6; ++b) q[b] -= c[b];
		}
		n0 += c[0];

		if (is_thr) {
			struct timespec req, rem;
			req.tv_sec = 0; req.tv_nsec = 1000000;
			for (b = 0; b < 4; ++b) {
				w[b].a = q[b+1], w[b].m = c[b+1];
				if (n0 == m) w[b].to_exit = 1; // signal the workers to exit
				while (!__sync_bool_compare_and_swap(&w[b].to_run, 0, 1)); // signal the workers to start
			}
			if (c[5]) mr_insert_multi_aux(mr->r[5], c[5], q[5], is_comp); // the master thread processes the "N" bucket
			while (!__sync_bool_compare_and_swap(&n_fin_workers, 4, 0)) // wait until all 4 workers finish
				nanosleep(&req, &rem);
		} else {
			for (b = 1; b < 6; ++b)
				if (c[b]) mr_insert_multi_aux(mr->r[b], c[b], q[b], is_comp);
		}
		if (n0 == m) break;

		memset(ac, 0, 48);
		for (b = 1; b < 6; ++b) { // update the intervals to account for buckets ahead
			int a;
			for (a = 0; a < 6; ++a) ac[a] += mr->r[b-1]->c[a];
			for (k = 0; k < c[b]; ++k) {
				triple64_t *p = &q[b][k];
				p->l += ac[p->c]; p->u += ac[p->c];
			}
		}
		swap = curr, curr = prev, prev = swap;
	}
	if (is_thr) for (b = 0; b < 4; ++b) pthread_join(tid[b], 0);
	free(a[0]); free(a[1]);
}
