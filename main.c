#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "rld0.h"
#include "rle.h"
#include "mrope.h"
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
#define FLAG_COMP 0x20
#define FLAG_THR 0x40
#define FLAG_SRT 0x80
#define FLAG_LINE 0x100
#define FLAG_RLD 0x200

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

static void liftrlimit() // increase the soft limit to hard limit
{
#ifdef __linux__
	struct rlimit r;
	getrlimit(RLIMIT_AS, &r);
	if (r.rlim_cur < r.rlim_max) r.rlim_cur = r.rlim_max;
	setrlimit(RLIMIT_AS, &r);
#endif
}

double cputime()
{
	struct rusage r;
	getrusage(RUSAGE_SELF, &r);
	return r.ru_utime.tv_sec + r.ru_stime.tv_sec + 1e-6 * (r.ru_utime.tv_usec + r.ru_stime.tv_usec);
}

double realtime()
{
	struct timeval tp;
	struct timezone tzp;
	gettimeofday(&tp, &tzp);
	return tp.tv_sec + tp.tv_usec * 1e-6;
}

int main(int argc, char *argv[])
{
	mrope_t *mr = 0;
	gzFile fp;
	kseq_t *ks;
	int64_t m = 0;
	int c, i, block_len = 512, max_nodes = 64, from_stdin = 0, verbose = 3;
	int flag = FLAG_FOR | FLAG_REV | FLAG_ODD;
	kstring_t buf = { 0, 0, 0 };
	double ct, rt;

	while ((c = getopt(argc, argv, "LTFROtrbdsl:n:m:v:o:i:")) >= 0)
		if (c == 'F') flag &= ~FLAG_FOR;
		else if (c == 'o') freopen(optarg, "w", stdout);
		else if (c == 'R') flag &= ~FLAG_REV;
		else if (c == 'O') flag &= ~FLAG_ODD;
		else if (c == 'T') flag |= FLAG_TREE;
		else if (c == 'b') flag |= FLAG_BIN;
		else if (c == 't') flag |= FLAG_THR;
		else if (c == 's') flag |= FLAG_SRT;
		else if (c == 'r') flag |= FLAG_SRT | FLAG_COMP;
		else if (c == 'L') flag |= FLAG_LINE;
		else if (c == 'd') flag |= FLAG_RLD;
		else if (c == 'l') block_len = atoi(optarg);
		else if (c == 'n') max_nodes= atoi(optarg);
		else if (c == 'v') verbose = atoi(optarg);
		else if (c == 'i') {
			FILE *fp;
			if ((fp = fopen(optarg, "rb")) == 0) {
				fprintf(stderr, "[E::%s] fail to open file '%s'\n", __func__, optarg);
				return 1;
			}
			mr = mr_restore(fp);
			fclose(fp);
		} else if (c == 'm') {
			double x;
			char *p;
			x = strtod(optarg, &p);
			if (*p == 'K' || *p == 'k') x *= 1024;
			else if (*p == 'M' || *p == 'm') x *= 1024 * 1024;
			else if (*p == 'G' || *p == 'g') x *= 1024 * 1024 * 1024;
			m = (int64_t)(x * .97) + 1;
		}

	from_stdin = !isatty(fileno(stdin));
	if (optind == argc && !from_stdin) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage:   ropebwt2 [options] <in.fq.gz>\n\n");
		fprintf(stderr, "Options: -l INT     leaf block length [%d]\n", block_len);
		fprintf(stderr, "         -n INT     max number children per internal node (bpr only) [%d]\n", max_nodes);
		fprintf(stderr, "         -s         build BWT in the reverse lexicographical order (RLO)\n");
		fprintf(stderr, "         -r         build BWT in RCLO, overriding -s \n");
		fprintf(stderr, "         -m INT     batch size for multi-string indexing; 0 for single-string [0]\n");
		fprintf(stderr, "         -t         use 5 threads, only effective with -m\n\n");
		fprintf(stderr, "         -i FILE    read existing index in the FMR format from FILE [null]\n");
		fprintf(stderr, "         -L         input in the one-sequence-per-line format\n");
		fprintf(stderr, "         -F         skip forward strand\n");
		fprintf(stderr, "         -R         skip reverse strand\n");
		fprintf(stderr, "         -O         suppress end trimming when forward==reverse\n\n");
		fprintf(stderr, "         -o FILE    write output to FILE [stdout]\n");
		fprintf(stderr, "         -b         dump the index in the binary FMR format\n");
		fprintf(stderr, "         -d         dump the index in the binary fermi's FMD format\n");
		fprintf(stderr, "         -T         output the index in the Newick format (for debugging)\n\n");
		return 1;
	}

	liftrlimit();
	if (mr == 0) mr = mr_init(max_nodes, block_len);
	fp = !from_stdin && strcmp(argv[optind], "-")? gzopen(argv[optind], "rb") : gzdopen(fileno(stdin), "rb");
	ks = kseq_init(fp);
	ct = cputime(); rt = realtime();
	for (;;) {
		int l;
		uint8_t *s;
		if (flag & FLAG_LINE) { // read a line
			int dret;
			if (ks_getuntil(ks->f, KS_SEP_LINE, &ks->seq, &dret) < 0) break;
		} else if (kseq_read(ks) < 0) break; // read fasta/fastq
		l = ks->seq.l;
		s = (uint8_t*)ks->seq.s;
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
			ks->seq.s[l] = 0;
			ks->seq.l = l;
		}
		if (flag & FLAG_FOR) {
			if (!m) {
				if (flag & FLAG_SRT) mr_insert_string_rlo(mr, s, flag&FLAG_COMP);
				else mr_insert_string_io(mr, s);
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
				if (flag & FLAG_SRT) mr_insert_string_rlo(mr, s, flag&FLAG_COMP);
				else mr_insert_string_io(mr, s);
			} else {
				kputsn((char*)ks->seq.s, ks->seq.l, &buf);
				kputc(0, &buf);
			}
		}
		if (m && buf.l >= m) {
			double ct = cputime(), rt = realtime();
			mr_insert_multi(mr, buf.l, (uint8_t*)buf.s, flag&FLAG_SRT, flag&FLAG_COMP, flag&FLAG_THR);
			if (verbose >= 3) fprintf(stderr, "[M::%s] inserted %ld symbols in %.3f real sec and %.3f CPU sec\n",
					__func__, (long)buf.l, realtime() - rt, cputime() - ct);
			buf.l = 0;
		}
	}
	if (m && buf.l) {
		double ct = cputime(), rt = realtime();
		mr_insert_multi(mr, buf.l, (uint8_t*)buf.s, flag&FLAG_SRT, flag&FLAG_COMP, flag&FLAG_THR);
		if (verbose >= 3) fprintf(stderr, "[M::%s] inserted %ld symbols in %.3f real sec and %.3f CPU sec\n",
				__func__, (long)buf.l, realtime() - rt, cputime() - ct);
	}
	if (verbose >= 3) fprintf(stderr, "[M::%s] constructed FM-index in %.3f real sec and %.3f CPU sec\n",
			__func__, realtime() - rt, cputime() - ct);
	kseq_destroy(ks);
	gzclose(fp);

	if (flag & FLAG_BIN) {
		mr_dump(mr, stdout);
	} else if (flag & FLAG_TREE) {
		mr_print_tree(mr);
	} else {
		mritr_t itr;
		int len;
		const uint8_t *block;
		rld_t *e = 0;
		rlditr_t di;
		if (flag & FLAG_RLD) {
			e = rld_init(6, 3);
			rld_itr_init(e, &di, 0);
		}
		mr_itr_first(mr, &itr, 1);
		while ((block = mr_itr_next_block(&itr, &len)) != 0) {
			const uint8_t *q = block + 2, *end = block + 2 + *rle_nptr(block);
			if (flag & FLAG_RLD) {
				while (q < end) {
					int c = 0;
					int64_t l;
					rle_dec1(q, c, l);
					rld_enc(e, &di, l, c);
				}
			} else {
				while (q < end) {
					int c = 0;
					int64_t j, l;
					rle_dec1(q, c, l);
					for (j = 0; j < l; ++j) putchar("$ACGTN"[c]);
				}
			}
		}
		if (flag & FLAG_RLD) {
			rld_enc_finish(e, &di);
			rld_dump(e, "-");
		} else if (!(flag & FLAG_BIN)) putchar('\n');
	}
	mr_destroy(mr);
	return 0;
}
