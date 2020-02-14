#pragma once
#include "floodFill.h"

// Command-line arguments parser.
struct Arguments
{
	// Default both to invalid
	const char* source = nullptr, *destination = nullptr;
	// Default to vector version
	eFloodFillAlgorithm algorithm = eFloodFillAlgorithm::VectorBlocksBits;
	// Default to invalid
	CPoint startingPoint = CPoint{ -1, -1 };
	// Default to green
	uint32_t color = 0xFF00;
	// Default to something reasonable
	uint8_t tolerance = 16;

	// Try to parse what was passed to the command-line, returns false and prints errors if failed.
	bool parse( int argc, const char* argv[] );
	// Ensure the required parameters were supplied.
	bool isValid() const;

	static void printHelp();

	using pfnFillFunc = void( *)( Image& image, CPoint pt, uint32_t fillColor, uint8_t tolerance );
	// Get function pointer to flood fill an image using the algorithm specified in this->algorithm value. Returns a specialized version of floodFill<> template function.
	pfnFillFunc fillFunc() const;

private:

	bool parseColor( const char* str );
	bool parsePoint( const char* str );
	bool parseAlgorithm( const char* str );
	bool parseTolerance( const char* str );
};