/*
 * Copyright (c) 2014 Artur Grabowski <art@blahonga.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#include "bmap.h"

struct bmap *
bmap_alloc(void)
{
	struct bmap *b;

	b = malloc(sizeof *b);
	posix_memalign(&b->bits, 32, NBITS / CHAR_BIT);
	memset(b->bits, 0, NBITS / CHAR_BIT);

	return b;
}

struct bmap *
bmap_alloc_rnd(void)
{
	struct bmap *b = bmap_alloc();
	uint16_t *d;
	int i;

	d = b->bits;
	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		*d = random();
	}

	return b;
}

int
bmap_count_r(struct bmap *b)
{
	uint64_t *d = b->bits;
	int ndocs = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		ndocs += __builtin_popcountll(d[i]);
	return ndocs;
}

int
bmap_count(struct bmap *b)
{
	uint64_t *d = b->bits;
	int ndocs = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		ndocs += __builtin_popcountll(d[i]);
	return ndocs;
}

int
bmap_inter64_count(struct bmap *r, struct bmap *s)
{
	uint64_t *d = r->bits;
	uint64_t *d2 = s->bits;
	int ndocs = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		ndocs += __builtin_popcountll(d[i] &= d2[i]);
	return ndocs;
}

int
bmap_inter64_postcount(struct bmap *r, struct bmap *s)
{
	uint64_t *d = r->bits;
	uint64_t *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		d[i] &= d2[i];
	return bmap_count(r);
}

int
bmap_inter64_count_r(struct bmap * restrict r, struct bmap * restrict s)
{
	uint64_t *d = r->bits;
	uint64_t *d2 = s->bits;
	int ndocs = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		ndocs += __builtin_popcountll(d[i] &= d2[i]);
	return ndocs;
}

int
bmap_inter64_postcount_r(struct bmap * restrict r, struct bmap * restrict s)
{
	uint64_t * restrict d = r->bits;
	uint64_t * restrict d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		d[i] &= d2[i];
	return bmap_count(r);
}

#ifdef __AVX__
#include <immintrin.h>

/*
 * The lack of this instruction is hilarious.
 *
 * Why are there separate instructions that do the exact same things for single and double precision, but not
 * for ints where bit operations actually make sense. It's all casts anyway.
 */
#define mm256_and_si256(v1, v2) _mm256_castpd_si256(_mm256_and_pd(_mm256_castsi256_pd(v1), _mm256_castsi256_pd(v2)))

int
bmap_inter64_avx_u_count(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int ndocs = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256 v = mm256_and_si256(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		_mm256_storeu_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		ndocs +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return ndocs;
}

int
bmap_inter64_avx_u_postcount(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256 v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_count(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int ndocs = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256 v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		ndocs +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return ndocs;
}

int
bmap_inter64_avx_a_postcount(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256 v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	return bmap_count(r);
}


#endif
