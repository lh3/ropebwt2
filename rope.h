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
	int64_t rope_insert_run(rope_t *rope, int64_t x, int a, int64_t rl);
	void rope_rank2a(const rope_t *rope, int64_t x, int64_t y, int64_t *cx, int64_t *cy);
	#define rope_rank1a(rope, x, cx) rope_rank2a(rope, x, -1, cx, 0)

	void rope_insert_string_io(rope_t *rope, const uint8_t *str);
	void rope_insert_string_rlo(rope_t *rope, const uint8_t *str);

	ropeitr_t *rope_itr_init(const rope_t *rope);
	const uint8_t *rope_itr_next(ropeitr_t *i, int *n);
	
#ifdef __cplusplus
}
#endif

#endif
