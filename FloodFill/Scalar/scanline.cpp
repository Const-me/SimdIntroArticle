#include "stdafx.h"
#include "../IO/Image.h"
#include "../floodFill.h"

class ScalarPixelComparer
{
	const int r, g, b;
	const int tolerance;

public:

	ScalarPixelComparer( uint32_t oldColor, uint8_t tol ) :
		r( oldColor & 0xFF ),
		g( oldColor & 0xFF00 ),
		b( oldColor & 0xFF0000 ),
		tolerance( tol )
	{ }

	inline bool isEmpty( uint32_t pixel ) const
	{
		if( 0 != ( pixel & 0xFF000000 ) )
			return false;	// Already filled that pixel
		const int pr = pixel & 0xFF;
		const int pg = pixel & 0xFF00;
		const int pb = pixel & 0xFF0000;
		return abs( r - pr ) <= tolerance && abs( g - pg ) <= ( tolerance << 8 ) && abs( b - pb ) <= ( tolerance << 16 );
	}
};

class ScanlineFill
{
	enum struct Direction : int
	{
		Up = -1,
		Down = +1,
	};
	inline bool isLineValid( int y ) const
	{
		return y >= 0 && y < image.size.cy;
	}
	struct Scanline
	{
		int y;
		int begin, end;
		Direction dir;
	};

	const ScalarPixelComparer comparer;
	const uint32_t fillColor;
	Image& image;
	std::vector<Scanline> stack;

	inline bool isEmpty( CPoint pt ) const
	{
		return comparer.isEmpty( image[ pt ] );
	}
	inline bool isEmpty( int x, int y ) const
	{
		return isEmpty( CPoint{ x,y } );
	}

	void fillInitialLine( CPoint pt );
	void fillLine( const Scanline& line );

	// Write pixels from x1 (inclusive) to x2 (the last one ain't included)
	void fillSegment( uint32_t* linePointer, int x1, int x2 ) const
	{
		assert( x1 >= 0 );
		assert( x2 > x1 );
		assert( x2 <= image.size.cx );
		std::fill( linePointer + x1, linePointer + x2, fillColor );
	}

public:

	ScanlineFill( Image& img, CPoint pt, uint32_t color, uint8_t tolerance ) :
		image( img ),
		fillColor( color | 0xFF000000 ),
		comparer( img[ pt ], tolerance )
	{
		// Trade some memory for performance, to reduce count of reallocations in runtime.
		stack.reserve( 1024 * 8 );
	}

	void run( CPoint pt );
};

void ScanlineFill::run( CPoint pt )
{
	// Clear the high byte in the image, we use that byte to mark filled pixels.
	for( uint32_t& pixel : image )
		pixel &= 0xFFFFFF;

	fillInitialLine( pt );

	while( !stack.empty() )
	{
		const Scanline line = *stack.rbegin();
		stack.pop_back();
		fillLine( line );
	}
}

void ScanlineFill::fillInitialLine( CPoint pt )
{
	uint32_t* const linePointer = image.line( pt.y );
	assert( comparer.isEmpty( linePointer[ pt.x ] ) );

	// Expand point into X-directed segment
	int xMin = pt.x;
	while( xMin > 0 && comparer.isEmpty( linePointer[ xMin - 1 ] ) )
		xMin--;
	int xMax = pt.x + 1;
	while( xMax < image.size.cx && comparer.isEmpty( linePointer[ xMax ] ) )
		xMax++;

	// Fill the pixels
	std::fill( linePointer + xMin, linePointer + xMax, fillColor );

	// Activate the neighbor lines
	Scanline sl;
	sl.begin = xMin;
	sl.end = xMax;
	if( pt.y > 0 )
	{
		sl.y = pt.y - 1;
		sl.dir = Direction::Up;
		stack.push_back( sl );
	}
	if( pt.y + 1 < image.size.cy )
	{
		sl.y = pt.y + 1;
		sl.dir = Direction::Down;
		stack.push_back( sl );
	}
}

void ScanlineFill::fillLine( const Scanline& line )
{
	uint32_t* const linePointer = image.line( line.y );

	const int yNext = line.y + (int)line.dir;
	const bool hasNextLine = isLineValid( yNext );

	const int yPrev = line.y - (int)line.dir;
	const bool hasPrevLine = isLineValid( yPrev );
	const Direction dirBackwards = (Direction)( -(int)line.dir );

	bool isFilling;
	// Begin of the current segment
	int xMin = line.begin;
	if( comparer.isEmpty( linePointer[ xMin ] ) )
	{
		// The first pixel of the segment is empty. Try to expand the segment towards 0.
		while( xMin > 0 && comparer.isEmpty( linePointer[ xMin - 1 ] ) )
			xMin--;
		isFilling = true;
		if( hasPrevLine && xMin < line.begin )
		{
			// Expanded at least by a single pixel. Push a new segment for the previous line for the expanded portion.
			stack.emplace_back( Scanline{ yPrev, xMin, line.begin, dirBackwards } );
		}
	}
	else
	{
		xMin++;
		isFilling = false;
	}

	int x = xMin;
	for( ; x < line.end; x++ )
	{
		const bool pixelEmpty = comparer.isEmpty( linePointer[ x ] );
		if( pixelEmpty == isFilling )
			continue;	// The state hasn't changed, continue filling or skipping these pixels
		isFilling = pixelEmpty;
		if( pixelEmpty )
		{
			// Started to fill a segment
			xMin = x;
		}
		else
		{
			// Finished the segment
			fillSegment( linePointer, xMin, x );
			if( hasNextLine )
				stack.emplace_back( Scanline{ yNext, xMin, x, line.dir } );
		}
	}

	if( isFilling )
	{
		// The last pixel of the segment was empty. Continue expanding the segment towards size.cx
		for( ; x < image.size.cx; x++ )
		{
			if( comparer.isEmpty( linePointer[ x ] ) )
				continue;
			break;
		}
		fillSegment( linePointer, xMin, x );
		if( hasNextLine )
			stack.emplace_back( Scanline{ yNext, xMin, x, line.dir } );

		if( hasPrevLine && x > line.end )
			stack.emplace_back( Scanline{ yPrev, line.end, x, dirBackwards } );
	}
}

template<>
void floodFill<eFloodFillAlgorithm::Scanline>( Image& image, CPoint pt, uint32_t fillColor, uint8_t tolerance )
{
	PerfTimer __timer( "eFloodFillAlgorithm::Scanline" );
	ScanlineFill fill{ image, pt, fillColor, tolerance };
	fill.run( pt );
}