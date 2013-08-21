#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rle.h"
#include "rope.h"
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

static inline int kputsn(const char *p, int l, kstring_t *s)
{
	if (s->l + l + 1 >= s->m) {
		char *tmp;
		s->m = s->l + l + 2;
		kroundup32(s->m);
		if ((tmp = (char*)realloc(s->s, s->m))) s->s = tmp;
		else return EOF;
	}
	memcpy(s->s + s->l, p, l);
	s->l += l;
	s->s[s->l] = 0;
	return l;
}

static inline int kputc(int c, kstring_t *s)
{
	if (s->l + 1 >= s->m) {
		char *tmp;
		s->m = s->l + 2;
		kroundup32(s->m);
		if ((tmp = (char*)realloc(s->s, s->m))) s->s = tmp;
		else return EOF;
	}
	s->s[s->l++] = c;
	s->s[s->l] = 0;
	return c;
}

int main(int argc, char *argv[])
{
	rope_t *r6;
	gzFile fp;
	FILE *out = stdout;
	kseq_t *ks;
	int c, i, block_len = 512, max_nodes = 64, m = 0;
	int flag = FLAG_FOR | FLAG_REV | FLAG_ODD;
	kstring_t buf = { 0, 0, 0 };

	while ((c = getopt(argc, argv, "TFRObso:l:n:m:")) >= 0)
		if (c == 'o') out = fopen(optarg, "wb");
		else if (c == 'F') flag &= ~FLAG_FOR;
		else if (c == 'R') flag &= ~FLAG_REV;
		else if (c == 'O') flag &= ~FLAG_ODD;
		else if (c == 'T') flag |= FLAG_TREE;
		else if (c == 'b') flag |= FLAG_BIN;
		else if (c == 's') flag |= FLAG_RLO;
		else if (c == 'l') block_len = atoi(optarg);
		else if (c == 'n') max_nodes= atoi(optarg);
		else if (c == 'm') {
			long x;
			char *p;
			x = strtol(optarg, &p, 10);
			if (*p == 'K' || *p == 'k') x *= 1024;
			else if (*p == 'M' || *p == 'm') x *= 1024 * 1024;
			else if (*p == 'G' || *p == 'g') x *= 1024 * 1024 * 1024;
			m = (int)(x * .97);
		}

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
		fprintf(stderr, "         -O         suppress end trimming when forward==reverse\n\n");
		return 1;
	}

	r6 = rope_init(max_nodes, block_len);
	fp = strcmp(argv[optind], "-")? gzopen(argv[optind], "rb") : gzdopen(fileno(stdin), "rb");
	ks = kseq_init(fp);
	while (kseq_read(ks) >= 0) {
		int l = ks->seq.l;
		uint8_t *s = (uint8_t*)ks->seq.s;
		for (i = 0; i < l; ++i) // change encoding
			s[i] = s[i] < 128? seq_nt6_table[s[i]] : 5;
		for (i = 0; i < l>>1; ++i) { // reverse
			int tmp = s[l-1-i];
			s[l-1-i] = s[i]; s[i] = tmp;
		}
		if ((flag & FLAG_ODD) && (l&1) == 0) { // then check reverse complement
			for (i = 0; i < l>>1; ++i) // is the reverse complement is identical to itself?
				if (s[i] + s[l-1-i] != 5) break;
			if (i == l>>1) --l; // if so, trim 1bp from the end
		}
		if (flag & FLAG_FOR) {
			if (!m) {
				if (flag & FLAG_RLO) rope_insert_string_rlo(r6, s);
				else rope_insert_string_io(r6, s);
			} else {
				kputsn((char*)ks->seq.s, ks->seq.l, &buf);
				kputc(0, &buf);
			}
		}
		if (flag & FLAG_REV) {
			for (i = 0; i < l>>1; ++i) {
				int tmp = s[l-1-i];
				tmp = (tmp >= 1 && tmp <= 4)? 5 - tmp : tmp;
				s[l-1-i] = (s[i] >= 1 && s[i] <= 4)? 5 - s[i] : s[i];
				s[i] = tmp;
			}
			if (l&1) s[i] = (s[i] >= 1 && s[i] <= 4)? 5 - s[i] : s[i];
			if (!m) {
				if (flag & FLAG_RLO) rope_insert_string_rlo(r6, s);
				else rope_insert_string_io(r6, s);
			} else {
				kputsn((char*)ks->seq.s, ks->seq.l, &buf);
				kputc(0, &buf);
			}
		}
		if (m && buf.l >= m) {
			rope_insert_multi(r6, buf.l, (uint8_t*)buf.s);
			buf.l = 0;
		}
	}
	if (m && buf.l) rope_insert_multi(r6, buf.l, (uint8_t*)buf.s);
	kseq_destroy(ks);
	gzclose(fp);

	{
		rpitr_t itr;
		int len;
		const uint8_t *block;
		rope_itr_first(r6, &itr);
		while ((block = rope_itr_next_block(&itr, &len)) != 0) {
			const uint8_t *q = block + 2, *end = block + 2 + *rle_nptr(block);
			while (q < end) {
				int c = 0;
				int64_t j, l;
				rle_dec1(q, c, l);
				for (j = 0; j < l; ++j) putchar("$ACGTN"[c]);
			}
		}
		putchar('\n');
	}
	rope_destroy(r6); fclose(out);
	return 0;
}
