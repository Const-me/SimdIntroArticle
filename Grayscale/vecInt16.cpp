#include "stdafx.h"
#include "grayscale.h"

// ==== Vector uint16_t SSE2 ====
namespace Sse
{
	// Pack red channel of 8 pixels into uint16_t lanes, in [ 0 .. 0xFF00 ] interval
	inline __m128i packRed( __m128i a, __m128i b )
	{
		const __m128i mask = _mm_set1_epi32( 0xFF );
		a = _mm_and_si128( a, mask );
		b = _mm_and_si128( b, mask );
		const __m128i packed = _mm_packus_epi32( a, b );
		return _mm_slli_si128( packed, 1 );
	}

	// Pack green channel of 8 pixels into uint16_t lanes, in [ 0 .. 0xFF00 ] interval
	inline __m128i packGreen( __m128i a, __m128i b )
	{
		const __m128i mask = _mm_set1_epi32( 0xFF00 );
		a = _mm_and_si128( a, mask );
		b = _mm_and_si128( b, mask );
		return _mm_packus_epi32( a, b );
	}

	// Pack blue channel of 8 pixels into uint16_t lanes, in [ 0 .. 0xFF00 ] interval
	inline __m128i packBlue( __m128i a, __m128i b )
	{
		const auto mask = _mm_set1_epi32( 0xFF00 );
		a = _mm_srli_si128( a, 1 );
		b = _mm_srli_si128( b, 1 );
		a = _mm_and_si128( a, mask );
		b = _mm_and_si128( b, mask );
		return _mm_packus_epi32( a, b );
	}

	// Load 8 pixels, split into RGB channels.
	inline void loadRgb( const __m128i *src, __m128i& red, __m128i& green, __m128i& blue )
	{
		const auto a = _mm_loadu_si128( src );
		const auto b = _mm_loadu_si128( src + 1 );
		red = packRed( a, b );
		green = packGreen( a, b );
		blue = packBlue( a, b );
	}

	// Compute brightness of 8 pixels. Input is 16-bit numbers in [ 0 .. 0xFF00 ] interval, output is 16-bit numbers in [ 0 .. 0xFF ] interval.
	inline __m128i brightness( __m128i r, __m128i g, __m128i b )
	{
		// Multiply RGB by these coefficients. From the documentation of _mm_mulhi_epu16:
		// Multiply the packed unsigned 16-bit integers in "a" and "b", producing intermediate 32-bit integers, and store the high 16 bits of the intermediate integers 
		r = _mm_mulhi_epu16( r, _mm_set1_epi16( (short)mulRed ) );
		g = _mm_mulhi_epu16( g, _mm_set1_epi16( (short)mulGreen ) );
		b = _mm_mulhi_epu16( b, _mm_set1_epi16( (short)mulBlue ) );
		// Add them together, with saturated additions just in case.
		// They shouldn't exceed 0xFF00 because these float coefficients add up to 1.0, but in case you modify the coefficients, you don't want the wrapping of high brightness into black.
		const auto result = _mm_adds_epu16( _mm_adds_epu16( r, g ), b );
		// Shift right by 8 bits = divide by 0x100, scaling the output to [ 0 .. 0xFF ] range.
		return _mm_srli_epi16( result, 8 );
	}
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::SseInt16>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	assert( 0 == ( count % 16 ) );

	const __m128i *source = ( const __m128i * )sourcePixels;
	const __m128i *sourceEnd = source + ( count / 4 );
	__m128i *dest = ( __m128i* )( destinationBytes );
	using namespace Sse;

	for( ; source < sourceEnd; source += 4, dest++ )
	{
		// Compute brightness of 16 pixels.
		__m128i r, g, b;
		loadRgb( source, r, g, b );
		const __m128i low = brightness( r, g, b );
		loadRgb( source + 2, r, g, b );
		const __m128i hi = brightness( r, g, b );

		// Pack 16-bit integers into bytes, and store the result.
		const __m128i bytes = _mm_packus_epi16( low, hi );
		_mm_storeu_si128( dest, bytes );
	}
}

// ==== Vector uint16_t AVX2 ====

namespace Avx
{
	// Pack red channel of 16 pixels into uint16_t lanes, in [ 0 .. 0xFF00 ] interval.
	// The order of the pixels is a0, a1, a2, a3, b0, b1, b2, b3, a4, a5, a6, a7, b4, b5, b6, b7.
	inline __m256i packRed( __m256i a, __m256i b )
	{
		const __m256i mask = _mm256_set1_epi32( 0xFF );
		a = _mm256_and_si256( a, mask );
		b = _mm256_and_si256( b, mask );
		const __m256i packed = _mm256_packus_epi32( a, b );
		return _mm256_slli_si256( packed, 1 );
	}

	// Pack green channel of 16 pixels into uint16_t lanes, in [ 0 .. 0xFF00 ] interval
	inline __m256i packGreen( __m256i a, __m256i b )
	{
		const __m256i mask = _mm256_set1_epi32( 0xFF00 );
		a = _mm256_and_si256( a, mask );
		b = _mm256_and_si256( b, mask );
		return _mm256_packus_epi32( a, b );
	}

	// Pack blue channel of 16 pixels into uint16_t lanes, in [ 0 .. 0xFF00 ] interval
	inline __m256i packBlue( __m256i a, __m256i b )
	{
		const auto mask = _mm256_set1_epi32( 0xFF00 );
		a = _mm256_srli_si256( a, 1 );
		b = _mm256_srli_si256( b, 1 );
		a = _mm256_and_si256( a, mask );
		b = _mm256_and_si256( b, mask );
		return _mm256_packus_epi32( a, b );
	}

	// Load 16 pixels, split into RGB channels
	inline void loadRgb( const __m256i *src, __m256i& red, __m256i& green, __m256i& blue )
	{
		const auto a = _mm256_loadu_si256( src );
		const auto b = _mm256_loadu_si256( src + 1 );
		red = packRed( a, b );
		green = packGreen( a, b );
		blue = packBlue( a, b );
	}

	// Compute brightness of 16 pixels. Input is 16-bit numbers in [ 0 .. 0xFF00 ] interval, output is 16-bit numbers in [ 0 .. 0xFF ] interval.
	inline __m256i brightness( __m256i r, __m256i g, __m256i b )
	{
		r = _mm256_mulhi_epu16( r, _mm256_set1_epi16( (short)mulRed ) );
		g = _mm256_mulhi_epu16( g, _mm256_set1_epi16( (short)mulGreen ) );
		b = _mm256_mulhi_epu16( b, _mm256_set1_epi16( (short)mulBlue ) );
		const auto result = _mm256_adds_epu16( _mm256_adds_epu16( r, g ), b );
		return _mm256_srli_epi16( result, 8 );
	}
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::AvxInt16>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	assert( 0 == ( count % 32 ) );

	const __m256i *source = ( const __m256i * )sourcePixels;
	const __m256i *sourceEnd = source + ( count / 8 );
	__m256i *dest = ( __m256i* )( destinationBytes );
	using namespace Avx;

	for( ; source < sourceEnd; source += 4, dest++ )
	{
		// Compute brightness of 32 pixels.
		__m256i r, g, b;
		loadRgb( source, r, g, b );
		__m256i low = brightness( r, g, b );
		loadRgb( source + 2, r, g, b );
		__m256i hi = brightness( r, g, b );

		// The pixel order is weird in low/high variables, due to the way 256-bit AVX2 pack instructions are implemented. They both contain pixels in the following order:
		// 0, 1, 2, 3,  8, 9, 10, 11,  4, 5, 6, 7,  12, 13, 14, 15
		// Permute them to be sequential by shuffling 64-bit blocks.
		constexpr int permuteControl = _MM_SHUFFLE( 3, 1, 2, 0 );
		low = _mm256_permute4x64_epi64( low, permuteControl );
		hi = _mm256_permute4x64_epi64( hi, permuteControl );

		// Pack 16-bit integers into bytes
		__m256i bytes = _mm256_packus_epi16( low, hi );

		// Once again, fix the order after 256-bit pack instruction.
		bytes = _mm256_permute4x64_epi64( bytes, permuteControl );

		// Store the results
		_mm256_storeu_si256( dest, bytes );
	}
}