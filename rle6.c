#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "rle6.h"

const uint8_t rle_auxtab[8] = { 0x01, 0x11, 0x21, 0x31, 0x03, 0x13, 0x07, 0x17 };

// insert symbol $a after $x symbols in $str; marginal counts added to $cnt; returns the size increase
int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6], const int64_t ec[6])
{
	uint16_t *nptr = rle_nptr(block_len, block);
	int diff;

	memset(cnt, 0, 48);
	if (*nptr == 0) {
		diff = rle_enc1(block, a, rl);
	} else {
		uint8_t *p, *end = block + *nptr, *q;
		int64_t pre, z, l = 0, tot;
		int c = -1, n_bytes = 0, n_bytes2, t = 0;
		uint8_t tmp[24];
		tot = ec[0] + ec[1] + ec[2] + ec[3] + ec[4] + ec[5];
		if (x == 0) {
			p = q = block; z = 0;
		} else if (x <= tot>>1) { // forward
			z = 0; p = block;
			while (z < x) {
				rle_dec1(p, c, l);
				z += l; cnt[c] += l;
			}
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
		if (z != x) cnt[c] -= z - x;
		pre = x - (z - l); p -= n_bytes;
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
		diff = n_bytes2 - n_bytes;
	}
	*nptr += diff;
	return *nptr + 18 > block_len? 1 : 0;
}

void rle_split(int block_len, uint8_t *block, uint8_t *new_block)
{
	uint16_t *r, *p = rle_nptr(block_len, block);
	uint8_t *end = block + *p, *q = block + (*p>>4>>1);
	while (*q>>6 == 2) --q;
	memcpy(new_block, q, end - q);
	r = rle_nptr(block_len, new_block);
	*r = end - q; *p = q - block;
}

void rle_count(int block_len, const uint8_t *block, int64_t cnt[6])
{
	const uint8_t *q = block, *end = q + *rle_nptr(block_len, block);
	while (q < end) {
		int c;
		int64_t l;
		rle_dec1(q, c, l);
		cnt[c] += l;
	}
}

void rle_print(int block_len, const uint8_t *block)
{
	uint16_t *p = rle_nptr(block_len, block);
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

void rle_rank1a(int block_len, const uint8_t *block, int64_t x, int64_t cnt[6], const int64_t ec[6])
{
	int64_t tot;
	const uint8_t *p;

	if (x == 0) return;
	tot = ec[0] + ec[1] + ec[2] + ec[3] + ec[4] + ec[5];
	if (x <= tot>>1) {
		int c = 0;
		int64_t l, z = 0;
		p = block;
		while (z < x) {
			rle_dec1(p, c, l);
			z += l; cnt[c] += l;
		}
		cnt[c] -= z - x;
	} else {
		int c, t = 0;
		int64_t l = 0, z = tot;
		for (c = 0; c != 6; ++c) cnt[c] += ec[c];
		p = block + *rle_nptr(block_len, block);
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
		cnt[*p&7] += x - z;
	}
}
