#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

/**********************
 *** Insert to leaf ***
 **********************/

// insert symbol $a after $x symbols in $str; marginal counts added to $cnt
int rle_insert_core(int len, uint8_t *str, int64_t x, int a, int64_t rl, int64_t cnt[6], int *m_bytes)
{
	memset(cnt, 0, 48);
	if (len == 0) {
		return (*m_bytes = rle_enc(str, a, rl));
	} else {
		uint8_t *p = str, *end = str + len;
		int64_t pre, z = 0, l = 0;
		int c = -1, n_bytes = 0, n_bytes2;
		uint8_t tmp[24];
		while (z < x) {
			n_bytes = rle_dec(p, &c, &l);
			z += l; p += n_bytes; cnt[c] += l;
		}
		if (x == z && a != c && p < end) { // then try the next run
			int t_bytes, tc;
			int64_t tl;
			t_bytes = rle_dec(p, &tc, &tl);
			if (a == tc)
				c = tc, n_bytes = t_bytes, l = tl, z += l, p += t_bytes;
		}
		if (c < 0) c = a, l = 0, pre = 0; // in this case, x==0 and the next run is different from $a
		else cnt[c] -= z - x, pre = x - (z - l), p -= n_bytes;
		if (a == c) { // insert to the same run
			n_bytes2 = rle_enc(tmp, c, l + rl);
			*m_bytes = n_bytes2;
		} else if (x == z) { // at the end; append to the existing run
			p += n_bytes; n_bytes = 0;
			n_bytes2 = *m_bytes = rle_enc(tmp, a, rl);
		} else { // break the current run
			n_bytes2 = rle_enc(tmp, c, pre);
			n_bytes2 += (*m_bytes = rle_enc(tmp + n_bytes2, a, rl));
			n_bytes2 += rle_enc(tmp + n_bytes2, c, l - pre);
		}
		if (n_bytes != n_bytes2) // size changed
			memmove(p + n_bytes2, p + n_bytes, end - p - n_bytes);
		memcpy(p, tmp, n_bytes2);
		return n_bytes2 - n_bytes;
	}
}

int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6])
{
	int m_bytes, diff;
	uint32_t *p;
	p = (uint32_t*)(block + block_len - 4);
	diff = rle_insert_core(*p>>4, block, x, a, rl, cnt, &m_bytes);
	*p = ((*p>>4) + diff) << 4 | ((*p&0xf) > m_bytes? (*p&0xf) : m_bytes);
	return (*p>>4) + 8 + (*p&0xf)*2 > block_len - 7? 1 : 0;
}

void rle_print(int block_len, const uint8_t *block)
{
	uint32_t *p = (uint32_t*)(block + block_len - 4);
	const uint8_t *q = block, *end = block + (*p>>4);
	int c;
	int64_t l;
	printf("%d\t%d\t", *p>>4, *p&0xf);
	while (q < end) {
		q += rle_dec(q, &c, &l);
		printf("%c%ld", "$ACGTN"[c], (long)l);
	}
	putchar('\n');
}
