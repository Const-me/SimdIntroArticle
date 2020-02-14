#include "stdafx.h"
#include "Bitmap.h"
#include "../IO/Image.h"
#include "loadSome.hpp"
// Construct the bitmap by comparing all pixels of the source image to the reference value, and making these bit masks.

__forceinline int roundDown16( int x )
{
	return x & ( ~15 );
}
__forceinline int roundUp16( int x )
{
	return roundDown16( x + 15 );
}

// Converts 16 uint values (assumed to be either 0 or UINT_MAX) into 16 bits
__forceinline uint16_t makeBits( __m256i low, __m256i hi )
{
	// UINT_MAX is interpreted as -1 signed integer.
	// Signed saturation implemented by vpackssdw and packsswb instructions does the right thing, signed short -1 == 0xFFFF, signed byte -1 == 0xFF, which is what we need for the final _mm_movemask_epi8.

	// Pack uint32_t values into uint16_t
	__m256i u16 = _mm256_packs_epi32( low, hi );

	// _mm256_packs_epi32 independently processes 128 bit lanes, resulting in weird order of data.
	// Fix that order by shuffling 64-bit blocks across 128-bit lanes.
	u16 = _mm256_permute4x64_epi64( u16, _MM_SHUFFLE( 3, 1, 2, 0 ) );

	// Unpack the value into low & high 128-bit pieces.
	const __m128i low16 = _mm256_castsi256_si128( u16 );
	const __m128i high16 = _mm256_extractf128_si256( u16, 1 );

	// Pack 16 short values into 16 bytes.
	const __m128i bytes = _mm_packs_epi16( low16, high16 );
	// Convert bytes to bits. _mm_movemask_epi8 creates mask from the most significant bit of each 8-bit element.
	// BTW, these movemask instructions are hard to port from SSE/AVX to NEON. Fortunately, this particular code only targets PCs.
	const uint16_t bits = (uint16_t)_mm_movemask_epi8( bytes );
	return bits;
}

static void compareLastPixels( int remainder, const PixelComparer& comparer, const uint32_t* line, uint16_t* result )
{
	__m256i low, hi;
	if( remainder < 8 )
	{
		low = loadSome( line, remainder );
		low = comparer.inRange( low );
		hi = _mm256_setzero_si256();
	}
	else if( remainder > 8 )
	{
		low = _mm256_loadu_si256( ( const __m256i* )line );;
		hi = loadSome( line + 8, remainder - 8 );
		low = comparer.inRange( low );
		hi = comparer.inRange( hi );
	}
	else
	{
		low = _mm256_loadu_si256( ( const __m256i* )line );;
		low = comparer.inRange( low );
		hi = _mm256_setzero_si256();
	}
	const uint16_t bits = makeBits( low, hi );
	// To workaround a performance bug in VC++, when loading <= 4 lanes with loadSome function, the upper half is undefined.
	// The code below masks out undefined lanes.
	const uint16_t validMask = ( (uint16_t)0xFFFF ) >> ( (uint16_t)( 16 - remainder ) );
	*result = bits & validMask;
}

__forceinline void comparePixels( const PixelComparer& comparer, const uint32_t* line, const uint32_t* imageEnd, int pixels, uint16_t* result )
{
	const uint32_t* const endAligned = line + roundDown16( pixels );

	while( line < endAligned )
	{
		// Load 16 sequential pixels. One __m256i register holds 8 of them.
		__m256i low = _mm256_loadu_si256( ( const __m256i* )line );
		__m256i hi = _mm256_loadu_si256( ( const __m256i* )( line + 8 ) );
		line += 16;	// Advance source pointer by 16 pixels = 64 bytes = 2 registers.

		// Compare 16 pixels with the reference value, including the tolerance
		low = comparer.inRange( low );
		hi = comparer.inRange( hi );
		// Now the hi/low results are either 0 or UINT_MAX. Pack them into 16 bits.
		const uint16_t bits = makeBits( low, hi );
		*result = bits;
		// Advance destination pointer by 16 values = 32 bytes, this advances to the same Y line of the next block of the output bitmap.
		// That's essentially random RAM access, but access is write not read, CPUs are fine with that.
		result += 16;
	}

	const int remainder = pixels % 16;
	if( remainder > 0 )
	{
		if( line + 16 <= imageEnd )
		{
			// Load 16 pixels. Some of them gonna be loaded from the next line but that's OK, the comparison result will be invalid but we'll mask away these bits afterwards.
			__m256i low = _mm256_loadu_si256( ( const __m256i* )line );
			__m256i hi = _mm256_loadu_si256( ( const __m256i* )( line + 8 ) );
			// Compare 16 pixels with the reference value, including the tolerance
			low = comparer.inRange( low );
			hi = comparer.inRange( hi );
			// Now the hi/low results are either 0 or UINT_MAX. Pack them into 16 bits.
			const uint16_t resultScalar = makeBits( low, hi );
			// Mask away invalid pixels loaded from the next line. Hopefully, it's faster than partial loads.
			const uint16_t validMask = ( (uint16_t)0xFFFF ) >> ( (uint16_t)( 16 - remainder ) );
			const uint16_t bits = ( resultScalar & validMask );
			*result = bits;
		}
		else
		{
			// Loading outside of the line would access RAM outside of the image. The code might crash if we do so.
			// Need to actually load less pixels. Fortunately, this only happens at the very end of the final line of the image, i.e. that cost is OK.
			// Moving to separate function hoping the compiler won't inline - it's called rarely, we would prefer smaller code size of the hot part of the code.
			compareLastPixels( remainder, comparer, line, result );
		}
	}
}

__forceinline int blocksCount( int pixels )
{
	return ( pixels + 15 ) / 16;
}

Bitmap::Bitmap( Image& image, const PixelComparer& comparer ) :
	sizePixels( image.size ),
	sizeBlocks{ blocksCount( image.size.cx ), blocksCount( image.size.cy ) }
{
	const size_t blocksCount = (size_t)sizeBlocks.cx * (size_t)sizeBlocks.cy;
	blocks = alignedArray<__m256i>( blocksCount );

	const uint32_t* source = image.begin();
	const uint32_t* const imageEnd = image.end();
	const int height = sizePixels.cy;
	for( int y = 0; y < height; y++ )
	{
		uint16_t* dest = getLine( y );
		comparePixels( comparer, source, imageEnd, image.size.cx, dest );
		source += image.size.cx;
	}
	const int remainder = height % 16;
	if( remainder > 0 )
	{
		// The image height was not a multiple of 16 pixels. Generally, the memory returned by alignedArray() is uninitialized.
		// Clear last bitmap lines which were added for rounding i.e. they aren't found in the source image and therefore weren't written by the above code.
		// The code does that by iterating over the last line of blocks, and using bitwise AND instructions to clear bottom portion of these blocks.
		alignas( 32 ) std::array<uint16_t, 16> andMaskScalars;
		std::fill( andMaskScalars.begin(), andMaskScalars.begin() + remainder, 0xFFFF );
		std::fill( andMaskScalars.begin() + remainder, andMaskScalars.end(), 0 );
		const __m256i andMask = _mm256_load_si256( ( const __m256i* )andMaskScalars.data() );

		__m256i* ptr = getBlockLine( height - 1 );
		__m256i* const ptrEnd = ptr + sizeBlocks.cx;
		assert( ptrEnd == blocks.get() + blocksCount );
		for( ; ptr < ptrEnd; ptr++ )
			*ptr = _mm256_and_si256( *ptr, andMask );
	}
}

Bitmap::Bitmap( const Bitmap& source ) :
	sizePixels( source.sizePixels ),
	sizeBlocks( source.sizeBlocks )
{
	const size_t blocksCount = (size_t)sizeBlocks.cx * (size_t)sizeBlocks.cy;
	blocks = alignedArray<__m256i>( blocksCount );
	memcpy( blocks.get(), source.blocks.get(), blocksCount * 32 );
}

Bitmap::Bitmap( Bitmap&& source ) :
	sizePixels( source.sizePixels ),
	sizeBlocks( source.sizeBlocks ),
	blocks( std::move( source.blocks ) )
{ }