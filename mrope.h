#ifndef MROPE_H_
#define MROPE_H_

#include "rope.h"

#define MR_SO_IO    0
#define MR_SO_RLO   1
#define MR_SO_RCLO  2

typedef struct {
	uint8_t so; // sorting order
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
	 * @param max_nodes     maximum number of nodes in an internal node; use ROPE_DEF_MAX_NODES (64) if unsure
	 * @param block_len     maximum block length in an external node; use ROPE_DEF_BLOCK_LEN (256) if unsure
	 * @param sorting_order the order in which sequences are added; possible values defined by the MR_SO_* macros
	 */
	mrope_t *mr_init(int max_nodes, int block_len, int sorting_order);

	void mr_destroy(mrope_t *r);

	/**
	 * Insert one string into the index
	 *
	 * @param r      multi-rope
	 * @param str    the *reverse* of the input string (important: it is reversed!)
	 */
	int64_t mr_insert1(mrope_t *r, const uint8_t *str);

	// mr_insert1() is calling the following two functions
	int64_t mr_insert_string_io(mrope_t *r, const uint8_t *str);
	int64_t mr_insert_string_rlo(mrope_t *r, const uint8_t *str, int is_comp);

	/**
	 * Insert multiple strings
	 *
	 * @param mr       multi-rope
	 * @param len      total length of $s
	 * @param s        concatenated, NULL delimited, reversed input strings
	 * @param is_thr   true to use 5 threads
	 */
	void mr_insert_multi(mrope_t *mr, int64_t len, const uint8_t *s, int is_thr);

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
