#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <stopwatch.h>

#include "bmap.h"

int
main(int argc, char **argv)
{
	struct stopwatch sw;
	const int nbmaps = 65536;
	struct bmap *bmaps[nbmaps];
	int nrep = 40;
	int rep;
	int i;

	stopwatch_reset(&sw);
	stopwatch_start(&sw);
	for (i = 0; i < nbmaps; i++) {
		bmaps[i] = bmap_alloc_rnd();
	}
	stopwatch_stop(&sw);
	printf("alloc: %f\n", stopwatch_to_ns(&sw) / 1000000000.0);

	stopwatch_reset(&sw);
	stopwatch_start(&sw);
	for (rep = 0; rep < nrep; rep++)
		for (i = 0; i < nbmaps; i+= 2)
			bmap_inter64_count(bmaps[i], bmaps[i + 1]);
	stopwatch_stop(&sw);
	printf("inter64_count: %f\n", stopwatch_to_ns(&sw) / 1000000000.0);

	stopwatch_reset(&sw);
	stopwatch_start(&sw);
	for (i = 0; i < nbmaps; i+= 2)
		bmap_inter64_postcount(bmaps[i], bmaps[i + 1]);
	stopwatch_stop(&sw);
	printf("inter64_postcount: %f\n", stopwatch_to_ns(&sw) / 1000000000.0);

	return 0;
}
