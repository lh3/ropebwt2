#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rle6.h"
#include "rope6.h"
#include "kseq.h"
KSEQ_INIT(gzFile, gzread)

static unsigned char seq_nt6_table[128] = {
    0, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 1, 5, 2,  5, 5, 5, 3,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  4, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 1, 5, 2,  5, 5, 5, 3,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  4, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5
};

#define FLAG_FOR 0x1
#define FLAG_REV 0x2
#define FLAG_ODD 0x4
#define FLAG_BIN 0x8
#define FLAG_TREE 0x10
#define FLAG_RLO 0x80

int main(int argc, char *argv[])
{
	rope6_t *r6;
	gzFile fp;
	FILE *out = stdout;
	kseq_t *ks;
	int c, i, block_len = 512, max_nodes = 64;
	int flag = FLAG_FOR | FLAG_REV | FLAG_ODD;

	while ((c = getopt(argc, argv, "TFRObso:l:n:")) >= 0)
		if (c == 'o') out = fopen(optarg, "wb");
		else if (c == 'F') flag &= ~FLAG_FOR;
		else if (c == 'R') flag &= ~FLAG_REV;
		else if (c == 'O') flag &= ~FLAG_ODD;
		else if (c == 'T') flag |= FLAG_TREE;
		else if (c == 'b') flag |= FLAG_BIN;
		else if (c == 'l') block_len = atoi(optarg);
		else if (c == 'n') max_nodes= atoi(optarg);

	if (optind == argc) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage:   ropebwt2 [options] <in.fq.gz>\n\n");
		fprintf(stderr, "Options: -l INT     leaf block length [%d]\n", block_len);
		fprintf(stderr, "         -n INT     max number children per internal node (bpr only) [%d]\n", max_nodes);
		fprintf(stderr, "         -o FILE    output file [stdout]\n");
		fprintf(stderr, "         -b         binary output (5+3 runs starting after 4 bytes)\n");
		fprintf(stderr, "         -s         build BWT in RLO\n");
		fprintf(stderr, "         -F         skip forward strand\n");
		fprintf(stderr, "         -R         skip reverse strand\n");
		fprintf(stderr, "         -O         suppress end trimming when forward==reverse\n");
		fprintf(stderr, "         -T         print the tree stdout (bpr and rbr only)\n\n");
		return 1;
	}

	r6 = r6_init(max_nodes, block_len);
	fp = strcmp(argv[optind], "-")? gzopen(argv[optind], "rb") : gzdopen(fileno(stdin), "rb");
	ks = kseq_init(fp);
	while (kseq_read(ks) >= 0) {
		int l = ks->seq.l;
		uint8_t *s = (uint8_t*)ks->seq.s;
		for (i = 0; i < l; ++i)
			s[i] = s[i] < 128? seq_nt6_table[s[i]] : 5;
		if ((flag & FLAG_ODD) && (l&1) == 0) { // then check reverse complement
			for (i = 0; i < l>>1; ++i) // is the reverse complement is identical to itself?
				if (s[i] + s[l-1-i] != 5) break;
			if (i == l>>1) --l; // if so, trim 1bp from the end
		}
		if (flag & FLAG_FOR)
			r6_insert_string_io(r6, ks->seq.l, s);
		if (flag & FLAG_REV) {
			for (i = 0; i < l>>1; ++i) {
				int tmp = s[l-1-i];
				tmp = (tmp >= 1 && tmp <= 4)? 5 - tmp : tmp;
				s[l-1-i] = (s[i] >= 1 && s[i] <= 4)? 5 - s[i] : s[i];
				s[i] = tmp;
			}
			if (l&1) s[i] = (s[i] >= 1 && s[i] <= 4)? 5 - s[i] : s[i];
			r6_insert_string_io(r6, ks->seq.l, s);
		}
	}
	kseq_destroy(ks);
	gzclose(fp);

	{
		r6itr_t *itr;
		int len;
		const uint8_t *block;
		itr = r6_itr_init(r6);
		while ((block = r6_itr_next(itr, &len)) != 0) {
			const uint8_t *q = block, *end = block + rle_runs(block, len);
			int c = 0;
			while (q < end) {
				int64_t j, l;
#if RLE_CODEC != 3
				q += rle_dec(q, &c, &l);
#else
				if (*q>>7) l = (*q&0x7f)<<4;
				else c = *q&7, l = *q>>3;
				++q;
#endif
				for (j = 0; j < l; ++j) putchar("$ACGTN"[c]);
			}
		}
		putchar('\n');
		free(itr);
	}
	r6_destroy(r6); fclose(out);
	return 0;
}
