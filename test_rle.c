#include <string.h>
#include "rle6.h"

int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6], const int64_t end_cnt[6]);

int main(void)
{
	int i, block_len = 512;
	uint8_t block[512];
	int64_t cnt[6], end_cnt[6];
	memset(block, 0, block_len);
	rle_insert(block_len, block, 0, 3, 1, cnt, end_cnt); ++end_cnt[3];
	rle_insert(block_len, block, 1, 1, 1, cnt, end_cnt); ++end_cnt[1];
	rle_insert(block_len, block, 1, 0, 1, cnt, end_cnt); ++end_cnt[0];
	rle_print(block_len, block);
	rle_insert(block_len, block, 0, 2, 1, cnt, end_cnt); ++end_cnt[2];
	rle_print(block_len, block);
	return 0;
}
