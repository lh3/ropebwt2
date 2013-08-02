#include <string.h>
#include "rope6.h"

int main(void)
{
	int block_len = 512;
	uint8_t block[512];
	int64_t cnt[6];
	memset(block, 0, 512);
	rle_insert(block_len, block, 0, 2, 300, cnt);
	rle_print(block_len, block);
	rle_insert(block_len, block, 100, 3, 3, cnt);
	rle_print(block_len, block);
	rle_insert(block_len, block, 0, 1, 4, cnt);
	rle_print(block_len, block);
	rle_insert(block_len, block, 104, 3, 4, cnt);
	rle_print(block_len, block);
	return 0;
}
