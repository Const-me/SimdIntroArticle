#include "stdafx.h"
#include "Image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG

#define STBI_MALLOC( size ) details::alignedMalloc( size, 32 )
#define STBI_FREE( pointer ) details::alignedFree( pointer )
#define STBI_REALLOC( pointer, newSize ) details::alignedRealloc( pointer, newSize, 32 );
#include "stb_image.h"

Image::Image( Image&& source ) :
	size( source.size ),
	pixels( std::move( source.pixels ) )
{ }

Image::Image( uint32_t* pointer, CSize _size ) :
	size( _size ),
	pixels( pointer )
{ }

Image Image::load( const char* pngPath )
{
	CSize size;
	int components;
	uint8_t* const pointer = stbi_load( pngPath, &size.cx, &size.cy, &components, 4 );
	if( nullptr == pointer )
		throw std::runtime_error( stbi_failure_reason() );
	return Image{ (uint32_t*)pointer, size };
}

Image::Image( CSize size ) :
	size( size )
{
	pixels = std::move( alignedArray<uint32_t>( countPixels() ) );
}

Image Image::create( CSize size )
{
	return Image{ size };
}