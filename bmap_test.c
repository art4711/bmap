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

#include <stopwatch.h>

#include "bmap.h"

struct {
	int (*t)(struct bmap *r, struct bmap *);
	const char *n;
} tests[] = {
	{ bmap_inter64_count, "inter64_count" },
	{ bmap_inter64_postcount, "inter64_postcount" },
#ifdef __AVX__
	{ bmap_inter64_avx_u_count, "inter64_avx_u_count" },
	{ bmap_inter64_avx_u_postcount, "inter64_avx_u_postcount" },
	{ bmap_inter64_avx_a_count, "inter64_avx_a_count" },
	{ bmap_inter64_avx_a_postcount, "inter64_avx_a_postcount" },
#endif
};

int
main(int argc, char **argv)
{
	struct stopwatch sw;
	const int nbmaps = 65536;
	struct bmap *bmaps[nbmaps];
	int nrep = 40;
	int rep;
	int i,t;

	stopwatch_reset(&sw);
	stopwatch_start(&sw);
	for (i = 0; i < nbmaps; i++) {
		bmaps[i] = bmap_alloc_rnd();
	}
	stopwatch_stop(&sw);
	printf("alloc: %f\n", stopwatch_to_ns(&sw) / 1000000000.0);

	for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
		stopwatch_reset(&sw);
		stopwatch_start(&sw);
		for (rep = 0; rep < nrep; rep++)
			for (i = 0; i < nbmaps; i+= 2)
				(*tests[t].t)(bmaps[i], bmaps[i + 1]);
		stopwatch_stop(&sw);
		printf("%s: %f\n", tests[t].n, stopwatch_to_ns(&sw) / 1000000000.0);
	}

	return 0;
}
