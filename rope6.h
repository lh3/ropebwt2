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

	rope6_t *r6_init(int max_nodes, int block_len);
	void r6_destroy(rope6_t *rope);
	void r6_insert_string_io(rope6_t *rope, int l, uint8_t *str);
	void r6_insert_string_rlo(rope6_t *rope, int len, uint8_t *str);

	r6itr_t *r6_itr_init(const rope6_t *rope);
	const uint8_t *r6_itr_next(r6itr_t *i, int *n);
	
#ifdef __cplusplus
}
#endif

#endif
