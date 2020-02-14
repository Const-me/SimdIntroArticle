#include "stdafx.h"
#include "Image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void Image::save( const char* pngPath )
{
	for( uint32_t& pixel : ( *this ) )
		pixel |= 0xFF000000;
	stbi_write_png( pngPath, size.cx, size.cy, 4, pixels.get(), size.cx * 4 );
}