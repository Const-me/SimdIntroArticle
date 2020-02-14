#pragma once
#include "../common.h"

// Fill the buffer with uniformly-distributed random floats in the range [0 .. +1)
void fillRandomVector( bool cacheData, float* ptr, size_t count, uint32_t randomSeed );

enum struct eDotProductAlgorithm : uint8_t
{
	Scalar,
	ScalarDouble,

	SseDpPs,
	AvxDpPs,

	SseVertical,
	AvxVertical,

	SseVerticalFma,
	AvxVerticalFma,
	SseVerticalFma2,
	AvxVerticalFma2,
	SseVerticalFma3,
	AvxVerticalFma3,
	SseVerticalFma4,
	AvxVerticalFma4,
	SseVertical4,

	valuesCount,
};

// Get the name of the algorithm, or nullptr if the argument is invalid.
const char* algorithmName( eDotProductAlgorithm algo );

// Run the specified algorithm, print time along with the resulting dot product.
void dispatchAndMeasure( eDotProductAlgorithm algo, const float* p1, const float* p2, size_t count );

// Various *.cpp source files in this project are implementing specialized versions of this function.
template<eDotProductAlgorithm algo>
float dotProduct( const float* p1, const float* p2, size_t count );