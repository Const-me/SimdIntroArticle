#include "stdafx.h"
#include "grayscale.h"

std::unique_ptr<uint32_t[]> createRandomImage()
{
	std::independent_bits_engine<std::default_random_engine, 32, uint32_t> re{ 11 };
	std::vector<uint32_t> data( pixelsCount );
	std::generate( begin( data ), end( data ), std::ref( re ) );

	// To simulate externally-supplied image, evict the vector from CPU cache.
	// Allocate an array of pixels, and copy the data with stream store instructions.
	std::unique_ptr<uint32_t[]> ramCopy( new uint32_t[ pixelsCount ] );
	const uint32_t* source = data.data();
	const uint32_t* sourceEnd = source + pixelsCount;
	uint32_t* dest = ramCopy.get();
	for( ; source < sourceEnd; source++, dest++ )
		_mm_stream_si32( (int*)dest, (int)( *source ) );
	return std::move( ramCopy );
}

const char* algorithmName( eGrayscaleAlgorithm algo )
{
	switch( algo )
	{
#define AN( T ) case eGrayscaleAlgorithm::T: return #T;
		AN( ScalarFloats );
		AN( ScalarInt16 );
		AN( SseFloat );
		AN( SseFloatFma );
		AN( SseInt16 );
		AN( AvxFloat );
		AN( AvxFloatFma );
		AN( AvxInt16 );
#undef AN
	}
	return nullptr;
}

template<eGrayscaleAlgorithm algo>
static double measure( const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	const Stopwatch stopwatch;
	convertToGrayscale<algo>( sourcePixels, destinationBytes, count );
	return stopwatch.elapsedMilliseconds();
}

double dispatchAndMeasure( eGrayscaleAlgorithm how, const uint32_t* sourcePixels, uint8_t* destinationBytes, size_t count )
{
	switch( how )
	{
#define AN( T ) case eGrayscaleAlgorithm::T: return measure<eGrayscaleAlgorithm::T>( sourcePixels, destinationBytes, count );
		AN( ScalarFloats );
		AN( ScalarInt16 );
		AN( SseFloat );
		AN( SseFloatFma );
		AN( SseInt16 );
		AN( AvxFloat );
		AN( AvxFloatFma );
		AN( AvxInt16 );
#undef AN
	}
	return -1;
}