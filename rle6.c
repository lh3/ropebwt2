#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "rle6.h"

// #define RLE_ALT_FORWARD

const uint8_t rle_auxtab[8] = { 0x01, 0x11, 0x21, 0x31, 0x03, 0x13, 0x07, 0x17 };

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
		int c = -1, n_bytes = 0, n_bytes2, t = 0;
		uint8_t tmp[24];
		tot = ec[0] + ec[1] + ec[2] + ec[3] + ec[4] + ec[5];
		if (x <= tot>>1) { // forward
			z = 0; p = str;
#ifndef RLE_ALT_FORWARD
			while (z < x) {
				rle_dec1(p, c, l);
				z += l; cnt[c] += l;
			}
#else
			while (z < x) {
				if (LIKELY(*p>>7 == 0)) {
					c = *p & 7;
					l = *p >> 3;
					z += l; cnt[c] += l;
				} else if (*p>>6 != 2) {
					c = *p & 7;
					t = rle_auxtab[*p>>3&7];
					l = t >> 4;
					t &= 0xf;
				} else {
					int64_t s;
					l = l<<6 | (*p&0x3fL);
					s = --t? 0 : l;
					z += s; cnt[c] += s;
				}
				++p;
			}
#endif
			for (q = p - 1; *q>>6 == 2; --q);
		} else { // backward
			memcpy(cnt, ec, 48);
			z = tot; p = end;
			while (z >= x) {
				--p;
				if (*p>>6 != 2) {
					l |= *p>>7? (int64_t)rle_auxtab[*p>>3&7]>>4 << t : *p>>3;
					z -= l; cnt[*p&7] -= l;
					l = 0; t = 0;
				} else {
					l |= (*p&0x3fL) << t;
					t += 6;
				}
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
		if (c < 0) c = a, l = 0, pre = 0; // in this case, x==0 and the next run is different from $a
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
		if (n_bytes != n_bytes2 && end != p + n_bytes) // size changed
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
	return *p + 18 > block_len? 1 : 0;
}

int rle_insert1(int block_len, uint8_t *block, int64_t x, int a, int64_t cnt[6], const int64_t end_cnt[6])
{
	return rle_insert(block_len, block, x, a, 1, cnt, end_cnt);
}

void rle_split(int block_len, uint8_t *block, uint8_t *new_block)
{
	uint16_t *r, *p = (uint16_t*)(block + block_len - 2);
	uint8_t *end = block + *p, *q = block + (*p>>4>>1);
	while (*q>>6 == 2) --q;
	memcpy(new_block, q, end - q);
	r = (uint16_t*)(new_block + block_len - 2);
	*r = end - q; *p = q - block;
}

void rle_count(int block_len, const uint8_t *block, int64_t cnt[6])
{
	uint16_t *p = (uint16_t*)(block + block_len - 2);
	const uint8_t *q = block, *end = block + *p;
	while (q < end) {
		int c;
		int64_t l;
		rle_dec1(q, c, l);
		cnt[c] += l;
	}
}

void rle_print(int block_len, const uint8_t *block)
{
	uint16_t *p = (uint16_t*)(block + block_len - 2);
	const uint8_t *q = block, *end = block + *p;
	printf("%d\t", *p);
	while (q < end) {
		int c;
		int64_t l;
		rle_dec1(q, c, l);
		printf("%c%ld", "$ACGTN"[c], (long)l);
	}
	putchar('\n');
}
