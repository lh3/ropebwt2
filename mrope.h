#ifndef MROPE_H_
#define MROPE_H_

#include "rope.h"

typedef struct {
	rope_t *r[6];
} mrope_t; // multi-rope

typedef struct {
	mrope_t *r;
	int a, to_free;
	rpitr_t i;
} mritr_t;

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * Initiate a multi-rope
	 *
	 * @param max_nodes   maximum number of nodes in an internal node; use ROPE_DEF_MAX_NODES (64) if unsure
	 * @param block_len   maximum block length in an external node; use ROPE_DEF_BLOCK_LEN (256) if unsure
	 */
	mrope_t *mr_init(int max_nodes, int block_len);

	void mr_destroy(mrope_t *r);

	/**
	 * Insert a string in the input order
	 *
	 * @param r        multi-rope
	 * @param str      NULL terminated, reversed input string
	 */
	void mr_insert_string_io(mrope_t *r, const uint8_t *str);

	/**
	 * Insert a string in the sorted order
	 *
	 * @param r        multi-rope
	 * @param str      NULL terminated, reversed input string
	 * @param is_comp  if true, in RCLO; otherwise in RLO
	 *
	 * Note: Different input orders should not be mixed.
	 */
	void mr_insert_string_rlo(mrope_t *r, const uint8_t *str, int is_comp);

	/**
	 * Insert multiple strings
	 *
	 * @param mr       multi-rope
	 * @param len      total length of $s
	 * @param s        concatenated, NULL delimited, reversed input strings
	 * @param is_srt   true if insert in RLO or RCLO; otherwise in the input order
	 * @param is_comp  when is_srt is set, true for RCLO; false for RLO
	 * @param is_thr   true to use 5 threads
	 */
	void mr_insert_multi(mrope_t *mr, int64_t len, const uint8_t *s, int is_srt, int is_comp, int is_thr);

	/**
	 * Put the iterator at the start of the index
	 *
	 * @param r         multi-rope
	 * @param i         iterator to be initialized
	 * @param to_free   if true, free visited buckets
	 */
	void mr_itr_first(mrope_t *r, mritr_t *i, int to_free);

	/**
	 * Iterate to the next block
	 *
	 * @param i         iterator
	 *
	 * @return          pointer to the start of a block; see rle.h for decoding the block
	 */
	const uint8_t *mr_itr_next_block(mritr_t *i);

	void mr_print_tree(const mrope_t *mr);
	void mr_dump(mrope_t *mr, FILE *fp);
	mrope_t *mr_restore(FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
