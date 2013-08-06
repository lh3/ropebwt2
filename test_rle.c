#include <string.h>
#include "rle6.h"

int main(void)
{
	int i, block_len = 512;
	uint8_t block[512];
	int64_t cnt[6], end_cnt[6];
	memset(block, 0, block_len);
	memset(end_cnt, 0, 48);
	for (i = 0; i < 3998; ++i) {
		rle_insert1(block_len, block, 0, 1, cnt, end_cnt);
		++end_cnt[1];
	}
	rle_insert1(block_len, block, 1998, 1, cnt, end_cnt);
	++end_cnt[1];
	rle_print(block_len, block);

	rle_insert1(block_len, block, 15, 2, cnt, end_cnt);
	++end_cnt[2];
	rle_print(block_len, block);

	rle_insert1(block_len, block, 3000, 3, cnt, end_cnt);
	++end_cnt[2];
	rle_print(block_len, block);

	return 0;
}
