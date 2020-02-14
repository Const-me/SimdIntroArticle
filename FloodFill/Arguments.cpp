// This huge source file does nothing useful, just parses and validates the command-line arguments of the program.
// For this reason I rarely use C++ for the complete programs, higher-level languages are much better at boilerplate like this.
#include "stdafx.h"
#include "Arguments.h"
#include <functional>

#ifdef _MSC_VER
// Intel has documented _bswap() intrinsic: https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_bswap
// Neither VC++ nor GCC implemented that, both provide a custom incompatible version.

// Reverse the byte order of 32-bit integer "a". This intrinsic is provided for conversion between little and big endian values.
inline int _bswap( int a )
{
	// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/byteswap-uint64-byteswap-ulong-byteswap-ushort?view=vs-2019
	return (int)_byteswap_ulong( (unsigned long)a );
}

// Compare two strings ignoring case
inline int strcasecmp( const char *s1, const char *s2 )
{
	return _stricmp( s1, s2 );
}
#else
// Reverse the byte order of 32-bit integer "a". This intrinsic is provided for conversion between little and big endian values.
inline int _bswap( int a )
{
	// https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
	return (int)__builtin_bswap32( (uint32_t)a );
}
#endif

bool Arguments::isValid() const
{
	if( nullptr == source || nullptr == destination )
		return false;
	if( startingPoint.x < 0 || startingPoint.y < 0 )
		return false;
	return true;
}

static bool isSwitch( const char* arg )
{
	if( nullptr == arg )
		return false;
	return arg[ 0 ] == '-';
}

static bool isSwitch( const char* arg, const char* abbr, const char* full )
{
	assert( isSwitch( arg ) );
	if( 0 == strcasecmp( arg + 1, abbr ) )
		return true;
	if( arg[ 1 ] == '-' && 0 == strcasecmp( arg + 2, full ) )
		return true;
	return false;
}

enum struct eSwitch : uint8_t
{
	Input,
	Output,
	Color,
	Algorithm,
	Point,
	Tolerance,
};

void Arguments::printHelp()
{
	printf( "Usage example: FloodFill -i source.png -o result.png -p 12,33 -c #FF00FF -t 30 -a Scanline\n" );
}

// The function must have prototype similar to this: bool parseValue( eSwitch sw, const char* str )
template<class TFunc>
static bool parseSwitches( int argc, const char* argv[], TFunc func )
{
	struct sSwitch
	{
		eSwitch sw;
		const char* abbr, *full;
	};
	sSwitch switches[] =
	{
		{ eSwitch::Input, "i", "input" },
		{ eSwitch::Output, "o", "output" },
		{ eSwitch::Point, "p", "point" },
		{ eSwitch::Color, "c", "color" },
		{ eSwitch::Algorithm, "a", "algorithm" },
		{ eSwitch::Tolerance, "t", "tolerance" },
	};
	for( int i = 1; i < argc; i++ )
	{
		const char* const str = argv[ i ];
		if( !isSwitch( str ) )
		{
			printf( "Argument \"%s\" is not a valid switch\n", str );
			Arguments::printHelp();
			return false;
		}

		bool found = false;
		for( const auto& s : switches )
		{
			if( !isSwitch( str, s.abbr, s.full ) )
				continue;
			i++;
			if( i >= argc )
			{
				printf( "No value supplied for parameter \"%s\"\n", str );
				return false;
			}
			if( !func( s.sw, argv[ i ] ) )
			{
				printf( "Error parsing parameter \"%s\"\n", str );
				return false;
			}
			found = true;
			break;
		}
		if( !found )
		{
			printf( "Unrecognized option \"%s\"\n", str );
			return false;
		}
	}
	return true;
}

bool Arguments::parse( int argc, const char* argv[] )
{
	auto func = [ this ]( eSwitch sw, const char* value )
	{
		switch( sw )
		{
		case eSwitch::Input:
			source = value;
			return true;
		case eSwitch::Output:
			destination = value;
			return true;
		case eSwitch::Color:
			return parseColor( value );
		case eSwitch::Point:
			return parsePoint( value );
		case eSwitch::Algorithm:
			return parseAlgorithm( value );
		case eSwitch::Tolerance:
			return parseTolerance( value );
		}
		return false;
	};
	return parseSwitches( argc, argv, func );
}

bool Arguments::parseColor( const char* str )
{
	do
	{
		if( 7 != strlen( str ) )
			break;
		if( str[ 0 ] != '#' )
			break;
		int val;
		const bool parsed = nonstd::atoi( str + 1, val, 16 );
		if( !parsed )
			break;
		if( val < 0 || val > 0xFFFFFF )
			break;
		// The input string is interpreted as #RRGGBB value.
		// Modern computers are little endian, 0xCC001122 is stored in memory as 0x22 0x11 0x00 0xCC, so that RGB value is laid out as BB GG RR 00
		// The images we deal with use RGB layout however, not BGR. Need to flip 3 bytes in the color so the first of them is for the red channel.
		// Since Intel launched 80486 in 1989, CPUs have instructions to flip order of bytes: https://www.felixcloutier.com/x86/bswap
		// For this particular use case (single value) it doesn't matter how to do it.
		// Dping that to demonstrate that intrinsics aren't limited to SIMD, some scalar ones can be useful, as well.
		// Especially bitwise stuff: bsr/bsf, popcnt, pext/pdep, etc.
		val = _bswap( val ) >> 8;
		color = (uint32_t)val;
		return true;
	} while( false );
	printf( "Error parsing color string \"%s\". Examples of valid values: #000000 = black, #0000FF = blue\n", str );
	return false;
}

bool Arguments::parsePoint( const char* str )
{
	CPoint pt;
	if( 2 != sscanf( str, "%i,%i", &pt.x, &pt.y ) )
	{
		printf( "Error parsing coordinate string \"%s\". Must be in form x,y where x and y are integers\n", str );
		return false;
	}
	startingPoint = pt;
	return true;
}

bool Arguments::parseAlgorithm( const char* str )
{
	if( 0 == strcasecmp( str, "Scanline" ) )
		algorithm = eFloodFillAlgorithm::Scanline;
	else if( 0 == strcasecmp( str, "VectorBlocksBits" ) )
		algorithm = eFloodFillAlgorithm::VectorBlocksBits;
	else
	{
		printf( "Unknown algorithm \"%s\"\n", str );
		return false;
	}
	return true;
}

bool Arguments::parseTolerance( const char* str )
{
	int i;
	if( !nonstd::atoi( str, i ) )
	{
		printf( "Unable to parse tolerance value \"%s\": not an integer\n", str );
		return false;
	}
	if( i < 0 || i >= 0xFF )
	{
		printf( "Unable to parse tolerance value \"%i\": valid range is [ 0 .. 254 ]\n", i );
		return false;
	}
	tolerance = (uint8_t)i;
	return true;
}

Arguments::pfnFillFunc Arguments::fillFunc() const
{
	switch( algorithm )
	{
	case eFloodFillAlgorithm::Scanline:
		return &floodFill<eFloodFillAlgorithm::Scanline>;
	case eFloodFillAlgorithm::VectorBlocksBits:
		return &floodFill<eFloodFillAlgorithm::VectorBlocksBits>;
	}
	assert( false );
	return nullptr;
}