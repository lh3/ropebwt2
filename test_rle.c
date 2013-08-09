#include <string.h>
#include "rle6.h"

int rle_insert(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6], const int64_t end_cnt[6]);

int main(void)
{
	int i, block_len = 512;
	uint8_t block[512];
	int64_t cnt[6], end_cnt[6];
	memset(block, 0, block_len);
	memset(end_cnt, 0, 48);

	rle_insert1(block_len, block, 0, 3, cnt, end_cnt);
	rle_insert1(block_len, block, 1, 4, cnt, end_cnt);
	rle_insert1(block_len, block, 2, 3, cnt, end_cnt);
	rle_insert1(block_len, block, 2, 4, cnt, end_cnt);
	rle_insert1(block_len, block, 4, 3, cnt, end_cnt);
	rle_insert1(block_len, block, 3, 1, cnt, end_cnt);
	rle_insert1(block_len, block, 1, 4, cnt, end_cnt);
	rle_print(block_len, block);

	return 0;

	for (i = 0; i < 1000; ++i) {
		rle_insert1(block_len, block, 0, 1, cnt, end_cnt);
		++end_cnt[1];
	}
	rle_print(block_len, block);

	rle_insert1(block_len, block, 15, 2, cnt, end_cnt);
	++end_cnt[2];
	rle_print(block_len, block);

	rle_insert1(block_len, block, 3000, 3, cnt, end_cnt);
	++end_cnt[2];
	rle_print(block_len, block);

	return 0;
}
