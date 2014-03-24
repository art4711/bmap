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

struct bmap {
	void *bits;
};
#define NBITS 65536
struct bmap *bmap_alloc(void);
struct bmap *bmap_alloc_rnd(void);
int bmap_count(struct bmap *b);
int bmap_inter64_count(struct bmap *r, struct bmap *s);
int bmap_inter64_postcount(struct bmap *r, struct bmap *s);
int bmap_inter64_count_r(struct bmap * restrict r, struct bmap * restrict s);
int bmap_inter64_postcount_r(struct bmap * restrict r, struct bmap * restrict s);
#ifdef __AVX__
int bmap_inter64_avx_u_count(struct bmap *r, struct bmap *s);
int bmap_inter64_avx_u_postcount(struct bmap *r, struct bmap *s);
int bmap_inter64_avx_a_count(struct bmap *r, struct bmap *s);
int bmap_inter64_avx_a_postcount(struct bmap *r, struct bmap *s);
int bmap_inter64_avx_a_count_r(struct bmap * restrict r, struct bmap * restrict s);
int bmap_inter64_avx_a_postcount_r(struct bmap * restrict r, struct bmap * restrict s);
#endif
