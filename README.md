# bmap tests

Tests to see the efficiency of various approaches to performing intersection and union operations on large bitmaps.

## The data

The data is a large amount of (currently randomly generated) bits. The output is a bitmap of the intersection of two bitmaps and a count of bits set in the result.

## The implementation variants.

### bmap_inter64_count

Treat the bitmap arrays as arrays of uint64_t values. Intersect them with `&=` and count the number of set bits on the fly with `__builtin_popcountll`.

The first thing to test here is how -mpopcnt vs. a version without the popcnt instructions perform:


    x statdir-nothing/inter64_count
    + statdir-popcount/inter64_count
        N           Min           Max        Median           Avg        Stddev
    x  20      1.341714      1.462667      1.418456     1.4126183   0.031488544
    +  20      0.602577       0.66121        0.6423     0.6358017   0.017562349
    Difference at 95.0% confidence
	-0.776817 +/- 0.0163178
	-54.9913% +/- 1.15514%
	(Student's t, pooled s = 0.0254947)

This is really good. The popcount version is over twice as fast. We should use popcount from now on.

### bmap_inter64_postcount

Like `bmap_inter64_count` but instead of counting the bits on the fly we go through the result array once after the intersection and count the bits that way.

This is the first trouble with the compiler. Since building is done with -mavx and clang was used, clang happily vectorized this function which was not the point of the test. A separate build without -mavx, but with -mpopcnt shows how this approach performs:

    x statdir-popcount/inter64_count
    + statdir-popcount/inter64_postcount
        N           Min           Max        Median           Avg        Stddev
    x  20      0.602577       0.66121        0.6423     0.6358017   0.017562349
    +  20      1.054001      1.228634      1.097642     1.1060203   0.047991186
    Difference at 95.0% confidence
	0.470219 +/- 0.0231285
	73.9568% +/- 3.6377%
	(Student's t, pooled s = 0.0361358)

Yeah. This is clearly not the right way to do it. At least 70% slower. This is with a data set that should fit comfortably in the cache.

Since out of the two naive implementations this one is just plainly worse, we'll be using the "count on the fly" `bmap_inter64_count` function for all the future comparisons except there's a reason to highlight some other interesting behavior.

### bmap_inter64_count_r and bmap_inter64_postcount_r

Does the `restrict` keyword help? Theoretically if the compiler thinks that the incoming arrays overlap it couldn't make certain optimizations. On the other hand, there isn't much space for such optimizations here anyway.

This is what all the functions that end with `_r` test.

The short version here is:

    No difference proven at 95.0% confidence

Comparing the generated code:

    _bmap_inter64_count:
        pushq   %rbp
        movq    %rsp, %rbp
        movq    (%rdi), %rcx
        movq    (%rsi), %rdx
        xorl    %esi, %esi
        movl    %esi, %eax
        nop
        movl    %eax, %edi
        movq    (%rcx,%rsi,8), %rax
        andq    (%rdx,%rsi,8), %rax
        movq    %rax, (%rcx,%rsi,8)
        popcntq %rax, %rax
        addl    %edi, %eax
        incq    %rsi
        cmpq    $0x400, %rsi
        jne     0x110
        popq    %rbp
        ret

    _bmap_inter64_count_r:
        pushq   %rbp
        movq    %rsp, %rbp
        movq    (%rdi), %rcx
        movq    (%rsi), %rdx
        xorl    %esi, %esi
        movl    %esi, %eax
        nop
        movl    %eax, %edi
        movq    (%rcx,%rsi,8), %rax
        andq    (%rdx,%rsi,8), %rax
        movq    %rax, (%rcx,%rsi,8)
        popcntq %rax, %rax
        addl    %edi, %eax
        incq    %rsi
        cmpq    $0x400, %rsi
        jne     0x200
        popq    %rbp
        ret

Yup. The functions are exactly the same other than a different jump address. This is good.

But hold on, that's clang.

What does gcc do? (this is with `gcc -mavx`)

    x statdir/inter64_postcount
    + statdir/inter64_postcount_r
        N           Min           Max        Median           Avg        Stddev
    x 100       0.85042      0.871176      0.852957    0.85657087  0.0068027174
    + 100      0.855066      0.876472      0.858056    0.86205287    0.00698094
    Difference at 95.0% confidence
	0.005482 +/- 0.00191048
	0.639994% +/- 0.223038%
	(Student's t, pooled s = 0.0068924)

Interesting. How can restrict make things slower? Comparing the generated code (this is in the same file, so there was no mistake with compilation options or such), the restrict function is twice as long, it saves and restores the frame pointers which the non-restrict function doesn't. Both functions are vectorized. I have no idea what gcc did, but it doesn't seem very good. Both functions are very much longer than what clang generated. Protip for gcc: don't try to optimize stuff if you don't know that your optimizations help. `restrict` just allows you to make the code better. Don't make it worse.

One interesting thing worth noting with the vectorization that gcc did is that it checks if the source pointer (but not destination) is on a 16 byte boundary and if it isn't, it uses a non-vectoried version of the function. This would make sense, except that the instructions we use require 32 byte alignment (gcc only checks one of the pointers for 16-byte alignment) or no alignment at all and gcc uses the unaligned instructions anyway. So the test manages to be both unnecessary and incorrect at the same time. This is baffling. I'll have to try a newer version of gcc.

What the `restrict` version of the code does is very unclear and I really don't feel like debugging it. There's shifts, ands, ors, subtractions, additions and jumps all over the place, strange tests for alignment.

Let's get out of here.

### bmap_inter64_avx_u_count

Our own implementation of vectorized intersection and count on the fly.

    x statdir-avx/inter64_count
    + statdir-avx/inter64_avx_u_count
        N           Min           Max        Median           Avg        Stddev
    x 100      0.585282       0.73527      0.632399    0.63146839   0.019001334
    + 100      0.560045      0.637535      0.603296     0.5986947   0.017212232
    Difference at 95.0% confidence
	-0.0327737 +/- 0.00502507
	-5.19008% +/- 0.795775%
	(Student's t, pooled s = 0.0181289)

Not much to say. 5% faster than the reference version.

### bmap_inter64_avx_u_postcount

Let's just double check that the postcount variant doesn't get somehow faster with manual vectorization:

    x statdir-avx/inter64_count
    + statdir-avx/inter64_avx_u_postcount
        N           Min           Max        Median           Avg        Stddev
    x 100      0.585282       0.73527      0.632399    0.63146839   0.019001334
    + 100      1.037822      1.143331      1.110603     1.1032523   0.027815844
    Difference at 95.0% confidence
	0.471784 +/- 0.00660253
	74.7122% +/- 1.04558%
	(Student's t, pooled s = 0.0238199)

Yup. As expected.

### bmap_inter64_avx_a_count and bmap_inte64_avx_a_postcount

How do the "yes, I guarantee that my pointers are properly aligned" versions of the instructions do? We know that our data is properly aligned because it's correctly allocated with `posix_memalign`, but the `_u_` versions of the functions deliberately used the unaligned instructions. Let's see:

    x statdir-avx/inter64_avx_u_count
    + statdir-avx/inter64_avx_a_count
        N           Min           Max        Median           Avg        Stddev
    x 100      0.560045      0.637535      0.603296     0.5986947   0.017212232
    + 100      0.565072      0.637885      0.612693    0.60942185   0.014390706
    Difference at 95.0% confidence
	0.0107272 +/- 0.00439737
	1.79176% +/- 0.734492%
	(Student's t, pooled s = 0.0158643)

and for postcount:

    x statdir-avx/inter64_avx_u_postcount
    + statdir-avx/inter64_avx_a_postcount
        N           Min           Max        Median           Avg        Stddev
    x 100      1.037822      1.143331      1.110603     1.1032523   0.027815844
    + 100       1.01506       1.14685      1.083987     1.0861179    0.03578815
    Difference at 95.0% confidence
	-0.0171345 +/- 0.00888404
	-1.55309% +/- 0.805259%
	(Student's t, pooled s = 0.0320508)

Really? This small difference? And with the amount of noise in the data (the test running on a busy laptop with a running screensaver while I was at lunch), this is more or less nothing. The tests done with gcc on a different machine show between 0 and 1.5% speedup for the aligned instructions. It's probably not even worth doing a run-time check to ensure alignment of pointers.

### *_r

None of the avx functions show any difference regardless of restrict or no restrict. Neither on clang or gcc. This is expected.

### bmap_inter64_avx_a_postavxcount_r

Can we do something clever when counting the bits? Load a 256 bit register, split it into two 128 bit registers, in turn split those into four 64 bit registers, popcount those and sum that.

Let's try:

    x statdir/inter64_avx_a_postcount_r
    + statdir/inter64_avx_a_postavxcount_r
        N           Min           Max        Median           Avg        Stddev
    x 100      0.868158      0.892062      0.870806    0.87354851  0.0062611958
    + 100      0.736553      0.766799      0.740339    0.74351738  0.0070138716
    Difference at 95.0% confidence
	-0.130031 +/- 0.00184279
	-14.8854% +/- 0.210954%
	(Student's t, pooled s = 0.00664819)

Hey! It's much faster.

    x statdir/inter64_avx_a_count_r
    + statdir/inter64_avx_a_postavxcount_r
        N           Min           Max        Median           Avg        Stddev
    x 100       0.59324      0.605583      0.595645    0.59757002  0.0040832208
    + 100      0.736553      0.766799      0.740339    0.74351738  0.0070138716
    Difference at 95.0% confidence
	0.145947 +/- 0.00159071
	24.4235% +/- 0.266196%
	(Student's t, pooled s = 0.00573878)

But it's still slower than counting on the fly.

### *_ps

There is one thing that is baffling about the instruction set. All we're doing is a bitwise and between two 256 bit registers. Yet there are two different instructions to perform those ands. One is `vandps` and one is `vandpd`. The total result should be the same, when reading the documentation on the Intel website they are described differently, but only in the amount of steps taken, the total result of those descriptions is the same. There's a new instruction in AVX2 that's supposed to do the same thing, but on 256 bits at the same time. The only difference in the code is how we cast our registers which in practice means nothing because the code generated is exactly the same except which instruction is used.

Let's test this:

    x statdir-avx/inter64_avx_u_count
    + statdir-avx/inter64_avx_u_count_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      0.560045      0.637535      0.603296     0.5986947   0.017212232
    + 100       0.55812       0.62644      0.589211    0.59133161   0.014490002
    Difference at 95.0% confidence
	-0.00736309 +/- 0.00440987
	-1.22986% +/- 0.736581%
	(Student's t, pooled s = 0.0159094)

    x statdir-avx/inter64_avx_a_count
    + statdir-avx/inter64_avx_a_count_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      0.565072      0.637885      0.612693    0.60942185   0.014390706
    + 100      0.559504      0.647206      0.605988    0.60369831   0.016322484
    Difference at 95.0% confidence
	-0.00572354 +/- 0.00426504
	-0.939175% +/- 0.699851%
	(Student's t, pooled s = 0.0153869)

    x statdir-avx/inter64_avx_a_count_r
    + statdir-avx/inter64_avx_a_count_r_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      0.567086       0.63749      0.616933    0.61294017   0.014084492
    + 100      0.571841      0.639775      0.608983    0.60674502   0.015120895
    Difference at 95.0% confidence
	-0.00619515 +/- 0.00405021
	-1.01073% +/- 0.660784%
	(Student's t, pooled s = 0.0146119)

    x statdir-avx/inter64_avx_a_postcount_r
    + statdir-avx/inter64_avx_a_postcount_r_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      1.013647      1.150214      1.120865     1.1107866   0.029352729
    + 100      0.995471      1.152386       1.07586     1.0785581   0.036093498
    Difference at 95.0% confidence
	-0.0322285 +/- 0.00911837
	-2.90141% +/- 0.820893%
	(Student's t, pooled s = 0.0328962)

And with gcc (one a different cpu):

    x statdir/inter64_avx_u_count
    + statdir/inter64_avx_u_count_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      0.606431      0.627989      0.608255    0.61048798  0.0047611389
    + 100      0.607259      0.644171      0.620541    0.62149122  0.0077547099
    Difference at 95.0% confidence
	0.0110032 +/- 0.00178354
	1.80237% +/- 0.292149%
	(Student's t, pooled s = 0.00643444)

    x statdir/inter64_avx_a_count
    + statdir/inter64_avx_a_count_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      0.592638      0.616427      0.596664    0.59876876  0.0050343575
    + 100      0.590364      0.619833      0.601529    0.60198246  0.0083532685
    Difference at 95.0% confidence
	0.0032137 +/- 0.0019116
	0.536718% +/- 0.319255%
	(Student's t, pooled s = 0.00689644)

    x statdir/inter64_avx_a_count_r
    + statdir/inter64_avx_a_count_r_ps
        N           Min           Max        Median           Avg        Stddev
    x 100       0.59324      0.605583      0.595645    0.59757002  0.0040832208
    + 100      0.592873      0.631654      0.600321     0.6014889  0.0074278546
    Difference at 95.0% confidence
	0.00391888 +/- 0.00166133
	0.655803% +/- 0.278015%
	(Student's t, pooled s = 0.00599357)

    x statdir/inter64_avx_a_postcount_r
    + statdir/inter64_avx_a_postcount_r_ps
        N           Min           Max        Median           Avg        Stddev
    x 100      0.868158      0.892062      0.870806    0.87354851  0.0062611958
    + 100      0.866344      0.920214      0.897464    0.89837754   0.010474882
    Difference at 95.0% confidence
	0.024829 +/- 0.00239189
	2.84232% +/- 0.273813%
	(Student's t, pooled s = 0.00862919)

This is somewhere between nothing and weird. I think I'll trust the gcc numbers better since it's a newer cpu and a more quiet machine.

## Conclusion

`bmap_avx_u_count` is proabably the best function to use in this very specific use case unless we can really guarantee that the data is aligned, then `bmap_avx_a_count` might be better.
