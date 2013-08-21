#ifndef ROPE_H_
#define ROPE_H_

#include <stdint.h>

#define ROPE_MAX_DEPTH 80

typedef struct rpnode_s {
	struct rpnode_s *p; // child; at the bottom level, $p points to a string with the first 2 bytes giving the number of runs (#runs)
	uint64_t l:54, n:9, is_bottom:1; // $n and $is_bottom are only set for the first node in a bucket
	int64_t c[6]; // marginal counts
} rpnode_t;

typedef struct {
	int max_nodes, block_len; // both MUST BE even numbers
	int64_t c[6]; // marginal counts
	rpnode_t *root;
	void *node, *leaf; // memory pool
} rope_t;

typedef struct {
	const rope_t *rope; // the rope
	const rpnode_t *pa[ROPE_MAX_DEPTH]; // parent nodes
	int ia[ROPE_MAX_DEPTH]; // index in the parent nodes
	int d; // the current depth in the B+-tree
} rpitr_t;

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
	void rope_insert_multi(rope_t *rope, int64_t len, const uint8_t *s);

	void rope_itr_first(const rope_t *rope, rpitr_t *i);
	const uint8_t *rope_itr_next_block(rpitr_t *i, int *n);
	
#ifdef __cplusplus
}
#endif

#endif
