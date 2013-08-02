#ifndef ROPE6_H_
#define ROPE6_H_

#include <stdint.h>

static inline int rle_dec(const uint8_t *p, int *c, int64_t *l)
{ // FIXME: little-endian ONLY!!!
	const uint64_t rle_const = 0x232235314C484440ULL;
	const uint64_t *q = (const uint64_t*)p;
	int d = rle_const >> (*p>>5<<2) & 0xff, n_bytes = 1 << (d & 0x3);
	*c = *p & 0x7;
	*l = (*q>>8 & ((1ULL << ((n_bytes-1)<<3)) - 1)) << (d>>4) | (d & 0xc) | (*p>>3 & 0x3);
	return n_bytes;
}

static inline int rle_enc1(uint8_t *p, int c)
{
	*p = 1<<3 | c;
	return 1;
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

#endif
