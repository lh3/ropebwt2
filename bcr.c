#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "rope.h"
#include "rle.h"
#include "bcr.h"

typedef const uint8_t *cstr_t;

typedef struct {
	int64_t u, v; // $u: position in partial BWT:61, base:3; $v: seq_id:48, seq_len:16
} pair64_t;

typedef struct {
	int64_t n, ac[6];
	rope_t *r;
	pair64_t *a;
} bucket_t;

struct bcr_s {
	int flag;
	int64_t m, tot, ac[6];
	bucket_t bwt[6];
	volatile int proc_cnt;
};

/******************
 *** Radix sort ***
 ******************/

#define rstype_t pair64_t
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

/******************************
 *** Classify pair64_t::u&7 ***
 ******************************/

void rs_classify_alt(rstype_t *beg, rstype_t *end, int64_t *ac) // very similar to the first half of rs_sort()
{
	rsbucket_t *k, b[8], *be = b + 8;
	for (k = b; k != be; ++k) k->b = beg + ac[k - b];
	for (k = b; k != be - 1; ++k) k->e = k[1].b;
	k->e = end;
	for (k = b; k != be;) {
		if (k->b != k->e) {
			rsbucket_t *l;
			if ((l = b + ((*k->b).u&7)) != k) {
				rstype_t tmp = *k->b, swap;
				do {
					swap = tmp; tmp = *l->b; *l->b++ = swap;
					l = b + (tmp.u&7);
				} while (l != k);
				*k->b++ = tmp;
			} else ++k->b;
		} else ++k;
	}
}

bcr_t *bcr_init(int max_nodes, int block_len)
{
	bcr_t *b;
	int a;
	b = calloc(1, sizeof(bcr_t));
	for (a = 0; a != 6; ++a)
		b->bwt[a].r = rope_init(max_nodes, block_len);
	return b;
}

static pair64_t *set_bucket(bcr_t *bcr, pair64_t *a, int pos)
{
	int64_t k, c[8], ac[8], m;
	int j, l;
	// compute the absolute position in the new BWT
	memset(c, 0, 64);
	if (pos) { // a[k].u computed in next_bwt() doesn't consider symbols inserted to other buckets. We need to fix this in the following code block.
		int b;
		for (b = m = 0; b < 6; ++b) { // loop through each bucket
			int64_t pc[8]; // partial counts
			bucket_t *bwt = &bcr->bwt[b]; // the bucket
			memcpy(pc, c, 64); // the accumulated counts prior to the current bucket
			for (k = 0; k < bwt->n; ++k) {
				pair64_t *u = &bwt->a[k];
				if ((u->u&7) == 0) continue; // come to the beginning of a string; no need to consider it in the next round
				u->u += pc[u->u&7]<<3; // correct for symbols inserted to other buckets
				++c[u->u&7];
				if (m == u - a) ++m;
				else a[m++] = *u;
			}
		}
		bcr->m = m; // $m is the new size of the $a array
	} else c[0] = bcr->m;
	for (k = 1, ac[0] = 0; k < 8; ++k) ac[k] = ac[k - 1] + c[k - 1]; // accumulative counts; NB: MUST BE "8"; otherwise rs_classify_alt() will fail
	for (k = 0; k < bcr->m; ++k) a[k].u += ac[a[k].u&7]<<3;
	// radix sort into each bucket; in the non-RLO mode, we can use a stable counting sort here and skip the other rs_sort() in next_bwt()
	rs_classify_alt(a, a + bcr->m, ac); // This is a bottleneck.
	for (j = 0; j < 6; ++j) bcr->bwt[j].a = a + ac[j];
	// update counts: $bcr->bwt[j].ac[l] equals the number of symbol $l prior to bucket $j; needed by next_bwt()
	for (l = 0; l < 6; ++l)
		for (j = 1, bcr->bwt[0].ac[l] = 0; j < 6; ++j)
			bcr->bwt[j].ac[l] = bcr->bwt[j-1].ac[l] + bcr->bwt[j-1].r->c[l];
	for (j = 0; j < 6; ++j) bcr->bwt[j].n = c[j], bcr->ac[j] += ac[j];
	bcr->tot += bcr->m;
	return a;
}

static void next_bwt(bcr_t *bcr, const cstr_t *seq, int class, int pos)
{
	int64_t k, l, beg, old_u, new_u, streak;
	bucket_t *bwt = &bcr->bwt[class];

	if (bwt->n == 0) return;
	for (k = 0; k < bwt->n; ++k) { // compute the relative position in the old bucket
		pair64_t *u = &bwt->a[k];
		int c = seq[u->v][pos];
		if (bcr->flag & BCR_F_COMP)
			c = c >= 1 && c <= 4? 5 - c : c; // complement $c. This is for RLO. $c will be complemented back later after sorting
		u->u = ((u->u>>3) - bcr->ac[class])<<3 | c;
	}
	for (k = bcr->tot<<3, l = 0; k; k >>= 1, ++l);
	rs_sort(bwt->a, bwt->a + bwt->n, 8, l > 7? l - 7 : 0); // sort by the absolute position in the new BWT
	for (k = 1, beg = 0; k <= bwt->n; ++k)
		if (k == bwt->n || bwt->a[k].u>>3 != bwt->a[k-1].u>>3) {
			if (k - beg > 1) {
				pair64_t *i, *u, *end = bwt->a + k, *start = &bwt->a[beg];
				for (i = start + 1, u = start; i < end; ++i)
					if ((u->u&7) == (i->u&7)) i->u = u->u;
					else i->u += (i - start)<<3, u = i;
			}
			beg = k;
		}
	for (k = 0, streak = 0, old_u = new_u = -1; k < bwt->n; ++k) {
		pair64_t *u = &bwt->a[k];
		int a = u->u&7;
		if (bcr->flag & BCR_F_COMP)
			a = a >= 1 && a <= 4? 5 - a : a; // complement back
		if (u->u != old_u) { // in the non-RLO mode, we always come to the following block
			l = rope_insert_run(bwt->r, u->u>>3, a, 1);
			old_u = u->u;
			new_u = u->u = (l + bcr->ac[a] + bwt->ac[a])<<3 | a; // compute the incomplete position in the new BWT
			streak = 0;
		} else {
			rope_insert_run(bwt->r, (u->u>>3) + (++streak), a, 1);
			u->u = new_u;
		}
	}
}

void bcr_destroy(bcr_t *b)
{
	int i;
	for (i = 0; i < 6; ++i)
		rope_destroy(b->bwt[i].r);
	free(b);
}

void bcr_print(const bcr_t *b)
{
	int a;
	for (a = 0; a < 6; ++a) {
		rpitr_t itr;
		int len;
		const uint8_t *block;
		rope_t *r6 = b->bwt[a].r;
		rope_itr_first(r6, &itr);
		while ((block = rope_itr_next_block(&itr, &len)) != 0) {
			const uint8_t *q = block + 2, *end = block + 2 + *rle_nptr(block);
			while (q < end) {
				int c = 0;
				int64_t j, l;
				rle_dec1(q, c, l);
				for (j = 0; j < l; ++j) putchar("$ACGTN"[c]);
			}
		}
	}
	putchar('\n');
}

typedef struct {
	bcr_t *bcr;
	cstr_t *seq;
	int class;
	volatile int to_proc;
	int64_t pos, max_len;
} worker_t;

static int worker_aux(worker_t *w)
{
	struct timespec req, rem;
	req.tv_sec = 0; req.tv_nsec = 1000000;
	while (!__sync_bool_compare_and_swap(&w->to_proc, 1, 0))
		nanosleep(&req, &rem);
	next_bwt(w->bcr, w->seq, w->class, w->pos);
	__sync_add_and_fetch(&w->bcr->proc_cnt, 1);
	return (w->max_len == w->pos);
}

static void *worker(void *data)
{
	while (worker_aux(data) == 0);
	return 0;
}

void bcr_insert(bcr_t *b, int64_t len, const uint8_t *s, int flag)
{
	int n_threads, i, c;
	int64_t k, pos, max_len;
	pthread_t *tid = 0;
	worker_t *w = 0;
	pair64_t *a;
	cstr_t p, q, end = s + len, *seq;

	assert(len > 0 && s[len-1] == 0);
	max_len = 0;
	for (p = s, b->m = 0; p != end; ++p) // count #sentinels
		if (*p == 0) ++b->m;
	seq = malloc(b->m * sizeof(cstr_t));
	for (p = q = s, i = 0; p != end; ++p) { // find the start of each string
		if (*p == 0) {
			max_len = max_len > p - q? max_len : p - q;
			seq[i++] = q, q = p + 1;
		}
	}

	b->flag = flag;
	n_threads = (flag & BCR_F_THR)? 5 : 1;
	if (n_threads > 1) {
		tid = alloca(n_threads * sizeof(pthread_t)); // tid[0] is not used, as the worker 0 is launched by the master
		w = alloca(n_threads * sizeof(worker_t));
		memset(w, 0, n_threads * sizeof(worker_t));
		for (i = 0; i < n_threads; ++i)
			w[i].class = i + 1, w[i].bcr = b, w[i].seq = seq, w[i].max_len = max_len;
		for (i = 1; i < n_threads; ++i) pthread_create(&tid[i], 0, worker, &w[i]);
	}
	a = malloc(b->m * 16);
	for (k = 0; k < b->m; ++k) // keep the sequence lengths in the $a array: reduce memory and cache misses
		a[k].u = (flag&BCR_F_RLO)? 0 : (k + b->ac[1]) << 3, a[k].v = k;
	
	for (pos = 0; pos <= max_len; ++pos) { // "==" to add the sentinels
		a = set_bucket(b, a, pos);
		if (pos) {
			if (n_threads > 1) {
				struct timespec req, rem;
				req.tv_sec = 0; req.tv_nsec = 1000000;
				for (c = 0; c < n_threads; ++c) {
					volatile int *p = &w[c].to_proc;
					w[c].pos = pos;
					while (!__sync_bool_compare_and_swap(p, 0, 1));
				}
				worker_aux(&w[0]);
				while (!__sync_bool_compare_and_swap(&b->proc_cnt, n_threads, 0))
					nanosleep(&req, &rem);
			} else for (c = 1; c <= 5; ++c) next_bwt(b, seq, c, pos);
		} else next_bwt(b, seq, 0, pos);
	}
	for (i = 1; i < n_threads; ++i) pthread_join(tid[i], 0);
	free(a); free(seq);
}
