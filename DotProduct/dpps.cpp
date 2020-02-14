#include "stdafx.h"
#include "dotproduct.h"

template<>
float dotProduct<eDotProductAlgorithm::SseDpPs>( const float* p1, const float* p2, size_t count )
{
	assert( 0 == count % 4 );

	__m128 acc = _mm_setzero_ps();
	const float* const p1End = p1 + count;
	for( ; p1 < p1End; p1 += 4, p2 += 4 )
	{
		// Load 2 vectors, 4 floats / each
		const __m128 a = _mm_loadu_ps( p1 );
		const __m128 b = _mm_loadu_ps( p2 );
		// Compute dot product of them. The 0xFF constant means "use all 4 source lanes, and broadcast the result into all 4 lanes of the destination".
		const __m128 dp = _mm_dp_ps( a, b, 0xFF );
		acc = _mm_add_ps( acc, dp );
	}
	// By the way, the intrinsic below compiles into no instructions.
	// When a function is returning a float, modern compilers pass the return value in the lowest lane of xmm0 vector register.
	return _mm_cvtss_f32( acc );
}

template<>
float dotProduct<eDotProductAlgorithm::AvxDpPs>( const float* p1, const float* p2, size_t count )
{
	assert( 0 == count % 8 );

	__m256 acc = _mm256_setzero_ps();
	const float* const p1End = p1 + count;
	for( ; p1 < p1End; p1 += 8, p2 += 8 )
	{
		// Load 2 vectors, 8 floats / each
		const __m256 a = _mm256_loadu_ps( p1 );
		const __m256 b = _mm256_loadu_ps( p2 );
		// vdpps AVX instruction does not compute dot product of 8-wide vectors.
		// Instead, that instruction computes 2 independent dot products of 4-wide vectors.
		const __m256 dp = _mm256_dp_ps( a, b, 0xFF );
		acc = _mm256_add_ps( acc, dp );
	}

	// Add the 2 results into a single float.
	const __m128 low = _mm256_castps256_ps128( acc );	//< Compiles into no instructions. The low half of a YMM register is directly accessible as an XMM register with the same number.
	const __m128 high = _mm256_extractf128_ps( acc, 1 );	//< This one however does need to move data, from high half of a register into low half. vextractf128 instruction does that.
	const __m128 result = _mm_add_ss( low, high );
	return _mm_cvtss_f32( result );
}