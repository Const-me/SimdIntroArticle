#pragma once
#include <stdint.h>
#include "misc.hpp"

enum struct eFloodFillAlgorithm : uint8_t
{
	// Scalar algorithm, approximately this one: https://en.wikipedia.org/wiki/Flood_fill#Scanline_fill
	Scanline,
	// Vectorized algorithm, to my knowledge it's not documented anywhere.
	VectorBlocksBits,
};

class Image;

template<eFloodFillAlgorithm algo>
void floodFill( Image& image, CPoint pt, uint32_t fillColor, uint8_t tolerance = 16 );