#include "stdafx.h"
#include "Bitmap.h"
#include "../IO/Image.h"

__m256i* Bitmap::getBlockLine( int y ) const
{
	assert( y >= 0 && y < sizePixels.cy );
	return blocks.get() + sizeBlocks.cx * ( y / 16 );
}

uint16_t* Bitmap::getLine( int y ) const
{
	uint16_t* const block = (uint16_t*)getBlockLine( y );
	return block + ( y % 16 );
}

__m256i& Bitmap::blockAt( CPoint blockCoord ) const
{
	assert( isInBounds( sizeBlocks, blockCoord ) );
	return *( blocks.get() + blockCoord.y * sizeBlocks.cx + blockCoord.x );
}

bool Bitmap::isEmptyAt( CPoint pt ) const
{
	assert( isInBounds( sizePixels, pt ) );
	const __m256i* blockVec = getBlockLine( pt.y ) + ( pt.x / 16 );
	const uint16_t* blockScalar = (const uint16_t*)blockVec;
	const uint16_t line = blockScalar[ pt.y % 16 ];
	const uint16_t pixelBit = (uint16_t)1 << (uint16_t)( pt.x % 16 );
	return (bool)( line & pixelBit );
}

Image Bitmap::dbgMakeImage() const
{
	Image result = Image::create( sizePixels );
	for( CPoint px = CPoint::zero(); px.y < sizePixels.cy; px.y++ )
		for( px.x = 0; px.x < sizePixels.cx; px.x++ )
			*result.at( px ) = isEmptyAt( px ) ? UINT_MAX : 0;
	return std::move( result );
}