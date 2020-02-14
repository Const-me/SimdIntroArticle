#pragma once
#include "../../common.h"
#include "../misc.hpp"

// An RGB
class Image
{
	std::unique_ptr<uint32_t[], details::AlignedDeleter> pixels;

	Image( uint32_t* pointer, CSize size );
	Image( CSize size );

public:
	const CSize size;

	size_t countPixels() const
	{
		return (size_t)size.cx * (size_t)size.cy;
	}

	Image( const Image& ) = delete;
	Image( Image&& source );
	~Image() = default;

	static Image load( const char* pngPath );
	static Image create( CSize size );

	void save( const char* pngPath );

	uint32_t* begin() { return pixels.get(); }
	uint32_t* end()
	{
		return begin() + countPixels();
	}

	bool isInBounds( CPoint pt ) const
	{
		return ::isInBounds( size, pt );
	}

	uint32_t operator[]( CPoint pt ) const
	{
		assert( isInBounds( pt ) );
		const size_t offset = (size_t)pt.y * (size_t)size.cx + pt.x;
		return pixels.get()[ offset ];
	}
	uint32_t* at( CPoint pt ) const
	{
		assert( isInBounds( pt ) );
		const size_t offset = (size_t)pt.y * (size_t)size.cx + pt.x;
		return pixels.get() + offset;
	}
	uint32_t* line( int y ) const
	{
		assert( y >= 0 && y < size.cy );
		return pixels.get() + (size_t)y * (size_t)size.cx;
	}
};