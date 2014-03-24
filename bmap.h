struct bmap {
	void *bits;
};
#define NBITS 65536
struct bmap *bmap_alloc(void);
struct bmap *bmap_alloc_rnd(void);
int bmap_count(struct bmap *b);
int bmap_inter64_count(struct bmap *r, struct bmap *s);
int bmap_inter64_postcount(struct bmap *r, struct bmap *s);
