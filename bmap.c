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
	b->bits = malloc(NBITS / CHAR_BIT);
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
