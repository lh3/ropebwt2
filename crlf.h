#ifndef CRLF_H
#define CRLF_H

#include <stdio.h>
#include <stdint.h>

#define CRLF_BUF_LEN 0x10000

struct crlf_s;
typedef int (*crlf_write_f)(struct crlf_s*, int c, uint64_t);

// Structure for a user tag
typedef struct {
	char tag[2];   // two-symbol tag
	uint64_t len;  // length of the data
	uint8_t *data; // actual user data
} crlf_tag_t;

// The CRLF file handler
typedef struct crlf_s {
	uint8_t is_writing;   // if the file is open for writing
	uint8_t n_symbols;    // number of symbols, including the sentinel
	uint32_t dectab[256]; // decoding table
	crlf_write_f encode;  // encoding function; only used on writing
	FILE *fp;             // file pointer

	uint32_t n_tags;      // number of tags
	crlf_tag_t *tags;     // only filled on reading!

	// The following are related to bufferring
	int c;                     // symbol of the bufferred run
	uint64_t l;                // length of the bufferred run
	int i;                     // current position in the run
	int buf_len;               // length of the filled buffer (<=CRLF_BUF_LEN)
	uint8_t buf[CRLF_BUF_LEN]; // buffer
} crlf_t;

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * Create a CRLF handler for writing
	 *
	 * @param fn         file name; NULL or "-" for stdout
	 * @param n_symbols  number of symbols
	 * @param dectab     the decoding table
	 * @param encode     the encoding function
	 * @param n_tags     number of tags
	 * @param tags       tag data of size n_tags
	 *
	 * @return CRLF file handler
	 */
	crlf_t *crlf_create(const char *fn, int n_symbols, const uint32_t dectab[256], crlf_write_f encode, uint32_t n_tags, const crlf_tag_t *tags);

	/**
	 * Open a CRLF file for reading
	 *
	 * @param fn    file name; NULL or "-" for stdin
	 *
	 * @return CRLF file handler
	 */
	crlf_t *crlf_open(const char *fn);

	/**
	 * Close a CRLF file
	 *
	 * @param crlf  file handler
	 */
	int crlf_close(crlf_t *crlf);

	int crlf_dectab_RL53(uint32_t dectab[256]);
	int crlf_write_RL53(crlf_t *crlf, int c, uint64_t l);

#ifdef __cplusplus
}
#endif

/**
 * Write a byte (bufferred)
 *
 * @param crlf    file handler
 * @param byte    the byte to write
 */
static inline void crlf_write_byte(crlf_t *crlf, uint8_t byte)
{
	if (crlf->i == CRLF_BUF_LEN) {
		fwrite(crlf->buf, 1, crlf->i, crlf->fp);
		crlf->i = 0;
	}
	crlf->buf[crlf->i++] = byte;
}

/**
 * Write a run (possibly merging adjacent runs)
 *
 * @param crlf    file handler
 * @param c       symbol
 * @param l       length
 *
 * @return 0 for success; -1 for errors
 */
static inline int crlf_write(crlf_t *crlf, int c, uint64_t l)
{
	int ret = 0;
	if (c >= crlf->n_symbols || l == 0) return -1;
	if (crlf->l > 0) { // a staging run
		if (c != crlf->c) { // a new run
			ret = crlf->encode(crlf, crlf->c, crlf->l);
			crlf->c = c, crlf->l = l;
		} else crlf->l += l; // extend the staging run
	} else crlf->c = c, crlf->l = l;
	return ret;
}

/**
 * Read one byte (bufferred)
 *
 * @param crlf    file handler
 * @param l       length of the run
 *
 * @return the symbol if not negative; <0 for errors
 */
static inline int crlf_read_byte(crlf_t *crlf, uint32_t *l)
{
	uint32_t x;
	if (crlf->buf_len == 0) return -1;
	if (crlf->i == crlf->buf_len) { // then fill the buffer
		crlf->buf_len = fread(crlf->buf, 1, CRLF_BUF_LEN, crlf->fp);
		crlf->i = 0;
		if (crlf->buf_len == 0) return -1;
	}
	x = crlf->dectab[crlf->buf[crlf->i++]];
	*l = x>>8;
	return x&7;
}

/**
 * Read one entire run (continuing reading until symbol changes)
 *
 * @param crlf    file handler
 * @param l       length of the run
 *
 * @return the symbol if not negative; <0 for EOF or errors
 */
static inline int crlf_read(crlf_t *crlf, uint64_t *l)
{
	int c, ret_c;
	uint32_t l1;
	if (crlf->buf_len == 0) return -1;
	while ((c = crlf_read_byte(crlf, &l1)) == crlf->c)
		crlf->l += l1;
	*l = crlf->l, ret_c = crlf->c;
	crlf->l = l1, crlf->c = c;
	return ret_c;
}

#endif
