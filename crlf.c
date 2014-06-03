#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "crlf.h"

crlf_t *crlf_create(const char *fn, int n_symbols, const uint32_t dectab[256], crlf_write_f encode, uint32_t n_tags, const crlf_tag_t *tags)
{
	crlf_t *crlf;
	FILE *fp;
	int to_stdout;
	uint32_t i;

	to_stdout = (fn == 0 || strcmp(fn, "-") == 0);
	fp = to_stdout? stdout : fopen(fn, "wb");
	if (fp == 0) return 0;

	crlf = (crlf_t*)calloc(1, sizeof(crlf_t));
	crlf->n_symbols = n_symbols;
	crlf->n_tags = n_tags;
	crlf->encode = encode;
	crlf->is_writing = 1;
	crlf->fp = fp;
	memcpy(crlf->dectab, dectab, 256 * 4);

	fwrite("CRL\1", 1, 4, fp);
	fwrite(&n_symbols, 1, 1, fp);
	fwrite(crlf->dectab, 4, 256, fp);
	fwrite(&n_tags, 4, 1, fp);
	for (i = 0; i < n_tags; ++i) {
		fwrite(tags[i].tag, 1, 2, fp);
		fwrite(&tags[i].len, 8, 1, fp);
		fwrite(tags[i].data, 1, tags[i].len, fp);
	}
	return crlf;
}

crlf_t *crlf_open(const char *fn)
{
	FILE *fp;
	char magic[4];
	crlf_t *crlf;
	uint32_t i, l = 0;

	fp = (fn && strcmp(fn, "-"))? fopen(fn, "rb") : stdin;
	if (fp == 0) return 0;
	fread(magic, 1, 4, fp);
	if (strncmp(magic, "CRL\1", 4) != 0) {
		fclose(fp);
		return 0;
	}

	crlf = (crlf_t*)calloc(1, sizeof(crlf_t));
	crlf->fp = fp;
	fread(&crlf->n_symbols, 1, 1, fp);
	fread(crlf->dectab, 4, 256, fp);
	fread(&crlf->n_tags, 4, 1, fp);
	crlf->tags = (crlf_tag_t*)calloc(crlf->n_tags, sizeof(crlf_tag_t));
	for (i = 0; i < crlf->n_tags; ++i) {
		fread(crlf->tags[i].tag, 1, 2, fp);
		fread(&crlf->tags[i].len, 8, 1, fp);
		crlf->tags[i].data = (uint8_t*)malloc(crlf->tags[i].len);
		fread(crlf->tags[i].data, 1, crlf->tags[i].len, fp);
	}
	crlf->buf_len = fread(crlf->buf, 1, CRLF_BUF_LEN, fp);
	crlf->c = crlf_read_byte(crlf, &l);
	crlf->l = l;
	return crlf;
}

int crlf_close(crlf_t *crlf)
{
	if (crlf == 0) return -1;
	if (crlf->is_writing) {
		crlf->encode(crlf, crlf->c, crlf->l);
		fwrite(crlf->buf, 1, crlf->i, crlf->fp);
	}
	if (crlf->tags) {
		uint32_t i;
		for (i = 0; i < crlf->n_tags; ++i)
			free(crlf->tags[i].data);
		free(crlf->tags);
	}
	fclose(crlf->fp);
	free(crlf);
	return 0;
}

/**************
 *** Codecs ***
 **************/

int crlf_dectab_RL53(uint32_t dectab[256])
{
	uint32_t x;
	for (x = 0; x < 256; ++x)
		dectab[x] = x>>3<<8 | (x&7);
	return 8;
}

int crlf_write_RL53(crlf_t *crlf, int c, uint64_t l)
{
	while (l > 31) {
		crlf_write_byte(crlf, 31<<3 | c);
		l -= 31;
	}
	crlf_write_byte(crlf, l<<3 | c);
	return 0;
}
