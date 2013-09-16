#ifndef MROPE_H_
#define MROPE_H_

#include "rope.h"

typedef struct {
	rope_t *r[6];
} mrope_t;

typedef struct {
	mrope_t *r;
	int a, to_free;
	rpitr_t i;
} mritr_t;

#ifdef __cplusplus
extern "C" {
#endif

	mrope_t *mr_init(int max_nodes, int block_len);
	void mr_destroy(mrope_t *r);

	void mr_insert_string_io(mrope_t *r, const uint8_t *str);
	void mr_insert_string_rlo(mrope_t *r, const uint8_t *str, int is_comp);
	void mr_insert_multi(mrope_t *mr, int64_t len, const uint8_t *s, int is_srt, int is_comp, int is_thr);

	void mr_itr_first(mrope_t *r, mritr_t *i, int to_free);
	const uint8_t *mr_itr_next_block(mritr_t *i, int *n);

#ifdef __cplusplus
}
#endif

#endif
