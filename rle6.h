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

#define rle_runs(len, block) (*(uint16_t*)((block) + (len) - 2))

#define rle_traverse(p, end, func) do { \
		if (p != (end)) { \
			int c = *p & 7, t = 3; \
			int64_t l = *p>>7? *p>>3&7 : *p>>3; \
			++p; \
			while (p != (end)) { \
				if (*p>>6 != 2) { \
					func(c, l); \
					c = *p & 7; \
					l = *p>>7? *p>>3&7 : *p>>3; \
					t = 3; \
				} else { \
					l |= (*p&0x3fLL) << t; \
					t += 6; \
				} \
				++p; \
			} \
			func(c, l); \
		} \
	} while (0)

#define rle_dec1(p, end, c, l) do { \
		c = *p & 7; \
		if (*p>>7 == 0) { \
			l = *p++ >> 3; \
		} else { \
			int t = 3; \
			l = *p++ >> 3 & 7; \
			while (p != end && *p>>6 == 2) \
				l |= (*p++ & 0x3fLL) << t; \
		} \
	} while (0)

static inline int rle_enc1(uint8_t *p, int c, int64_t l)
{
	if (l < 1LL<<4) {
		*p = l << 3 | c;
		return 1;
	} else if (l < 1LL<<9) {
		*p++ = 0xC0 | (l&7)<<3 | c;
		*p = 0x80 | l>>3;
		return 2;
	} else {
		uint8_t *p0 = p;
		*p++ = 0xC0 | (l&7)<<3 | c;
		for (l >>= 3; l; l >>= 6) *p++ = 0x80 | (l&0x3f);
		return p - p0;
	}
}

#endif
