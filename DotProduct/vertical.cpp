#include "stdafx.h"
#include "dotproduct.h"

// ==== Some helper functions ====

// Horizontal sum of 4 lanes of the vector
__forceinline float hadd_ps( __m128 r4 )
{
	// Add 4 values into 2
	const __m128 r2 = _mm_add_ps( r4, _mm_movehl_ps( r4, r4 ) );
	// Add 2 lower values into the final result
	const __m128 r1 = _mm_add_ss( r2, _mm_movehdup_ps( r2 ) );
	// Return the lowest lane of the result vector.
	// The intrinsic below compiles into noop, modern compilers return floats in the lowest lane of xmm0 register.
	return _mm_cvtss_f32( r1 );
}

// Horizontal sum of 8 lanes of the vector
__forceinline float hadd_ps( __m256 r8 )
{
	const __m128 low = _mm256_castps256_ps128( r8 );
	const __m128 high = _mm256_extractf128_ps( r8, 1 );
	return hadd_ps( _mm_add_ps( low, high ) );
}

// Compute a * b + acc; compiles either into a single FMA instruction, or into 2 separate SSE 1 instructions.
template<bool fma>
__forceinline __m128 fmadd_ps( __m128 a, __m128 b, __m128 acc )
{
	if constexpr( fma )
		return _mm_fmadd_ps( a, b, acc );
	else
		return _mm_add_ps( _mm_mul_ps( a, b ), acc );
}

// Same as above, for 8-wide AVX vectors
template<bool fma>
__forceinline __m256 fmadd_ps( __m256 a, __m256 b, __m256 acc )
{
	if constexpr( fma )
		return _mm256_fmadd_ps( a, b, acc );
	else
		return _mm256_add_ps( _mm256_mul_ps( a, b ), acc );
}

// ==== Vertical SSE version, with single accumulator register ====

template<bool fma>
__forceinline float sse_vertical( const float* p1, const float* p2, size_t count )
{
	assert( 0 == count % 4 );
	const float* const p1End = p1 + count;

	__m128 acc;
	// For the first 4 values we don't have anything to add yet, just multiplying
	{
		const __m128 a = _mm_loadu_ps( p1 );
		const __m128 b = _mm_loadu_ps( p2 );
		acc = _mm_mul_ps( a, b );
		p1 += 4;
		p2 += 4;
	}
	for( ; p1 < p1End; p1 += 4, p2 += 4 )
	{
		const __m128 a = _mm_loadu_ps( p1 );
		const __m128 b = _mm_loadu_ps( p2 );
		acc = fmadd_ps<fma>( a, b, acc );
	}
	return hadd_ps( acc );
}

template<>
float dotProduct<eDotProductAlgorithm::SseVertical>( const float* p1, const float* p2, size_t count )
{
	return sse_vertical<false>( p1, p2, count );
}

template<>
float dotProduct<eDotProductAlgorithm::SseVerticalFma>( const float* p1, const float* p2, size_t count )
{
	return sse_vertical<true>( p1, p2, count );
}

// ==== Vertical AVX version, with single accumulator register ====

template<bool fma>
__forceinline float avx_vertical( const float* p1, const float* p2, size_t count )
{
	assert( 0 == count % 8 );
	const float* const p1End = p1 + count;

	__m256 acc;
	// For the first 8 values we don't have anything to add yet, just multiplying
	{
		const __m256 a = _mm256_loadu_ps( p1 );
		const __m256 b = _mm256_loadu_ps( p2 );
		acc = _mm256_mul_ps( a, b );
		p1 += 8;
		p2 += 8;
	}

	for( ; p1 < p1End; p1 += 8, p2 += 8 )
	{
		const __m256 a = _mm256_loadu_ps( p1 );
		const __m256 b = _mm256_loadu_ps( p2 );
		acc = fmadd_ps<fma>( a, b, acc );
	}
	return hadd_ps( acc );
}

template<>
float dotProduct<eDotProductAlgorithm::AvxVertical>( const float* p1, const float* p2, size_t count )
{
	return avx_vertical<false>( p1, p2, count );
}

template<>
float dotProduct<eDotProductAlgorithm::AvxVerticalFma>( const float* p1, const float* p2, size_t count )
{
	return avx_vertical<true>( p1, p2, count );
}

// ==== Vertical SSE version, with up to 4 independent accumulators ====

template<int accumulators, bool fma = true>
__forceinline float sse_vertical_multi( const float* p1, const float* p2, size_t count )
{
	static_assert( accumulators > 1 && accumulators <= 4 );
	constexpr int valuesPerLoop = accumulators * 4;
	assert( 0 == count % valuesPerLoop );
	const float* const p1End = p1 + count;

	// These independent accumulators.
	// Depending on the accumulators template argument, some are unused, "unreferenced local variable" warning is OK.
	__m128 dot0, dot1, dot2, dot3;

	// For the first few values we don't have anything to add yet, just multiplying
	{
		__m128 a = _mm_loadu_ps( p1 );
		__m128 b = _mm_loadu_ps( p2 );
		dot0 = _mm_mul_ps( a, b );
		if constexpr( accumulators > 1 )
		{
			a = _mm_loadu_ps( p1 + 4 );
			b = _mm_loadu_ps( p2 + 4 );
			dot1 = _mm_mul_ps( a, b );
		}
		if constexpr( accumulators > 2 )
		{
			a = _mm_loadu_ps( p1 + 8 );
			b = _mm_loadu_ps( p2 + 8 );
			dot2 = _mm_mul_ps( a, b );
		}
		if constexpr( accumulators > 3 )
		{
			a = _mm_loadu_ps( p1 + 12 );
			b = _mm_loadu_ps( p2 + 12 );
			dot3 = _mm_mul_ps( a, b );
		}
		p1 += valuesPerLoop;
		p2 += valuesPerLoop;
	}

	// The main loop, reads valuesPerLoop floats from both vectors.
	for( ; p1 < p1End; p1 += valuesPerLoop, p2 += valuesPerLoop )
	{
		__m128 a = _mm_loadu_ps( p1 );
		__m128 b = _mm_loadu_ps( p2 );
		dot0 = fmadd_ps<fma>( a, b, dot0 );
		if constexpr( accumulators > 1 )
		{
			a = _mm_loadu_ps( p1 + 4 );
			b = _mm_loadu_ps( p2 + 4 );
			dot1 = fmadd_ps<fma>( a, b, dot1 );
		}
		if constexpr( accumulators > 2 )
		{
			a = _mm_loadu_ps( p1 + 8 );
			b = _mm_loadu_ps( p2 + 8 );
			dot2 = fmadd_ps<fma>( a, b, dot2 );
		}
		if constexpr( accumulators > 3 )
		{
			a = _mm_loadu_ps( p1 + 12 );
			b = _mm_loadu_ps( p2 + 12 );
			dot3 = fmadd_ps<fma>( a, b, dot3 );
		}
	}

	// Add the accumulators together into dot0. Using pairwise approach for slightly better precision, with 4 accumulators we compute ( d0 + d1 ) + ( d2 + d3 ).
	if constexpr( accumulators > 1 )
		dot0 = _mm_add_ps( dot0, dot1 );
	if constexpr( accumulators > 3 )
		dot2 = _mm_add_ps( dot2, dot3 );
	if constexpr( accumulators > 2 )
		dot0 = _mm_add_ps( dot0, dot2 );
	// Compute horizontal sum of all 4 lanes in dot0
	return hadd_ps( dot0 );
}

template<>
float dotProduct<eDotProductAlgorithm::SseVerticalFma2>( const float* p1, const float* p2, size_t count )
{
	return sse_vertical_multi<2>( p1, p2, count );
}
template<>
float dotProduct<eDotProductAlgorithm::SseVerticalFma3>( const float* p1, const float* p2, size_t count )
{
	return sse_vertical_multi<3>( p1, p2, count );
}
template<>
float dotProduct<eDotProductAlgorithm::SseVerticalFma4>( const float* p1, const float* p2, size_t count )
{
	return sse_vertical_multi<4>( p1, p2, count );
}
template<>
float dotProduct<eDotProductAlgorithm::SseVertical4>( const float* p1, const float* p2, size_t count )
{
	return sse_vertical_multi<4, false>( p1, p2, count );
}

// ==== Vertical AVX version, with up to 4 independent accumulators ====

template<int accumulators, bool fma = true>
__forceinline float avx_vertical_multi( const float* p1, const float* p2, size_t count )
{
	static_assert( accumulators > 1 && accumulators <= 4 );
	constexpr int valuesPerLoop = accumulators * 8;
	assert( 0 == count % valuesPerLoop );
	const float* const p1End = p1 + count;

	// These independent accumulators.
	// Depending on the accumulators template argument, some are unused, "unreferenced local variable" warning is OK.
	__m256 dot0, dot1, dot2, dot3;

	// For the first few values we don't have anything to add yet, just multiplying
	{
		__m256 a = _mm256_loadu_ps( p1 );
		__m256 b = _mm256_loadu_ps( p2 );
		dot0 = _mm256_mul_ps( a, b );
		if constexpr( accumulators > 1 )
		{
			a = _mm256_loadu_ps( p1 + 8 );
			b = _mm256_loadu_ps( p2 + 8 );
			dot1 = _mm256_mul_ps( a, b );
		}
		if constexpr( accumulators > 2 )
		{
			a = _mm256_loadu_ps( p1 + 16 );
			b = _mm256_loadu_ps( p2 + 16 );
			dot2 = _mm256_mul_ps( a, b );
		}
		if constexpr( accumulators > 3 )
		{
			a = _mm256_loadu_ps( p1 + 24 );
			b = _mm256_loadu_ps( p2 + 24 );
			dot3 = _mm256_mul_ps( a, b );
		}
		p1 += valuesPerLoop;
		p2 += valuesPerLoop;
	}

	for( ; p1 < p1End; p1 += valuesPerLoop, p2 += valuesPerLoop )
	{
		__m256 a = _mm256_loadu_ps( p1 );
		__m256 b = _mm256_loadu_ps( p2 );
		dot0 = fmadd_ps<fma>( a, b, dot0 );
		if constexpr( accumulators > 1 )
		{
			a = _mm256_loadu_ps( p1 + 8 );
			b = _mm256_loadu_ps( p2 + 8 );
			dot1 = fmadd_ps<fma>( a, b, dot1 );
		}
		if constexpr( accumulators > 2 )
		{
			a = _mm256_loadu_ps( p1 + 16 );
			b = _mm256_loadu_ps( p2 + 16 );
			dot2 = fmadd_ps<fma>( a, b, dot2 );
		}
		if constexpr( accumulators > 3 )
		{
			a = _mm256_loadu_ps( p1 + 24 );
			b = _mm256_loadu_ps( p2 + 24 );
			dot3 = fmadd_ps<fma>( a, b, dot3 );
		}
	}

	// Add the accumulators together into dot0. Using pairwise approach for slightly better precision, with 4 accumulators we compute ( d0 + d1 ) + ( d2 + d3 ).
	if constexpr( accumulators > 1 )
		dot0 = _mm256_add_ps( dot0, dot1 );
	if constexpr( accumulators > 3 )
		dot2 = _mm256_add_ps( dot2, dot3 );
	if constexpr( accumulators > 2 )
		dot0 = _mm256_add_ps( dot0, dot2 );
	// Return horizontal sum of all 8 lanes of dot0
	return hadd_ps( dot0 );
}

template<>
float dotProduct<eDotProductAlgorithm::AvxVerticalFma2>( const float* p1, const float* p2, size_t count )
{
	return avx_vertical_multi<2>( p1, p2, count );
}
template<>
float dotProduct<eDotProductAlgorithm::AvxVerticalFma3>( const float* p1, const float* p2, size_t count )
{
	return avx_vertical_multi<3>( p1, p2, count );
}
template<>
float dotProduct<eDotProductAlgorithm::AvxVerticalFma4>( const float* p1, const float* p2, size_t count )
{
	return avx_vertical_multi<4>( p1, p2, count );
}