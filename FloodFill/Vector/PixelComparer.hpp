#pragma once

class PixelComparer
{
	__m256i minValues, maxValues;

public:

	PixelComparer( uint32_t color, uint8_t tolerance )
	{
		const __m128i colorVec = _mm_set1_epi32( (int)color );
		const __m128i tol = _mm_set1_epi8( (char)tolerance );
		// add & sub instructions below are special, they use unsigned saturation, i.e. 10-20=0, 100+200=255.
		// Scalar code doesn't have instructions to do so.
		__m128i i = _mm_subs_epu8( colorVec, tol );
		__m128i ax = _mm_adds_epu8( colorVec, tol );
		// The highest byte of the source values contains alpha channel, we want to ignore what. To do that, we set i to 0, ax to 0xFF for these bytes.
		// This way the alpha channel values are never clipped with min & max, making inRange method ignore these bytes.
		i = _mm_and_si128( i, _mm_set1_epi32( 0xFFFFFF ) );
		ax = _mm_or_si128( ax, _mm_set1_epi32( 0xFF000000 ) );
		// So far, i & ax are 16 bytes / each. Broadcast them into both halves of the minValues & maxValues
		minValues = _mm256_set_m128i( i, i );
		maxValues = _mm256_set_m128i( ax, ax );
	}

	// Test 8 pixels, return UINT_MAX for pixels which are within the range i.e. can be flood-filled, 0 otherwise
	__forceinline __m256i inRange( __m256i pixels ) const
	{
		const __m256i clippedMin = _mm256_max_epu8( pixels, minValues );	// Clipped towards +INF by minValues
		const __m256i clippedMax = _mm256_min_epu8( pixels, maxValues );	// Clipped towards zero by maxValues
		// When neither of them was clipped, clippedMin == clippedMax == pixels.
		return _mm256_cmpeq_epi32( clippedMin, clippedMax );
	}
};