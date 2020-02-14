#include "stdafx.h"
#include "grayscale.h"
// Implement vectorized float versions.

// ==== Vectorized 4-wide float version ====

// returns (float)(pixels & andMask), for all 4 integer lanes of the input
inline __m128 makeFloats( __m128i pixels, int andMask )
{
	pixels = _mm_and_si128( pixels, _mm_set1_epi32( andMask ) );
	return _mm_cvtepi32_ps( pixels );
}

// Convert 4 pixels into grayscale, return 4-wide int32 vector.
template<bool fma>
inline __m128i grayscale_float4( const __m128i *source )
{
	__m128i pixels = _mm_loadu_si128( source );
	const __m128 red = makeFloats( pixels, 0xFF );
	const __m128 green = makeFloats( pixels, 0xFF00 );
	const __m128 blue = makeFloats( pixels, 0xFF0000 );
	__m128 res = _mm_mul_ps( red, _mm_set1_ps( mulRedFloat ) );
	if constexpr( fma )
	{
		// Using FMA to multiply + accumulate
		res = _mm_fmadd_ps( green, _mm_set1_ps( mulGreenFloat / 0x100 ), res );
		res = _mm_fmadd_ps( blue, _mm_set1_ps( mulBlueFloat / 0x10000 ), res );
	}
	else
	{
		// No FMA, doing the same math as above with separate add and mul instructions.
		res = _mm_add_ps( res, _mm_mul_ps( green, _mm_set1_ps( mulGreenFloat / 0x100 ) ) );
		res = _mm_add_ps( res, _mm_mul_ps( blue, _mm_set1_ps( mulBlueFloat / 0x10000 ) ) );
	}
	return _mm_cvtps_epi32( res );
}

template<bool fma>
inline void grayscale_sse2_float( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	assert( 0 == ( count % 16 ) );

	const __m128i *source = ( const __m128i * )sourcePixels;
	const __m128i *sourceEnd = source + ( count / 4 );
	__m128i *dest = ( __m128i* )( destinationBytes );

	for( ; source < sourceEnd; source += 4, dest++ )
	{
		// Compute brightness of 16 pixels
		const __m128i r0 = grayscale_float4<fma>( source );
		const __m128i r1 = grayscale_float4<fma>( source + 1 );
		const __m128i r2 = grayscale_float4<fma>( source + 2 );
		const __m128i r3 = grayscale_float4<fma>( source + 3 );

		// Pack 32-bit integers into bytes, and store the result.
		const __m128i r01 = _mm_packs_epi32( r0, r1 );
		const __m128i r23 = _mm_packs_epi32( r2, r3 );
		const __m128i bytes = _mm_packus_epi16( r01, r23 );
		_mm_storeu_si128( dest, bytes );
	}
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::SseFloat>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	grayscale_sse2_float<false>( sourcePixels, destinationBytes, count );
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::SseFloatFma>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	grayscale_sse2_float<true>( sourcePixels, destinationBytes, count );
}

// ==== Vectorized 8-wide float version ====

// returns (float)(pixels & andMask), for all 8 integer lanes of the input
inline __m256 makeFloats( __m256i pixels, int andMask )
{
	pixels = _mm256_and_si256( pixels, _mm256_set1_epi32( andMask ) );
	return _mm256_cvtepi32_ps( pixels );
}

// Convert 8 pixels into grayscale, return 8-wide int32 vector.
template<bool fma>
inline __m256i grayscale_float8( const __m256i *source )
{
	__m256i pixels = _mm256_loadu_si256( source );
	const __m256 red = makeFloats( pixels, 0xFF );
	const __m256 green = makeFloats( pixels, 0xFF00 );
	const __m256 blue = makeFloats( pixels, 0xFF0000 );
	__m256 res = _mm256_mul_ps( red, _mm256_set1_ps( mulRedFloat ) );
	if constexpr( fma )
	{
		// Using FMA to multiply + accumulate
		res = _mm256_fmadd_ps( green, _mm256_set1_ps( mulGreenFloat / 0x100 ), res );
		res = _mm256_fmadd_ps( blue, _mm256_set1_ps( mulBlueFloat / 0x10000 ), res );
	}
	else
	{
		// No FMA, doing the same math as above with separate add and mul instructions.
		res = _mm256_add_ps( res, _mm256_mul_ps( green, _mm256_set1_ps( mulGreenFloat / 0x100 ) ) );
		res = _mm256_add_ps( res, _mm256_mul_ps( blue, _mm256_set1_ps( mulBlueFloat / 0x10000 ) ) );
	}
	return _mm256_cvtps_epi32( res );
}

template<bool fma>
inline void grayscale_avx_float( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	assert( 0 == ( count % 32 ) );

	const __m256i *source = ( const __m256i * )sourcePixels;
	const __m256i *sourceEnd = source + ( count / 8 );
	__m256i *dest = ( __m256i* )( destinationBytes );

	for( ; source < sourceEnd; source += 4, dest++ )
	{
		// Compute brightness of 32 pixels
		const __m256i r0 = grayscale_float8<fma>( source );
		const __m256i r1 = grayscale_float8<fma>( source + 1 );
		const __m256i r2 = grayscale_float8<fma>( source + 2 );
		const __m256i r3 = grayscale_float8<fma>( source + 3 );

		// Pack 32-bit integers into bytes, and store the result.
		const __m256i r01 = _mm256_packs_epi32( r0, r1 );
		const __m256i r23 = _mm256_packs_epi32( r2, r3 );
		const __m256i bytes = _mm256_packus_epi16( r01, r23 );
		_mm256_storeu_si256( dest, bytes );
	}
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::AvxFloat>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	grayscale_avx_float<false>( sourcePixels, destinationBytes, count );
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::AvxFloatFma>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	grayscale_avx_float<true>( sourcePixels, destinationBytes, count );
}