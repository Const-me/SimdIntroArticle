#include "stdafx.h"
#include "../IO/Image.h"
#include "Bitmap.h"
#include "loadSome.hpp"
// For filling the image, we want to avoid read-after-write latency at the end of every line, it's not insignificant.
// This is unlike constructing the bitmap, where we only reading from the image but not writing.
// That's why using templates to handle images of widths not being multiple of 16.

// A magic number to expand bits in a byte into uint32_t lanes.
static const __m256i expandBitsMagic = _mm256_setr_epi32( 0x01010101, 0x02020202, 0x04040404, 0x08080808, 0x10101010, 0x20202020, 0x40404040, 0x80808080 );

// Use bits to selectively overwrite exactly 8 pixels.
__forceinline void fill8( uint8_t bits, uint32_t* dest, __m256i filledValue )
{
	if( 0 == bits )
	{
		// None of the 8 pixels were filled, nothing to do here.
		return;
	}

	// Expand 8 bits into uint32_t pixels, UINT_MAX where the bit was set, 0 otherwise
	__m256i mask = _mm256_set1_epi8( (char)bits );	// This is AVX2, compiles into vpbroadcastb
	const __m256i andMask = expandBitsMagic;
	mask = _mm256_and_si256( mask, andMask );
	mask = _mm256_cmpeq_epi32( mask, andMask );

	// Load a horizontal line of 8 pixels
	__m256i pixels = _mm256_loadu_si256( ( const __m256i* )dest );
	// Use vector blend instruction to selectively set some pixels to the fill color
	pixels = _mm256_blendv_epi8( pixels, filledValue, mask );
	// Write the pixels back to memory.
	_mm256_storeu_si256( ( __m256i* )dest, pixels );
}

// Store 1-7 pixels to memory. AVX2 has _mm256_maskstore_epi32, not using it for 2 reasons.
// 1. I want to demonstrate how to solve this complication (dealing with uneven images) without relying on AVX-only features.
// 2. I'm not sure about the performance.
// These instructions support arbitrary store pattern, and we only need to store initial part of the register.
// While the spec is certain these instruction won't write anything into masked-out locations, it doesn't say the CPU ain't gonna mark these addresses as dirty, decreasing performance of subsequent reads from there.
template<int lanes>
__forceinline void storeSome( uint32_t* pointer, __m256i value )
{
	static_assert( lanes > 0 && lanes < 8 );
#define LOW16 _mm256_castsi256_si128( value )
#define HIGH16 _mm256_extracti128_si256( value, 1 )
	switch( lanes )
	{
	case 1:
		_mm_storeu_si32( pointer, LOW16 );
		return;
	case 2:
		_mm_storel_epi64( ( __m128i* )pointer, LOW16 );
		return;
	case 3:
		_mm_storel_epi64( ( __m128i* )pointer, LOW16 );
		// pextrd can store directly into memory: https://www.felixcloutier.com/x86/pextrb:pextrd:pextrq
		pointer[ 2 ] = (uint32_t)_mm_extract_epi32( LOW16, 2 );
		return;
	case 4:
		_mm_storeu_si128( ( __m128i* )pointer, LOW16 );
		return;
	case 5:
		_mm_storeu_si128( ( __m128i* )pointer, LOW16 );
		_mm_storeu_si32( pointer + 4, HIGH16 );
		return;
	case 6:
		_mm_storeu_si128( ( __m128i* )pointer, LOW16 );
		_mm_storel_epi64( ( __m128i* )( pointer + 4 ), HIGH16 );
		return;
	case 7:
	{
		_mm_storeu_si128( ( __m128i* )pointer, LOW16 );
		const __m128i high = HIGH16;
		_mm_storel_epi64( ( __m128i* )( pointer + 4 ), high );
		pointer[ 6 ] = (uint32_t)_mm_extract_epi32( high, 2 );
		return;
	}
	}
#undef HIGH16
#undef LOW16
	__debugbreak();
}

// Use bits to selectively overwrite <8 pixels
template<int width>
__forceinline void fillPartial( uint8_t bits, uint32_t* dest, __m256i filledValue )
{
	static_assert( width < 8 && width > 0 );
	if( 0 == bits )
	{
		// None of the 8 pixels were filled, nothing to do here.
		return;
	}

	// Expand 8 bits into uint32_t pixels, UINT_MAX where the bit was set, 0 otherwise
	__m256i mask = _mm256_set1_epi8( (char)bits );	// Compiles into vpbroadcastb
	const __m256i andMask = expandBitsMagic;
	mask = _mm256_and_si256( mask, andMask );
	mask = _mm256_cmpeq_epi32( mask, andMask );

	// width is a template argument i.e. constexpr, and loadSome is an inline function.
	// The compiler should be able to figure out the value is known at compile time, and eliminate the switch in the loadSome function.
	__m256i pixels = loadSome( dest, width );
	pixels = _mm256_blendv_epi8( pixels, filledValue, mask );
	storeSome<width>( dest, pixels );
}

template<int widthBlock>
__forceinline void fillBlock( int widthImage, int heightBlock, const __m256i* filled, const __m256i* original, uint32_t* dest, __m256i filledValue )
{
	static_assert( widthBlock <= 16 && widthBlock > 0 );
	assert( heightBlock <= 16 && heightBlock > 0 );
	assert( heightBlock > 0 && heightBlock <= 16 );

	const __m256i filledPixels = _mm256_andnot_si256( *filled, *original );
	if( _mm256_testz_si256( filledPixels, filledPixels ) )
	{
		// None of the 16x16 = 256 pixels were filled in this block. Best case performance-wise.
		return;
	}

	// Some pixels were filled. Find out which ones, and update the bitmap accordingly, filling the RGB pixels.
	alignas( 32 ) std::array<uint8_t, 32> filledBytes;
	_mm256_store_si256( ( __m256i * )filledBytes.data(), filledPixels );
	const uint8_t* fb = filledBytes.data();
	// Inner Y loop, iterates over up to 16 lines of the block
	// Note we're limiting the count of iterations to only process first `heightBlock` lines
	const uint8_t* const fbEnd = fb + heightBlock * 2;
	for( ; fb < fbEnd; fb += 2, dest += widthImage )
	{
		if constexpr( widthBlock < 8 )
		{
			// Only fill initial few lanes
			fillPartial<widthBlock>( fb[ 0 ], dest, filledValue );
		}
		else if constexpr( widthBlock == 8 )
		{
			// Fill first half of the block
			fill8( fb[ 0 ], dest, filledValue );
		}
		else if constexpr( widthBlock < 16 )
		{
			// Fill first half of the block, and then some lanes
			fill8( fb[ 0 ], dest, filledValue );
			fillPartial<widthBlock - 8>( fb[ 1 ], dest + 8, filledValue );
		}
		else
		{
			// Fill the complete 16 pixels width of the block
			assert( widthBlock == 16 );
			fill8( fb[ 0 ], dest, filledValue );
			fill8( fb[ 1 ], dest + 8, filledValue );
		}
	}
}

template<int remainder>
static void __vectorcall fillBlockLine( int widthImage, int heightBlock, const __m256i* filled, const __m256i* original, uint32_t* destPixel, __m256i filledValue )
{
	assert( widthImage % 16 == remainder );

	uint32_t* const lineEnd = destPixel + ( widthImage - remainder );
	// Handle the majority of the blocks, which are 16-pixels wide
	for( ; destPixel < lineEnd; destPixel += 16, filled++, original++ )
		fillBlock<16>( widthImage, heightBlock, filled, original, destPixel, filledValue );

	if constexpr( remainder > 0 )
	{
		// Handle the final block, which has less than 16 pixels to write.
		fillBlock<remainder>( widthImage, heightBlock, filled, original, destPixel, filledValue );
	}
}

using pfnFillBlockLine = void( __vectorcall* )( int widthImage, int heightBlock, const __m256i* filled, const __m256i* original, uint32_t* destPixel, __m256i filledValue );

static const std::array<pfnFillBlockLine, 16> s_dispatch =
{
	&fillBlockLine<0>, &fillBlockLine<1>, &fillBlockLine<2>, &fillBlockLine<3>,
	&fillBlockLine<4>, &fillBlockLine<5>, &fillBlockLine<6>, &fillBlockLine<7>,
	&fillBlockLine<8>, &fillBlockLine<9>, &fillBlockLine<10>, &fillBlockLine<11>,
	&fillBlockLine<12>, &fillBlockLine<13>, &fillBlockLine<14>, &fillBlockLine<15>,
};

void Bitmap::fillBitmap( const Bitmap& origCopy, Image& image, uint32_t fillColor ) const
{
	assert( sizePixels == origCopy.sizePixels );
	assert( sizePixels == image.size );

	const __m256i filledValue = _mm256_set1_epi32( fillColor );
	const __m256i* filled = blocks.get();
	const __m256i* original = origCopy.blocks.get();
	uint32_t* destLine = image.begin();

	const pfnFillBlockLine pfn = s_dispatch[ image.size.cx % 16 ];

	// Count of pixels in 16 lines of the image
	const size_t pixelsInBlockLine = (size_t)image.size.cx * 16;
	const int completeBlockLines = image.size.cy - image.size.cy % 16;
	uint32_t* const imageEndComplete = image.begin() + ( completeBlockLines / 16 ) * pixelsInBlockLine;

	// Outer Y loop, iterates over blocks
	while( destLine < imageEndComplete )
	{
		pfn( image.size.cx, 16, filled, original, destLine, filledValue );
		destLine += pixelsInBlockLine;
		filled += sizeBlocks.cx;
		original += sizeBlocks.cx;
	}

	// If the height is the image is not multiple of 16, handle the remaining few lines.
	if( imageEndComplete < image.end() )
	{
		const int extraLines = image.size.cy % 16;
		pfn( image.size.cx, extraLines, filled, original, destLine, filledValue );
	}
}