#ifndef ROPE6_H_
#define ROPE6_H_

#include <stdint.h>

struct rope6_s;
typedef struct rope6_s rope6_t;

struct r6itr_s;
typedef struct r6itr_s r6itr_t;

#ifdef __cplusplus
extern "C" {
#endif

	int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6]);
	void rle_split(int block_len, uint8_t *block, uint8_t *new_block);
	void rle_print(int block_len, const uint8_t *block);

	rope6_t *r6_init(int max_nodes, int block_len);
	void r6_destroy(rope6_t *rope);
	void r6_insert_string_io(rope6_t *rope, int l, uint8_t *str);

	r6itr_t *r6_itr_init(const rope6_t *rope);
	const uint8_t *r6_itr_next(r6itr_t *i, int *n);
	
#ifdef __cplusplus
}
#endif

#define RLE_CONST 0x232235314C484440ULL
#define rle_bytes(_p) (1 << (RLE_CONST >> (*(_p)>>5<<3) & 3))

static inline int rle_dec(const uint8_t *p, int *c, int64_t *l)
{ // FIXME: little-endian ONLY!!!
	const uint64_t *q = (const uint64_t*)p;
	int d = RLE_CONST >> (*p>>5<<3) & 0xff, n_bytes = 1 << (d & 0x3);
	*c = *p & 0x7;
	*l = (*q>>8 & ((1ULL << ((n_bytes-1)<<3)) - 1)) << (d>>4) | (d & 0xc) | (*p>>3 & 0x3);
	return n_bytes;
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
