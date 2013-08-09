#ifndef RLE6_H_
#define RLE6_H_

#include <stdint.h>

#ifdef __GNUC__
#define LIKELY(x) __builtin_expect((x),1)
#else
#define LIKELY(x) (x)
#endif
#ifdef __cplusplus

extern "C" {
#endif

	int rle_insert1(int block_len, uint8_t *block, int64_t x, int a, int64_t cnt[6], const int64_t end_cnt[6]);
	void rle_split(int block_len, uint8_t *block, uint8_t *new_block);
	void rle_count(int block_len, const uint8_t *block, int64_t cnt[6]);

#ifdef __cplusplus
}
#endif

/******************
 *** 43+3 codec ***
 ******************/

#define RLE_CONST 0x8422000011111111ULL
#define rle_bytes(_p) (RLE_CONST >> (*(_p)>>4<<2) & 0xf)
#define rle_runs(len, block) (*(uint16_t*)((block) + (len) - 2))

// decode one run (c,l) and move the pointer p
#define rle_dec1(p, c, l) do { \
		(c) = *(p) & 7; \
		if (LIKELY((*(p)&0x80) == 0)) { \
			(l) = *(p)++ >> 3; \
		} else if (LIKELY(*(p)>>5 == 6)) { \
			(l) = (*(p)&0x18L)<<3L | ((p)[1]&0x7fL); \
			(p) += 2; \
		} else { \
			int i, n = rle_bytes(p); \
			(l) = (*(p)&8LL) << (n == 4? 15 : 39); \
			for (i = 1; i < n; ++i) \
				(l) = ((l)<<6) | ((p)[i]&0x7f); \
			(p) += n; \
		} \
	} while (0)

static inline int rle_enc(uint8_t *p, int c, int64_t l)
{
	if (l < 1LL<<4) {
		*p = l << 3 | c;
		return 1;
	} else if (l < 1LL<<8) {
		*p = 0xC0 | l >> 6 << 3 | c;
		p[1] = 0x80 | (l & 0x3f);
		return 2;
	} else if (l < 1LL<<19) {
		*p = 0xE0 | l >> 18 << 3 | c;
		p[1] = 0x80 | (l >> 12 & 0x3f);
		p[2] = 0x80 | (l >> 6 & 0x3f);
		p[3] = 0x80 | (l & 0x3f);
		return 4;
	} else {
		int i;
		uint64_t mask = 0x3FULL << 36;
		*p = 0xF0 | l >> 42 << 3 | c;
		for (i = 1; i < 8; ++i, mask >>= 6)
			p[i] = 0x80 | (l & mask);
		return 8;
	}
}

#endif
