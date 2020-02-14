#include "stdafx.h"
#include "IO/Image.h"
#include "Arguments.h"

int main( int argc, const char* argv[] )
{
	Arguments args;
	if( !args.parse( argc, argv ) )
		return 1;
	if( !args.isValid() )
	{
		Arguments::printHelp();
		return 2;
	}

	const auto pfnFill = args.fillFunc();

	try
	{
		Image image = Image::load( args.source );
		if( !isInBounds( image.size, args.startingPoint ) )
		{
			printf( "Starting point [ %i, %i ] is outside of the image. Image size is [ %i, %i ]\n",
				args.startingPoint.x, args.startingPoint.y, image.size.cx, image.size.cy );
			return 3;
		}

		pfnFill( image, args.startingPoint, args.color, args.tolerance );
		image.save( args.destination );
		return 0;
	}
	catch( const std::exception& ex )
	{
		// This is I/O, everything may fail.
		printf( "Flood fill failed: %s\n", ex.what() );
		return 4;
	}
}