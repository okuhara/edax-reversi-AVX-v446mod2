/**
 * @file count_last_flip_avx512cd.c
 *
 * A function is provided to count the number of fipped disc of the last move.
 *
 * Count last flip using the flip_avx512cd way.
 * For optimization purpose, the value returned is twice the number of flipped
 * disc, to facilitate the computation of disc difference.
 *
 * @date 2023
 * @author Toshihiko Okuhara
 * @version 4.4
 * 
 */

#include "bit.h"

extern	const V4DI lmask_v4[66], rmask_v4[66];

/**
 * Count last flipped discs when playing on the last empty.
 *
 * @param pos the last empty square.
 * @param P player's disc pattern.
 * @return flipped disc count.
 */

int last_flip(int pos, unsigned long long P)
{
	__m256i PP = _mm256_set1_epi64x(P);
	__m256i	flip, outflank, rmask, lmask;
	__m128i	flip2;

		// left: look for player LS1B
	lmask = lmask_v4[pos].v4;
	outflank = _mm256_and_si256(PP, lmask);
		// set below LS1B if P is in lmask
	// flip = _mm256_andnot_si256(outflank, _mm256_add_epi64(outflank, _mm256_set1_epi64x(-1)));
	// flip = _mm256_maskz_and_epi64(_mm256_test_epi64_mask(outflank, lmask), flip, lmask);
	flip = _mm256_maskz_ternarylogic_epi64(_mm256_test_epi64_mask(PP, lmask),
		outflank, _mm256_add_epi64(outflank, _mm256_set1_epi64x(-1)), lmask, 0x08);

		// right: look for player bit with lzcnt
	rmask = rmask_v4[pos].v4;
	outflank = _mm256_srlv_epi64(_mm256_set1_epi64x(0x8000000000000000), _mm256_lzcnt_epi64(_mm256_and_si256(PP, rmask)));
		// set all bits higher than outflank
	// flip = _mm256_or_si256(flip, _mm256_andnot_si256(_mm256_add_epi64(outflank, _mm256_set1_epi64x(-1)), _mm256_andnot_si256(PP, rmask)));
	flip = _mm256_ternarylogic_epi64(flip, _mm256_add_epi64(outflank, _mm256_set1_epi64x(-1)), _mm256_andnot_si256(PP, rmask), 0xf2);

	flip2 = _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
	flip2 = _mm_or_si128(flip2, _mm_shuffle_epi32(flip2, 0x4e));
	return 2 * bit_count(_mm_cvtsi128_si64(flip2));
}
