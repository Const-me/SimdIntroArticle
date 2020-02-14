#pragma once
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../common.h"
#include "floodFill.h"

// GCC doesn't have a few intrinsics we use. No big deal, implementing manually on top of what's available there.
#ifndef _MSC_VER

// Set packed __m256i vector with the supplied values.
__forceinline __m256i _mm256_setr_m128i( __m128i low, __m128i high )
{
	const __m256i v = _mm256_castsi128_si256( low );
	return _mm256_insertf128_si256( v, high, 1 );
}

// Set packed __m256i vector with the supplied values.
__forceinline __m256i _mm256_set_m128i( __m128i high, __m128i low )
{
	const __m256i v = _mm256_castsi128_si256( low );
	return _mm256_insertf128_si256( v, high, 1 );
}

// Load unaligned 32-bit integer from memory into the first element of vector, and set other 3 elements to 0.
__forceinline __m128i _mm_loadu_si32( void const* mem_addr )
{
	return _mm_cvtsi32_si128( *(const int*)( mem_addr ) );
}

// Store 32-bit integer from the first element of "a" into memory. "mem_addr" does not need to be aligned on any particular boundary.
__forceinline void _mm_storeu_si32( void* mem_addr, __m128i a )
{
	*(int*)( mem_addr ) = _mm_cvtsi128_si32( a );
}
#endif