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
		d[i] = random();
	}

	return b;
}

static inline int
bmap_count_internal(struct bmap * __restrict b)
{
	uint64_t *d = b->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		nbits += __builtin_popcountll(d[i]);
	return nbits;
}

int
bmap_count_r(struct bmap * __restrict b)
{
	return bmap_count_internal(b);
}

int
bmap_count(struct bmap *b)
{
	uint64_t *d = b->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		nbits += __builtin_popcountll(d[i]);
	return nbits;
}

int
bmap_inter64_count(struct bmap *r, struct bmap *s)
{
	uint64_t *d = r->bits;
	uint64_t *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		nbits += __builtin_popcountll(d[i] &= d2[i]);
	return nbits;
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
bmap_inter64_count_r(struct bmap * __restrict r, struct bmap * __restrict s)
{
	uint64_t * __restrict d = r->bits;
	uint64_t * __restrict d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		nbits += __builtin_popcountll(d[i] &= d2[i]);
	return nbits;
}

int
bmap_inter64_postcount_r(struct bmap * __restrict r, struct bmap * __restrict s)
{
	uint64_t * __restrict d = r->bits;
	uint64_t * __restrict d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++)
		d[i] &= d2[i];
	return bmap_count_internal(r);
}

#ifdef __AVX__
#include <immintrin.h>

/*
 * The lack of this instruction is hilarious.
 *
 * Why are there separate instructions that do the exact same things for single and double precision, but not
 * for ints where bit operations actually make sense. It's all casts anyway. Or is it? Two versions to test if
 * double vs. single precision makes sense.
 */
#define mm256_and_si256(v1, v2) _mm256_castpd_si256(_mm256_and_pd(_mm256_castsi256_pd(v1), _mm256_castsi256_pd(v2)))
#define mm256_and_si256_ps(v1, v2) _mm256_castps_si256(_mm256_and_ps(_mm256_castsi256_ps(v1), _mm256_castsi256_ps(v2)))

int
bmap_inter64_avx_u_count(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		_mm256_storeu_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_u_count_latestore(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		_mm256_storeu_si256(&d[i], v);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_u_count_laterstore(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
		_mm256_storeu_si256(&d[i], v);
	}
	return nbits;
}

static inline int
bmap_inter_avx_one(__m256i * restrict d1, const __m256i * restrict d2) {
	int nbits;
	__m256i v = mm256_and_si256(_mm256_loadu_si256(d1), _mm256_loadu_si256(d2));
	__m128i c1 = _mm256_extractf128_si256(v, 0);
	__m128i c2 = _mm256_extractf128_si256(v, 1);
	nbits = __builtin_popcountll(_mm_extract_epi64(c1, 0)) +
	    __builtin_popcountll(_mm_extract_epi64(c2, 0)) +
	    __builtin_popcountll(_mm_extract_epi64(c1, 1)) +
	    __builtin_popcountll(_mm_extract_epi64(c2, 1));
	_mm256_storeu_si256(d1, v);
	return nbits;
}

int
bmap_inter64_avx_u_count_laterstore_unroll2(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i += 2) {
		nbits += bmap_inter_avx_one(&d[i + 0], &d2[i + 0]);
		nbits += bmap_inter_avx_one(&d[i + 1], &d2[i + 1]);
	}
	return nbits;
}

int
bmap_inter64_avx_u_count_laterstore_unroll4(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i += 4) {
		nbits += bmap_inter_avx_one(&d[i + 0], &d2[i + 0]);
		nbits += bmap_inter_avx_one(&d[i + 1], &d2[i + 1]);
		nbits += bmap_inter_avx_one(&d[i + 2], &d2[i + 2]);
		nbits += bmap_inter_avx_one(&d[i + 3], &d2[i + 3]);
	}
	return nbits;
}

int
bmap_inter64_avx_u_count_laterstore_unroll8(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i += 8) {
		__m256i v1 = mm256_and_si256(_mm256_loadu_si256(&d[i + 0]), _mm256_loadu_si256(&d2[i + 0]));
		__m256i v2 = mm256_and_si256(_mm256_loadu_si256(&d[i + 1]), _mm256_loadu_si256(&d2[i + 1]));
		__m256i v3 = mm256_and_si256(_mm256_loadu_si256(&d[i + 2]), _mm256_loadu_si256(&d2[i + 2]));
		__m256i v4 = mm256_and_si256(_mm256_loadu_si256(&d[i + 3]), _mm256_loadu_si256(&d2[i + 3]));
		__m256i v5 = mm256_and_si256(_mm256_loadu_si256(&d[i + 0]), _mm256_loadu_si256(&d2[i + 4]));
		__m256i v6 = mm256_and_si256(_mm256_loadu_si256(&d[i + 1]), _mm256_loadu_si256(&d2[i + 5]));
		__m256i v7 = mm256_and_si256(_mm256_loadu_si256(&d[i + 2]), _mm256_loadu_si256(&d2[i + 6]));
		__m256i v8 = mm256_and_si256(_mm256_loadu_si256(&d[i + 3]), _mm256_loadu_si256(&d2[i + 7]));
		__m128i c11 = _mm256_extractf128_si256(v1, 0);
		__m128i c12 = _mm256_extractf128_si256(v1, 1);
		__m128i c21 = _mm256_extractf128_si256(v2, 0);
		__m128i c22 = _mm256_extractf128_si256(v2, 1);
		__m128i c31 = _mm256_extractf128_si256(v3, 0);
		__m128i c32 = _mm256_extractf128_si256(v3, 1);
		__m128i c41 = _mm256_extractf128_si256(v4, 0);
		__m128i c42 = _mm256_extractf128_si256(v4, 1);
		__m128i c51 = _mm256_extractf128_si256(v5, 0);
		__m128i c52 = _mm256_extractf128_si256(v5, 1);
		__m128i c61 = _mm256_extractf128_si256(v6, 0);
		__m128i c62 = _mm256_extractf128_si256(v6, 1);
		__m128i c71 = _mm256_extractf128_si256(v7, 0);
		__m128i c72 = _mm256_extractf128_si256(v7, 1);
		__m128i c81 = _mm256_extractf128_si256(v8, 0);
		__m128i c82 = _mm256_extractf128_si256(v8, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c11, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c12, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c11, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c12, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c21, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c22, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c21, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c22, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c31, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c32, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c31, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c32, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c41, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c42, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c41, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c42, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c51, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c52, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c51, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c52, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c61, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c62, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c61, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c62, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c71, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c72, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c71, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c72, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c81, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c82, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c81, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c82, 1));
		_mm256_storeu_si256(&d[i + 0], v1);
		_mm256_storeu_si256(&d[i + 1], v2);
		_mm256_storeu_si256(&d[i + 2], v3);
		_mm256_storeu_si256(&d[i + 3], v4);
		_mm256_storeu_si256(&d[i + 4], v5);
		_mm256_storeu_si256(&d[i + 5], v6);
		_mm256_storeu_si256(&d[i + 6], v7);
		_mm256_storeu_si256(&d[i + 7], v8);
	}
	return nbits;
}

int
bmap_inter64_avx_u_postcount(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		_mm256_storeu_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_count(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_a_postcount(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_count_r(struct bmap * __restrict r, struct bmap * __restrict s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_a_postcount_r(struct bmap * __restrict r, struct bmap * __restrict s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_postavxcount_r(struct bmap * __restrict r, struct bmap * __restrict s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = _mm256_load_si256(&d[i]);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_u_count_ps(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256_ps(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		_mm256_storeu_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_u_postcount_ps(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256_ps(_mm256_loadu_si256(&d[i]), _mm256_loadu_si256(&d2[i]));
		_mm256_storeu_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_count_ps(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256_ps(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_a_postcount_ps(struct bmap *r, struct bmap *s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256_ps(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_count_r_ps(struct bmap * __restrict r, struct bmap * __restrict s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256_ps(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

int
bmap_inter64_avx_a_postcount_r_ps(struct bmap * __restrict r, struct bmap * __restrict s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	return bmap_count(r);
}

int
bmap_inter64_avx_a_postavxcount_r_ps(struct bmap * __restrict r, struct bmap * __restrict s)
{
	__m256i *d = r->bits;
	__m256i *d2 = s->bits;
	int nbits = 0;
	int i;

	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = mm256_and_si256(_mm256_load_si256(&d[i]), _mm256_load_si256(&d2[i]));
		_mm256_store_si256(&d[i], v);
	}
	for (i = 0; i < NBITS / (CHAR_BIT * sizeof(*d)); i++) {
		__m256i v = _mm256_load_si256(&d[i]);
		__m128i c1 = _mm256_extractf128_si256(v, 0);
		__m128i c2 = _mm256_extractf128_si256(v, 1);
		nbits +=
			__builtin_popcountll(_mm_extract_epi64(c1, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 0)) +
			__builtin_popcountll(_mm_extract_epi64(c1, 1)) +
			__builtin_popcountll(_mm_extract_epi64(c2, 1));
	}
	return nbits;
}

#endif
