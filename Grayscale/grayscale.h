#pragma once
#include <stdint.h>

constexpr size_t pixelsCount = 3840 * 2160;

// The coefficients to produce the gray values
constexpr float mulRedFloat = 0.29891f;
constexpr float mulGreenFloat = 0.58661f;
constexpr float mulBlueFloat = 0.11448f;

// Same coefficients in 16-bit fixed point form.
constexpr uint16_t mulRed = (uint16_t)( mulRedFloat * 0x10000 );
constexpr uint16_t mulGreen = (uint16_t)( mulGreenFloat * 0x10000 );
constexpr uint16_t mulBlue = (uint16_t)( mulBlueFloat * 0x10000 );

// Create a new array of length `pixelsCount` filled with random data, not cached.
std::unique_ptr<uint32_t[]> createRandomImage();

enum struct eGrayscaleAlgorithm : uint8_t
{
	ScalarFloats,
	ScalarInt16,
	SseFloat,
	SseFloatFma,
	SseInt16,
	AvxFloat,
	AvxFloatFma,
	AvxInt16,

	valuesCount,
};

// Get the name of the algorithm, or nullptr if the argument is invalid.
const char* algorithmName( eGrayscaleAlgorithm algo );

// Run the specified algorithm, return time in milliseconds
double dispatchAndMeasure( eGrayscaleAlgorithm how, const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count );

// Various *.cpp source files in this project are actually implementing specialized versions of this function.
template<eGrayscaleAlgorithm algo>
void convertToGrayscale( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count );