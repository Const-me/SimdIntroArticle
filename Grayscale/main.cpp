#include "stdafx.h"
#include "grayscale.h"

static void printHelp()
{
	printf( "Valid arguments:\n" );
	for( uint8_t i = 0; i < (uint8_t)eGrayscaleAlgorithm::valuesCount; i++ )
		printf( "%i: %s\n", (int)i, algorithmName( (eGrayscaleAlgorithm)i ) );
}

int main( int argc, const char* argv[] )
{
	if( argc != 2 )
	{
		printHelp();
		return 1;
	}
	int algoInt;
	if( !nonstd::atoi( argv[ 1 ], algoInt ) || algoInt < 0 || algoInt >= (int)eGrayscaleAlgorithm::valuesCount )
	{
		printf( "Please provide a single integer argument, within [ 0 .. %i ] interval\n", (int)eGrayscaleAlgorithm::valuesCount - 1 );
		printHelp();
		return 2;
	}

	const eGrayscaleAlgorithm algo = (eGrayscaleAlgorithm)algoInt;
	const auto image = createRandomImage();
	std::vector<uint8_t> result( pixelsCount );
	const double ms = dispatchAndMeasure( algo, image.get(), result.data(), pixelsCount );
	printf( "%s: %g ms\n", algorithmName( algo ), ms );
	return 0;
}