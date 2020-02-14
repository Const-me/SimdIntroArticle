#include "stdafx.h"
#include "dotproduct.h"

template<>
float dotProduct<eDotProductAlgorithm::Scalar>( const float* p1, const float* p2, size_t count )
{
	float result = 0;
	const float* const p1End = p1 + count;
	for( ; p1 < p1End; p1++, p2++ )
		result += p1[ 0 ] * p2[ 0 ];
	return result;
}

template<>
float dotProduct<eDotProductAlgorithm::ScalarDouble>( const float* p1, const float* p2, size_t count )
{
	double result = 0;
	const float* const p1End = p1 + count;
	for( ; p1 < p1End; p1++, p2++ )
		result += p1[ 0 ] * p2[ 0 ];
	return (float)result;
}