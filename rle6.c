#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "rle6.h"

/******************
 *** 43+3 codec ***
 ******************/

// insert symbol $a after $x symbols in $str; marginal counts added to $cnt; returns the size increase
int rle_insert_core(int len, uint8_t *str, int64_t x, int a, int64_t rl, int64_t cnt[6], const int64_t ec[6])
{
	memset(cnt, 0, 48);
	if (len == 0) {
		return rle_enc1(str, a, rl);
	} else {
		uint8_t *p, *end = str + len, *q;
		int64_t pre, z, l = 0, tot;
		int c = 0, n_bytes = 0, n_bytes2;
		uint8_t tmp[24];
		tot = ec[0] + ec[1] + ec[2] + ec[3] + ec[4] + ec[5];
		if (x <= tot>>1) {
			z = 0; p = str;
			while (z < x) {
				rle_dec1(p, c, l);
				z += l; cnt[c] += l;
			}
			for (q = p - 1; *q>>6 == 2; --q);
		} else {
			memcpy(cnt, ec, 48);
            z = tot; p = end;
			while (z >= x) {
				--p;
				if (*p>>6 != 2) {
					l = *p>>7? l<<3|(*p>>3&7) : *p>>3;
					z -= l; cnt[*p&7] -= l;
					l = 0;
				} else l = l<<6 | (*p&0x3f);
			}
			q = p;
			rle_dec1(p, c, l);
			z += l; cnt[c] += l;
		}
		n_bytes = p - q;
		if (x == z && a != c && p < end) { // then try the next run
			int tc;
			int64_t tl;
			q = p;
			rle_dec1(q, tc, tl);
			if (a == tc)
				c = tc, n_bytes = q - p, l = tl, z += l, p = q, cnt[tc] += tl;
		}
		if (l == 0) c = a, l = 0, pre = 0; // in this case, x==0 and the next run is different from $a
		else cnt[c] -= z - x, pre = x - (z - l), p -= n_bytes;
		if (a == c) { // insert to the same run
			n_bytes2 = rle_enc1(tmp, c, l + rl);
		} else if (x == z) { // at the end; append to the existing run
			p += n_bytes; n_bytes = 0;
			n_bytes2 = rle_enc1(tmp, a, rl);
		} else { // break the current run
			n_bytes2 = rle_enc1(tmp, c, pre);
			n_bytes2 += rle_enc1(tmp + n_bytes2, a, rl);
			n_bytes2 += rle_enc1(tmp + n_bytes2, c, l - pre);
		}
		if (n_bytes != n_bytes2) // size changed
			memmove(p + n_bytes2, p + n_bytes, end - p - n_bytes);
		memcpy(p, tmp, n_bytes2);
		return n_bytes2 - n_bytes;
	}
}

int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6], const int64_t end_cnt[6])
{
	int diff;
	uint16_t *p = (uint16_t*)(block + block_len - 2);
	diff = rle_insert_core(*p, block, x, a, rl, cnt, end_cnt);
	*p += diff;
	block[*p] = 0;
	return *p + 19 > block_len? 1 : 0;
}

int rle_insert1(int block_len, uint8_t *block, int64_t x, int a, int64_t cnt[6], const int64_t end_cnt[6])
{
//	fprintf(stdout, "%d\t%lld\t", a, x); rle_print(block_len, block); if (x == 13) exit(0);
	return rle_insert(block_len, block, x, a, 1, cnt, end_cnt);
}

void rle_split(int block_len, uint8_t *block, uint8_t *new_block)
{
	uint16_t *r, *p = (uint16_t*)(block + block_len - 2);
	uint8_t *q = block + (*p>>4>>1), *end;
	while (*q>>6 == 2) ++q;
	end = block + *p;
	memcpy(new_block, q, end - q);
	r = (uint16_t*)(new_block + block_len - 2);
	*r = end - q; *p = q - block;
	block[*p] = 0; new_block[*r] = 0;
}

void rle_count(int block_len, const uint8_t *block, int64_t cnt[6])
{
	const uint8_t *end = block + *(uint16_t*)(block + block_len - 2);
	#define func(c, l) cnt[c] += l
	rle_traverse(block, end, func);
	#undef func
}

void rle_print(int block_len, const uint8_t *block)
{
	const uint8_t *end = block + *(uint16_t*)(block + block_len - 2);
	#define func(c, l) printf("%c%ld", "$ACGTN"[c], (long)l)
	rle_traverse(block, end, func);
	#undef func
	putchar('\n');
}
