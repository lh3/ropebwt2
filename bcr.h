#ifndef ROPE_BCR_H_
#define ROPE_BCR_H_

#include "rope.h"

#define BCR_F_THR 0x1
#define BCR_F_RLO 0x2
#define BCR_F_COMP 0x4

struct bcr_s;
typedef struct bcr_s bcr_t;

bcr_t *bcr_init(int max_nodes, int block_len);
void bcr_print(const bcr_t *b);
void bcr_insert(bcr_t *b, int64_t len, const uint8_t *s, int flag);

#endif
