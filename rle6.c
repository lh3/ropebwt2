#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "rle6.h"

#if RLE_CODEC == 1

/*****************************
 *** Variable-length codec ***
 *****************************/

// insert symbol $a after $x symbols in $str; marginal counts added to $cnt; returns the size increase
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
				c = tc, n_bytes = t_bytes, l = tl, z += l, p += t_bytes, cnt[tc] += tl;
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

void rle_check(int block_len, const uint8_t *block)
{
	uint32_t *p = (uint32_t*)(block + block_len - 4);
	const uint8_t *q = block, *end = block + (*p>>4);
	while (q < end) q += rle_bytes(q);
	assert(q == end);
}

// similar to rle_insert_core(), except that this function updates the total length kept in the last 4 bytes in an RLE block
int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6], const int64_t end_cnt[6])
{
	int m_bytes, diff;
	uint32_t *p = (uint32_t*)(block + block_len - 4);
	diff = rle_insert_core(*p>>4, block, x, a, rl, cnt, &m_bytes);
	*p = ((*p>>4) + diff) << 4 | ((*p&0xf) > m_bytes? (*p&0xf) : m_bytes);
	return (*p>>4) + 8 + (*p&0xf)*2 > block_len - 7? 1 : 0;
}

int rle_insert1(int block_len, uint8_t *block, int64_t x, int a, int64_t cnt[6], const int64_t end_cnt[6])
{
	return rle_insert(block_len, block, x, a, 1, cnt, end_cnt);
}

void rle_split(int block_len, uint8_t *block, uint8_t *new_block)
{
	uint32_t *r, *p = (uint32_t*)(block + block_len - 4);
	uint8_t *q = block, *end = block + (*p>>4>>1);
	while (q < end) q += rle_bytes(q);
	end = block + (*p>>4);
	memcpy(new_block, q, end - q);
	r = (uint32_t*)(new_block + block_len - 4);
	*r = (end - q) << 4 | (*p&0xf);
	*p = (q - block) << 4 | (*p&0xf);
}

void rle_count(int block_len, const uint8_t *block, int64_t cnt[6])
{
	uint32_t *p = (uint32_t*)(block + block_len - 4);
	const uint8_t *q = block, *end = block + (*p>>4);
	while (q < end) {
		int c;
		int64_t l;
		q += rle_dec(q, &c, &l);
		cnt[c] += l;
	}
}

void rle_print(int block_len, const uint8_t *block)
{
	uint32_t *p = (uint32_t*)(block + block_len - 4);
	const uint8_t *q = block, *end = block + (*p>>4);
	printf("%d\t%d\t", *p>>4, *p&0xf);
	while (q < end) {
		int c;
		int64_t l;
		q += rle_dec(q, &c, &l);
		printf("%c%ld", "$ACGTN"[c], (long)l);
	}
	putchar('\n');
}

#elif RLE_CODEC == 2

/*****************
 *** 5+3 codec ***
 *****************/

int rle_insert1(int block_len, uint8_t *block, int64_t x, int a, int64_t r[6], const int64_t c[6])
{ // IMPORTANT: always assume rl == 1
#define MAX_RUNLEN 31
#define _insert_after(_n, _s, _i, _b) if ((_i) + 1 != (_n)) memmove(_s+(_i)+2, _s+(_i)+1, (_n)-(_i)-1); _s[(_i)+1] = (_b); ++(_n)

	uint32_t *nptr = (uint32_t*)(block + block_len - 4);
	int i, len, l = 0, n = *nptr;
	uint8_t *s = block;

	if (n == 0) { // if $s is empty, that is easy
		for (i = 0; i < 6; ++i) r[i] = 0;
		s[n++] = 1<<3 | a;
		*nptr = n;
		return 0;
	}
	len = c[0] + c[1] + c[2] + c[3] + c[4] + c[5];
	if (x < len>>1) { // forwardly search for the run to insert
		for (i = 0; i < 6; ++i) r[i] = 0;
		do {
			l += *s>>3;
			r[*s&7] += *s>>3;
			++s;
		} while (l < x);
	} else { // backwardly search for the run to insert; this block has exactly the same functionality as the above
		for (i = 0; i < 6; ++i) r[i] = c[i];
		l = len, s += n;
		do {
			--s;
			l -= *s>>3;
			r[*s&7] -= *s>>3;
		} while (l >= x);
		l += *s>>3; r[*s&7] += *s>>3; ++s;
	}
	i = s - block; s = block;
	assert(i <= n);
	r[s[--i]&7] -= l - x; // $i now points to the left-most run where $a can be inserted
	if (l == x && i != n - 1 && (s[i+1]&7) == a) ++i; // if insert to the end of $i, check if we'd better to the start of ($i+1)
	if ((s[i]&7) == a) { // insert to a long $a run
		if (s[i]>>3 == MAX_RUNLEN) { // the run is full
			for (++i; i != n && (s[i]&7) == a; ++i); // find the end of the long run
			--i;
			if (s[i]>>3 == MAX_RUNLEN) { // then we have to add one run
				_insert_after(n, s, i, 1<<3|a);
			} else s[i] += 1<<3;
		} else s[i] += 1<<3;
	} else if (l == x) { // insert to the end of run; in this case, neither this and the next run is $a
		_insert_after(n, s, i, 1<<3 | a);
	} else if (i != n - 1 && (s[i]&7) == (s[i+1]&7)) { // insert to a long non-$a run
		int rest = l - x, c = s[i]&7;
		s[i] -= rest<<3;
		_insert_after(n, s, i, 1<<3 | a);
		for (i += 2; i != n && (s[i]&7) == c; ++i); // find the end of the long run
		--i;
		if ((s[i]>>3) + rest > MAX_RUNLEN) { // we cannot put $rest to $s[$i]
			rest = (s[i]>>3) + rest - MAX_RUNLEN;
			s[i] = MAX_RUNLEN<<3 | (s[i]&7);
			_insert_after(n, s, i, rest<<3 | c);
		} else s[i] += rest<<3;
	} else { // insert to a short run
		memmove(s + i + 3, s + i + 1, n - i - 1);
		s[i]  -= (l-x)<<3;
		s[i+1] = 1<<3 | a;
		s[i+2] = (l-x)<<3 | (s[i]&7);
		n += 2;
	}
	*nptr = n;
	return n + 2 > block_len - 4? 1 : 0;
}

void rle_split(int block_len, uint8_t *block, uint8_t *new_block)
{
	uint32_t *nptr = (uint32_t*)(block + block_len - 4);
	int beg = *nptr >> 1;
	memcpy(new_block, block + beg, (*nptr) - beg);
	*(uint32_t*)(new_block + block_len - 4) = *nptr - beg;
	*nptr = beg;
}

void rle_count(int block_len, const uint8_t *block, int64_t cnt[6])
{
	uint32_t *nptr = (uint32_t*)(block + block_len - 4);
	const uint8_t *p = block, *end = p + (*nptr);
	for (; p != end; ++p) cnt[*p&7] += *p>>3;
}

#endif
