#include "stdafx.h"
#include "../IO/Image.h"
#include "PixelComparer.hpp"
#include "Bitmap.h"
#include "../floodFill.h"

// When your source image contains large areas (complete 16x16 squares of pixels) to be filled, change this to true for some performance improvement.
constexpr bool optimizeEmptyBlocks = false;

struct HotBlock
{
	// Coordinates of the block.
	CPoint coord;
	// Pointer to the block in the bitmap with the above coordinates.
	// This is a performance optimization, a pointer is fast to offset to point to a neighbor, while computing it from coordinates require integer multiplication.
	__m256i* bitmapPointer;
	// Hot pixels of the block from where the fill should begin.
	std::array<uint16_t, 16> activation;
	// Load hot pixels into a vector register
	__m256i load() const
	{
		return _mm256_loadu_si256( ( const __m256i* )activation.data() );
	}
	HotBlock() = default;
	HotBlock( int x, int y, __m256i* ptr, __m256i hot ) :
		coord{ x, y }, bitmapPointer( ptr )
	{
		_mm256_storeu_si256( ( __m256i * )activation.data(), hot );
	}
};

// Implements the main part of the algorithm, i.e. the flood fill itself.
class VectorBlocksFill
{
	Bitmap& bitmap;
	std::vector<HotBlock> stack;

	void pushHotBlock( int x, int y, __m256i* pBitmap, __m256i bits );

public:

	VectorBlocksFill( Bitmap& bmp ) :
		bitmap( bmp )
	{
		// Trade some memory for performance, to reduce reallocations in runtime.
		// Not too much anyway, 2k stack entries consume 96kb RAM
		stack.reserve( 2048 );
	}

	void pushHot( CPoint pt );

	void run();
};

void VectorBlocksFill::pushHot( CPoint pt )
{
	assert( stack.empty() );
	assert( bitmap.isEmptyAt( pt ) );

	stack.emplace_back( HotBlock{} );
	HotBlock& hb = *stack.rbegin();
	hb.coord = CPoint{ pt.x / 16, pt.y / 16 };
	hb.bitmapPointer = &bitmap.blockAt( hb.coord );
	hb.activation[ pt.y % 16 ] = 1 << ( pt.x % 16 );
}

__forceinline __m256i expandX( __m256i hot, __m256i& left, __m256i& right )
{
	// The left neighbor receives the lowest bit of 16-bit lanes, shifted to the highest position.
	left = _mm256_or_si256( left, _mm256_slli_epi16( hot, 15 ) );
	// The right neighbor receives the highest bit of 16-bit lanes, shifted to the lowest positions.
	right = _mm256_or_si256( right, _mm256_srli_epi16( hot, 15 ) );

	// Expand hot areas horizontally within the block.
	const __m256i h1 = _mm256_srli_epi16( hot, 1 );
	const __m256i h2 = _mm256_slli_epi16( hot, 1 );
	return _mm256_or_si256( h1, h2 );
}

// Make a 32-byte register with lower 16 bytes from the argument, higher 16 bytes zero.
__forceinline __m256i setLow( __m128i low )
{
	return _mm256_insertf128_si256( _mm256_setzero_si256(), low, 0 );
}
// Make a 32-byte register with higher 16 bytes from the argument, lower 16 bytes zero.
__forceinline __m256i setHigh( __m128i high )
{
	return _mm256_insertf128_si256( _mm256_setzero_si256(), high, 1 );
}

__forceinline __m256i expandY( __m256i hot, __m256i& top, __m256i& bottom )
{
	const __m128i low = _mm256_castsi256_si128( hot );
	const __m128i high = _mm256_extracti128_si256( hot, 1 );

	// The top neighbor receives the lowest 16 bits, shifted into the highest position
	top = _mm256_or_si256( top, setHigh( _mm_slli_si128( low, 14 ) ) );
	// The bottom neighbor receives the highest 16 bits, shifted into the lowest position.
	bottom = _mm256_or_si256( bottom, setLow( _mm_srli_si128( high, 14 ) ) );

	// Expand hot areas vertically within the block. The shift amount is 2 bytes = 16 bits = 1 pixel in Y direction.
	const __m256i h1 = _mm256_srli_si256( hot, 2 );
	const __m256i h2 = _mm256_slli_si256( hot, 2 );

	// Unfortunately, we're not done yet because vpsrldq / vpslldq instructions are only shifting within 128-bit lanes, not the complete 32-bytes register.
	// The code below propagates fills across y=8 boundary of the block.
	const __m128i y7 = _mm_srli_si128( low, 14 );
	const __m128i y8 = _mm_slli_si128( high, 14 );
	const __m256i h3 = _mm256_setr_m128i( y8, y7 );
	return _mm256_or_si256( _mm256_or_si256( h1, h2 ), h3 );
}

// True if every last one of the 256 bits is zero.
__forceinline bool isZero( __m256i x )
{
	return (bool)_mm256_testz_si256( x, x );
}
// True if at least a single bit is set in the value.
__forceinline bool isNotZero( __m256i x )
{
	return !isZero( x );
}
// Return a register filled with all 1-s
__forceinline __m256i allOnes()
{
	const __m256i undef = _mm256_undefined_si256();
	return _mm256_cmpeq_epi32( undef, undef );
}
__forceinline bool isAllOne( __m256i x )
{
	x = _mm256_xor_si256( x, allOnes() );
	return isZero( x );
}

// The inner loop of the algorithm, fills 16x16 block activated by another 16x16 block.
// The complete loop only operates on registers.
__forceinline __m256i fillBlock( __m256i bmp, __m256i hot, __m256i &left, __m256i &top, __m256i &right, __m256i &bottom )
{
	// Bitmap has 1 for empty pixels.
	// Activation has 1 for activated pixels.

	while( true )
	{
		if( _mm256_testz_si256( hot, bmp ) )
		{
			// The intersection of ( hot & bmp ) has no pixels. We've finished with this block.
			return bmp;
		}

		// Fill the new pixels = clear the bits in the bmp, also trim the hot areas so it only contains newly filled pixels
		const __m256i filled = _mm256_and_si256( hot, bmp );
		bmp = _mm256_andnot_si256( hot, bmp );
		hot = filled;

		// Expand the hot area by 1 pixel in all 4 directions
		const __m256i h1 = expandX( hot, left, right );
		const __m256i h2 = expandY( hot, top, bottom );
		// Update the hot area
		hot = _mm256_or_si256( h1, h2 );
	}
}

void VectorBlocksFill::pushHotBlock( int x, int y, __m256i* pBitmap, __m256i bits )
{
	if( _mm256_testz_si256( bits, *pBitmap ) )
	{
		// Either that neighbor was not activated i.e. the bits are all zeros, or the bitmap doesn't have any empty pixels at the activated locations.
		// In either case no need to push the block.
		return;
	}
	stack.emplace_back( HotBlock{ x, y, pBitmap, bits } );
}

void VectorBlocksFill::run()
{
	// Count of blocks in the line, this value is used to offset the pointer to move to -Y / +Y neighbors
	const size_t blocksPerLine = (size_t)bitmap.sizeBlocks.cx;

	// The outer loop of the fill, processes the stack until it's empty, which means the fill is finished.
	while( !stack.empty() )
	{
		// Pop entry from the stack
		const HotBlock& hb = *stack.rbegin();
		const CPoint coord = hb.coord;
		__m256i* const source = hb.bitmapPointer;
		const __m256i activation = hb.load();
		stack.pop_back();

		__m256i bmp = *source;
		if constexpr( optimizeEmptyBlocks )
		{
			if( isAllOne( bmp ) )
			{
				// The whole block is ones, i.e. empty space.
				// It will be completely filled regardless on where it was activated, and all 4 neighbors will be activated, too, at their complete sides.
				*source = _mm256_setzero_si256();

				if( coord.x > 0 )
					pushHotBlock( coord.x - 1, coord.y, source - 1, _mm256_set1_epi16( (short)0x8000 ) );
				if( coord.y > 0 )
				{
					const __m256i bottomLine = _mm256_setr_epi16( 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1 );
					pushHotBlock( coord.x, coord.y - 1, source - blocksPerLine, bottomLine );
				}
				if( coord.x + 1 < bitmap.sizeBlocks.cx )
					pushHotBlock( coord.x + 1, coord.y, source + 1, _mm256_set1_epi16( 1 ) );
				if( coord.y + 1 < bitmap.sizeBlocks.cy )
				{
					const __m256i topLine = _mm256_setr_epi16( -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
					pushHotBlock( coord.x, coord.y + 1, source + blocksPerLine, topLine );
				}
				continue;
			}
		}

		// Fill the 16x16 block of bits.
		__m256i left = _mm256_setzero_si256();
		__m256i top = _mm256_setzero_si256();
		__m256i right = _mm256_setzero_si256();
		__m256i bottom = _mm256_setzero_si256();
		*source = fillBlock( bmp, activation, left, top, right, bottom );

		// Handle activated neighbors, if any, by pushing hot blocks onto the stack.
		if( coord.x > 0 )
			pushHotBlock( coord.x - 1, coord.y, source - 1, left );
		if( coord.y > 0 )
			pushHotBlock( coord.x, coord.y - 1, source - blocksPerLine, top );
		if( coord.x + 1 < bitmap.sizeBlocks.cx )
			pushHotBlock( coord.x + 1, coord.y, source + 1, right );
		if( coord.y + 1 < bitmap.sizeBlocks.cy )
			pushHotBlock( coord.x, coord.y + 1, source + blocksPerLine, bottom );
	}
}

template<>
void floodFill<eFloodFillAlgorithm::VectorBlocksBits>( Image& image, CPoint pt, uint32_t fillColor, uint8_t tolerance )
{
	PerfTimer __timer( "eFloodFillAlgorithm::VectorBlocksBits" );

	// Compare colors of the complete image, produce 1 bit/pixel version, laid out in memory as a 2D array of 16x16 blocks of bits.
	PixelComparer comparer{ image[ pt ], tolerance };
	Bitmap source{ image, comparer };

	// Make a backup. Fill algorithm selectively clears the bitmap, and after it's complete we want to know which pixels were filled.
	Bitmap sourceBackup = source;

	// Run the fill algorithm.
	VectorBlocksFill fill{ source };
	fill.pushHot( pt );
	fill.run();

	// Paint pixels in the image by comparing the 2 bitmaps, before and after the fill algorithm.
	source.fillBitmap( sourceBackup, image, fillColor );
}