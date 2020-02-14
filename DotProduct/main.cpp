#include "stdafx.h"

// constexpr size_t vectorLength = 64 * 1024 * 1024;
constexpr size_t vectorLength = 256 * 1024;	// 256k floats = 1MB of data.

// Just for lulz, you can replace this with false, and the source data will be produced in a way so it's not on the cache when calling the dotProduct function, and see what happens.
constexpr bool cacheInputData = true;

static void printHelp()
{
	printf( "Valid arguments:\n" );
	for( uint8_t i = 0; i < (uint8_t)eDotProductAlgorithm::valuesCount; i++ )
		printf( "%i: %s\n", (int)i, algorithmName( (eDotProductAlgorithm)i ) );
}

int main( int argc, const char* argv[] )
{
	if( argc != 2 )
	{
		printHelp();
		return 1;
	}
	int algoInt;
	if( !nonstd::atoi( argv[ 1 ], algoInt ) || algoInt < 0 || algoInt >= (int)eDotProductAlgorithm::valuesCount )
	{
		printf( "Please provide a single integer argument, within [ 0 .. %i ] interval\n", (int)eDotProductAlgorithm::valuesCount - 1 );
		printHelp();
		return 2;
	}

	const eDotProductAlgorithm algo = (eDotProductAlgorithm)algoInt;
	auto v1 = alignedArray<float>( vectorLength );
	auto v2 = alignedArray<float>( vectorLength );
	fillRandomVector( cacheInputData, v1.get(), vectorLength, 11 );
	fillRandomVector( cacheInputData, v2.get(), vectorLength, 12 );
	dispatchAndMeasure( algo, v1.get(), v2.get(), vectorLength );
	return 0;
}