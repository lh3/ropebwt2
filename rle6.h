#ifndef RLE6_H_
#define RLE6_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	int rle_insert1(int block_len, uint8_t *block, int64_t x, int a, int64_t cnt[6], const int64_t end_cnt[6]);
	void rle_split(int block_len, uint8_t *block, uint8_t *new_block);
	void rle_count(int block_len, const uint8_t *block, int64_t cnt[6]);

#ifdef __cplusplus
}
#endif

#define RLE_CODEC 1

#if RLE_CODEC == 1

#define RLE_CONST 0x232235314C484440ULL
#define rle_bytes(_p) (1 << (RLE_CONST >> (*(_p)>>5<<3) & 3))
#define rle_runs(len, block) (*(uint32_t*)((block) + (len) - 4) >> 4)
/*
static inline int rle_dec(const uint8_t *p, int *c, int64_t *l)                                                                                                       
{
	const uint64_t x = *(const uint64_t*)p;
	int d = RLE_CONST >> (x>>5<<3) & 0xff, n_bytes = 1 << (d & 0x3);
	*c = x & 0x7;
	*l = (x & ((1ULL << (n_bytes<<3)) - 1)) >> 8 << (d>>4) | (d & 0xc) | (x>>3 & 0x3);
	return n_bytes;
}
*/
static inline int rle_dec(const uint8_t *p, int *c, int64_t *l)                                                                                                       
{
	*c = *p & 0x7;
	if ((*p&0x80) == 0) {
		*l = *p >> 3;
		return 1;
	} else if ((*p&0xC0) == 0x80) {
		*l = (uint64_t)p[1] << 3 | (*p >> 3 & 0x7);
		return 2;
	} else if ((*p&0xE0) == 0xC0) {
		*l = *(uint32_t*)p >> 8 << 2 | (*p >> 3 & 0x3);
		return 4;
	} else {
		*l = *(uint64_t*)p >> 8 << 2 | (*p >> 3 & 0x3);
		return 8;
	}
}

static inline int rle_enc(uint8_t *p, int c, int64_t l)
{ // FIXME: little-endian ONLY!!!
	if (l < 1LL<<4) {
		*p = l<<3 | c;
		return 1;
	} else if (l < 1LL<<11) {
		uint16_t *q = (uint16_t*)p;
		*q = c | (l&7)<<3 | 0x80 | l>>3<<8;
		return 2;
	} else if (l < 1LL<<26) {
		uint32_t *q = (uint32_t*)p;
		*q = c | (l&3)<<3 | 0xC0 | l>>2<<8;
		return 4;
	} else {
		uint64_t *q = (uint64_t*)p;
		*q = c | (l&3)<<3 | 0xE0 | l>>2<<8;
		return 8;
	}
}

#elif RLE_CODEC == 2

#define rle_runs(len, block) (*(uint32_t*)((block) + (len) - 4))

static inline int rle_dec(const uint8_t *p, int *c, int64_t *l)
{
	*c = *p&7; *l = *p>>3;
	return 1;
}

#endif

#endif
