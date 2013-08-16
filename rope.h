#ifndef ROPE_H_
#define ROPE_H_

#include <stdint.h>

struct rope_s;
typedef struct rope_s rope_t;

struct ropeitr_s;
typedef struct ropeitr_s ropeitr_t;

#ifdef __cplusplus
extern "C" {
#endif

	rope_t *rope_init(int max_nodes, int block_len);
	void rope_destroy(rope_t *rope);
	void rope_insert_string_io(rope_t *rope, int l, uint8_t *str);
	void rope_insert_string_rlo(rope_t *rope, int len, uint8_t *str);

	ropeitr_t *rope_itr_init(const rope_t *rope);
	const uint8_t *rope_itr_next(ropeitr_t *i, int *n);
	
#ifdef __cplusplus
}
#endif

#endif
