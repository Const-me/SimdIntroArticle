#include "stdafx.h"
#include "grayscale.h"
// Implement the two scalar versions

template<>
void convertToGrayscale<eGrayscaleAlgorithm::ScalarFloats>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	const uint32_t* const sourceEnd = sourcePixels + count;

	for( ; sourcePixels < sourceEnd; sourcePixels++, destinationBytes++ )
	{
		const uint32_t rgba = *sourcePixels;
		const float red = ( rgba & 0xFF ) * mulRedFloat;
		const float green = ( rgba & 0xFF00 ) * ( mulGreenFloat / 0x100 );
		const float blue = ( rgba & 0xFF0000 ) * ( mulBlueFloat / 0x10000 );
		const float result = red + green + blue;
		*destinationBytes = (uint8_t)result;
	}
}

template<>
void convertToGrayscale<eGrayscaleAlgorithm::ScalarInt16>( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	const uint32_t* const sourceEnd = sourcePixels + count;

	for( ; sourcePixels < sourceEnd; sourcePixels++, destinationBytes++ )
	{
		const uint32_t pixel = *sourcePixels;
		const uint16_t red = (uint16_t)( pixel & 0xFF ) * mulRed;
		const uint16_t green = (uint16_t)( ( pixel >> 8 ) & 0xFF ) * mulGreen;
		const uint16_t blue = (uint16_t)( ( pixel >> 16 ) & 0xFF ) * mulBlue;
		*destinationBytes = (uint8_t)( ( red + green + blue ) >> 8 );
	}
}