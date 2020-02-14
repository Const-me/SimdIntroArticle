#pragma once
#include "../../common.h"
#include "../misc.hpp"
#include "PixelComparer.hpp"

// Dense 2D array of 16x16 blocks of bits. One block is __m256i value, fits in a single AVX vector register.
class Bitmap
{
	std::unique_ptr<__m256i[], details::AlignedDeleter> blocks;

	// Pointer to the start of the block containing the horizontal line. Input is pixels.
	__m256i* getBlockLine( int y ) const;

	// Pointer to the start of the horizontal line. Input is pixels.
	uint16_t* getLine( int y ) const;

public:
	const CSize sizePixels;
	const CSize sizeBlocks;

	Bitmap() = delete;
	~Bitmap() = default;
	// Construct from an image, by comparing every pixel to the reference value, using the tolerance.
	Bitmap( Image& image, const PixelComparer& comparer );
	// Copy the bitmap
	Bitmap( const Bitmap& source );
	// Move the bitmap
	Bitmap( Bitmap&& source );

	__m256i& blockAt( CPoint blockCoord ) const;

	bool isEmptyAt( CPoint pt ) const;

	void fillBitmap( const Bitmap& origCopy, Image& image, uint32_t fillColor ) const;

	Image dbgMakeImage() const;
};