#include <string.h>
#include "rle6.h"

int rle_insert2(int block_len, uint8_t *block, int64_t x, int a, int64_t rl, int64_t cnt[6], int64_t end_cnt[6])
{
	rle_insert(block_len, block, x, a, rl, cnt, end_cnt);
	end_cnt[a] += rl;
}

int main(void)
{
	int i, block_len = 512;
	uint8_t block[512];
	int64_t cnt[6], end_cnt[6];
	memset(block, 0, block_len);
	rle_insert2(block_len, block, 0, 3, 1000000, cnt, end_cnt);
	rle_insert2(block_len, block, 800000, 2, 10000, cnt, end_cnt);
	rle_print(block_len, block);
	return 0;
}
