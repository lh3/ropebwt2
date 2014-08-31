#include <zlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "rld0.h"
#include "rle.h"
#include "mrope.h"
#include "crlf.h"
#include "kseq.h"
KSEQ_INIT(gzFile, gzread)

#define ROPEBWT2_VERSION "r187"

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
#define FLAG_THR 0x40
#define FLAG_LINE 0x100
#define FLAG_RLD 0x200
#define FLAG_NON 0x400
#define FLAG_CRLF 0x800
#define FLAG_CUTN 0x1000

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

static inline int is_rev_same(int l, const uint8_t *s)
{
	int i;
	if (l&1) return 0;
	for (i = 0; i < l>>1; ++i)
		if (s[i] + s[l-1-i] != 5) break;
	return (i == l>>1);
}

int main_ropebwt2(int argc, char *argv[])
{
	mrope_t *mr = 0;
	gzFile fp;
	kseq_t *ks;
	int64_t m = (int64_t)(.97 * 10 * 1024 * 1024 * 1024) + 1;;
	int c, i, block_len = ROPE_DEF_BLOCK_LEN, max_nodes = ROPE_DEF_MAX_NODES, from_stdin = 0, verbose = 3, so = MR_SO_IO, min_q = 0, thr_min = -1, min_cut_len = 0;
	int flag = FLAG_FOR | FLAG_REV | FLAG_THR;
	kstring_t buf = { 0, 0, 0 };
	double ct, rt;

	while ((c = getopt(argc, argv, "BPNLTFRCtrbdsl:n:m:v:o:i:q:M:x:")) >= 0) {
		if (c == 'o') freopen(optarg, "w", stdout);
		else if (c == 'F') flag &= ~FLAG_FOR;
		else if (c == 'R') flag &= ~FLAG_REV;
		else if (c == 'C') flag |= FLAG_ODD;
		else if (c == 'T') flag |= FLAG_TREE;
		else if (c == 'b') flag |= FLAG_BIN;
		else if (c == 't') flag |= FLAG_THR;
		else if (c == 'L') flag |= FLAG_LINE;
		else if (c == 'd') flag |= FLAG_RLD;
		else if (c == 'N') flag |= FLAG_NON;
		else if (c == 'B') flag |= FLAG_CRLF;
		else if (c == 'P') flag &= ~FLAG_THR;
		else if (c == 's') so = so != MR_SO_RCLO? MR_SO_RLO : MR_SO_RCLO;
		else if (c == 'r') so = MR_SO_RCLO;
		else if (c == 'l') block_len = atoi(optarg);
		else if (c == 'n') max_nodes= atoi(optarg);
		else if (c == 'v') verbose = atoi(optarg);
		else if (c == 'q') min_q = atoi(optarg);
		else if (c == 'M') thr_min = atoi(optarg);
		else if (c == 'x') min_cut_len = atoi(optarg), flag |= FLAG_CUTN;
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
			m = x? (int64_t)(x * .97) + 1 : 0;
		}
	}

	from_stdin = !isatty(fileno(stdin));
	if (optind == argc && !from_stdin) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage:   ropebwt2-%s [options] <in.fq.gz>\n\n", ROPEBWT2_VERSION);
		fprintf(stderr, "Options: -l INT     leaf block length [%d]\n", block_len);
		fprintf(stderr, "         -n INT     max number children per internal node [%d]\n", max_nodes);
		fprintf(stderr, "         -s         build BWT in the reverse lexicographical order (RLO)\n");
		fprintf(stderr, "         -r         build BWT in RCLO, overriding -s \n");
		fprintf(stderr, "         -m INT     batch size for multi-string indexing; 0 for single-string [10g]\n");
		fprintf(stderr, "         -P         always use a single thread\n");
		fprintf(stderr, "         -M INT     switch to single thread when < INT strings remain in a batch [%d]\n\n", 1000);
		fprintf(stderr, "         -i FILE    read existing index in the FMR format from FILE, overriding -s/-r [null]\n");
		fprintf(stderr, "         -L         input in the one-sequence-per-line format\n");
		fprintf(stderr, "         -F         skip forward strand\n");
		fprintf(stderr, "         -R         skip reverse strand\n");
		fprintf(stderr, "         -N         skip sequences containing ambiguous bases\n");
		fprintf(stderr, "         -x INT     cut at ambiguous bases and discard segment with length <INT [0]\n");
		fprintf(stderr, "         -C         cut one base if forward==reverse\n");
		fprintf(stderr, "         -q INT     hard mask bases with QUAL<INT [0]\n\n");
		fprintf(stderr, "         -o FILE    write output to FILE [stdout]\n");
		fprintf(stderr, "         -b         dump the index in the binary FMR format\n");
		fprintf(stderr, "         -d         dump the index in fermi's FMD format\n");
		fprintf(stderr, "         -T         output the index in the Newick format (for debugging)\n\n");
		return 1;
	}

	if ((flag & FLAG_CUTN) && m == 0) {
		fprintf(stderr, "[E::%s] option '-x' cannot be used with '-m0'\n", __func__);
		return 1;
	}

	liftrlimit();
	if (mr == 0) mr = mr_init(max_nodes, block_len, so);
	if (thr_min > 0) mr_thr_min(mr, thr_min);
	fp = optind < argc && strcmp(argv[optind], "-")? gzopen(argv[optind], "rb") : gzdopen(fileno(stdin), "rb");
	ks = kseq_init(fp);
	ct = cputime(); rt = realtime();
	for (;;) {
		int l;
		uint8_t *s;
		if (flag & FLAG_LINE) { // read a line
			int dret;
			if (ks_getuntil(ks->f, KS_SEP_LINE, &ks->seq, &dret) < 0) break;
			for (i = 0; i < ks->seq.l; ++i)
				if (!isalpha(ks->seq.s[i])) break;
			ks->seq.l = i;
			ks->qual.l = 0;
		} else if (kseq_read(ks) < 0) break; // read fasta/fastq
		l = ks->seq.l;
		s = (uint8_t*)ks->seq.s;
		for (i = 0; i < l; ++i) // change encoding
			s[i] = s[i] < 128? seq_nt6_table[s[i]] : 5;
		if (ks->qual.l && min_q > 0) // then hard mask the sequence
			for (i = 0; i < l; ++i)
				s[i] = ks->qual.s[i] - 33 >= min_q? s[i] : 5;
		if (flag & FLAG_NON) {
			for (i = 0; i < l; ++i)
				if (s[i] == 5) break;
			if (i < l) continue;
		}
		for (i = 0; i < l>>1; ++i) { // reverse
			int tmp = s[l-1-i];
			s[l-1-i] = s[i]; s[i] = tmp;
		}
		if (flag & FLAG_CUTN) {
			int b, k;
			for (k = b = i = 0; i <= l; ++i) {
				if (i == l || s[i] == 5) {
					int tmp_l = i - b;
					if (tmp_l >= min_cut_len) {
						if ((flag & FLAG_ODD) && is_rev_same(tmp_l, &s[k - tmp_l])) --k;
						s[k++] = 0;
					} else k -= tmp_l; // skip this segment
					b = i + 1;
				} else s[k++] = s[i];
			}
			if (--k == 0) continue;
			ks->seq.l = l = k;
		}
		if ((flag & FLAG_ODD) && is_rev_same(l, s)) {
			ks->seq.s[--l] = 0;
			ks->seq.l = l;
		}
		if (flag & FLAG_FOR) {
			if (m) kputsn((char*)ks->seq.s, ks->seq.l + 1, &buf);
			else mr_insert1(mr, s);
		}
		if (flag & FLAG_REV) {
			for (i = 0; i < l>>1; ++i) {
				int tmp = s[l-1-i];
				tmp = (tmp >= 1 && tmp <= 4)? 5 - tmp : tmp;
				s[l-1-i] = (s[i] >= 1 && s[i] <= 4)? 5 - s[i] : s[i];
				s[i] = tmp;
			}
			if (l&1) s[i] = (s[i] >= 1 && s[i] <= 4)? 5 - s[i] : s[i];
			if (m) kputsn((char*)ks->seq.s, ks->seq.l + 1, &buf);
			else mr_insert1(mr, s);
		}
		if (m && buf.l >= m) {
			double ct = cputime(), rt = realtime();
			mr_insert_multi(mr, buf.l, (uint8_t*)buf.s, flag&FLAG_THR);
			if (verbose >= 3) fprintf(stderr, "[M::%s] inserted %ld symbols in %.3f sec, %.3f CPU sec\n",
					__func__, (long)buf.l, realtime() - rt, cputime() - ct);
			buf.l = 0;
		}
	}
	if (m && buf.l) {
		double ct = cputime(), rt = realtime();
		mr_insert_multi(mr, buf.l, (uint8_t*)buf.s, flag&FLAG_THR);
		if (verbose >= 3) fprintf(stderr, "[M::%s] inserted %ld symbols in %.3f sec, %.3f CPU sec\n",
				__func__, (long)buf.l, realtime() - rt, cputime() - ct);
	}
	if (verbose >= 3) {
		int64_t c[6];
		fprintf(stderr, "[M::%s] constructed FM-index in %.3f sec, %.3f CPU sec\n", __func__, realtime() - rt, cputime() - ct);
		mr_get_c(mr, c);
		fprintf(stderr, "[M::%s] symbol counts: ($, A, C, G, T, N) = (%ld, %ld, %ld, %ld, %ld, %ld)\n", __func__,
				(long)c[0], (long)c[1], (long)c[2], (long)c[3], (long)c[4], (long)c[5]);
	}
	free(buf.s);
	kseq_destroy(ks);
	gzclose(fp);

	if (flag & FLAG_BIN) {
		mr_dump(mr, stdout);
	} else if (flag & FLAG_TREE) {
		mr_print_tree(mr);
	} else {
		mritr_t itr;
		const uint8_t *block;
		rld_t *e = 0;
		rlditr_t di;
		crlf_t *crlf = 0;

		if (flag & FLAG_RLD) {
			e = rld_init(6, 3);
			rld_itr_init(e, &di, 0);
		} else if (flag & FLAG_CRLF) {
			crlf_tag_t tag;
			int64_t c[6];
			uint32_t dectab[256];
			crlf_dectab_RL53(dectab);
			mr_get_c(mr, c);
			tag.tag[0] = 'M', tag.tag[1] = 'C';
			tag.len = 48;
			tag.data = malloc(48);
			memcpy(tag.data, c, 48);
			crlf = crlf_create(0, 6, dectab, crlf_write_RL53, 1, &tag);
			free(tag.data);
		}
		mr_itr_first(mr, &itr, 1);
		while ((block = mr_itr_next_block(&itr)) != 0) {
			const uint8_t *q = block + 2, *end = block + 2 + *rle_nptr(block);
			if (flag & FLAG_RLD) {
				while (q < end) {
					int c = 0;
					int64_t l;
					rle_dec1(q, c, l);
					rld_enc(e, &di, l, c);
				}
			} else if (flag & FLAG_CRLF) {
				while (q < end) {
					int c = 0;
					int64_t l;
					rle_dec1(q, c, l);
					crlf_write(crlf, c, l);
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
			fprintf(stderr, "[M::%s] rld: (tot, $, A, C, G, T, N) = (%ld, %ld, %ld, %ld, %ld, %ld, %ld)\n", __func__,
					(long)e->mcnt[0], (long)e->mcnt[1], (long)e->mcnt[2], (long)e->mcnt[3], (long)e->mcnt[4], (long)e->mcnt[5], (long)e->mcnt[6]);
			rld_dump(e, "-");
		} else if (flag & FLAG_CRLF) {
			crlf_close(crlf);
		} else if (!(flag & FLAG_BIN)) putchar('\n');
	}
	mr_destroy(mr);
	return 0;
}

int main(int argc, char *argv[])
{
	int ret, i;
	double t_start;
	t_start = realtime();
	ret = main_ropebwt2(argc, argv);
	if (ret == 0) {
		fprintf(stderr, "[M::%s] Version: %s\n", __func__, ROPEBWT2_VERSION);
		fprintf(stderr, "[M::%s] CMD:", __func__);
		for (i = 0; i < argc; ++i)
			fprintf(stderr, " %s", argv[i]);
		fprintf(stderr, "\n[M::%s] Real time: %.3f sec; CPU: %.3f sec\n", __func__, realtime() - t_start, cputime());
	}
	return ret;
}
