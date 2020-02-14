#pragma once
// Common C stuff
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <climits>
#include <string.h>

// SSE SIMD intrinsics
#include <xmmintrin.h>
// AVX SIMD intrinsics
#include <immintrin.h>

// Common C++ stuff
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <random>
#include <chrono>

// A wrapper around std::chrono::high_resolution_clock which starts measuring time once constructed, and reports elapsed time
class Stopwatch
{
	using Clock = std::chrono::high_resolution_clock;
	const Clock::time_point start;

	template<class _Period>
	double elapsed() const
	{
		const Clock::time_point finish = Clock::now();
		const auto timespan = finish - start;
		return std::chrono::duration_cast<std::chrono::duration<double, _Period>>( timespan ).count();
	}

public:

	Stopwatch() :
		start( Clock::now() )
	{ }

	// Milliseconds since constructed
	double elapsedMilliseconds() const
	{
		return elapsed<std::milli>();
	}

	// Microseconds since constructed
	double elapsedMicroseconds() const
	{
		return elapsed<std::micro>();
	}
};

// A wrapper around std::chrono::high_resolution_clock which prints the time passed between constructor and destructor.
class PerfTimer
{
	const char* const what;
	const Stopwatch stopwatch;

public:
	PerfTimer( const char* measure ) : what( measure ) { }

	~PerfTimer()
	{
		const double ms = stopwatch.elapsedMilliseconds();
		printf( "%s: %g ms\n", what, ms );
	}
};
#define MEASURE_THIS_FUNCTION() PerfTimer __time{ __func__  }

namespace nonstd
{
	// Parse string into integer
	inline bool atoi( const char* str, int& parsed, int radix = 10 )
	{
		// https://stackoverflow.com/a/6154614/126995
		if( nullptr == str || '\0' == str[ 0 ] )
			return false;
		char *end = nullptr;
		errno = 0;
		const long res = strtol( str, &end, radix );
		if( errno == ERANGE || end[ 0 ] != '\0' || res < INT_MIN || res > INT_MAX )
			return false;
		parsed = (int)res;
		return true;
	}
}

namespace details
{
	// Unfortunately, VC++ doesn't support std::aligned_alloc from C++/17 spec:
	// https://developercommunity.visualstudio.com/content/problem/468021/c17-stdaligned-alloc缺失.html

	// Allocate aligned block of memory
	inline void* alignedMalloc( size_t size, size_t alignment )
	{
#ifdef _MSC_VER
		return _aligned_malloc( size, alignment );
#else
		return aligned_alloc( alignment, size );
#endif
	}

	// Free aligned block of memory
	inline void alignedFree( void* pointer )
	{
#ifdef _MSC_VER
		_aligned_free( pointer );
#else
		free( pointer );
#endif
	}

	// Changes the size of the memory block pointed to by memblock. The function may move the memory block to a new location
	inline void* alignedRealloc( void *memblock, size_t size, size_t alignment )
	{
		// It's only used by stb_image.
#ifdef _MSC_VER
		return _aligned_realloc( memblock, size, alignment );
#else
		// Unfortunately, aligned_realloc is only available on Windows. Implementing a workaround:
		// https://stackoverflow.com/a/9078627/126995
		void* const reallocated = realloc( memblock, size );
		const bool isAligned = ( 0 == ( ( (size_t)reallocated ) % alignment ) );
		if( isAligned || nullptr == reallocated )
			return reallocated;

		void* const copy = aligned_alloc( alignment, size );
		if( nullptr == copy )
		{
			free( reallocated );
			return nullptr;
		}
		memcpy( copy, reallocated, size );
		free( reallocated );
		return copy;
#endif
	}

	// Enables std::unique_ptr to release aligned memory, without using a custom deleter which would double the size of the smart pointer.
	struct AlignedDeleter
	{
		inline void operator()( void* pointer )
		{
			alignedFree( pointer );
		}
	};
}

// Allocate and return block of memory aligned by at least 32 bytes. This does not call constructors, the memory is uninitialized. Destructors aren't called, either.
// If that's not what you want, you can wrap alignedMalloc/alignedFree into custom allocator, and use std::vector instead: https://stackoverflow.com/a/12942652/126995
template<class T>
inline std::unique_ptr<T[], details::AlignedDeleter> alignedArray( size_t size )
{
	constexpr size_t minimumAlignment = 32;
	constexpr size_t align = std::max( alignof( T ), minimumAlignment );

	if( size <= 0 )
		throw std::invalid_argument( "alignedArray() function doesn't support zero-length arrays." );
	T* const pointer = (T*)details::alignedMalloc( size * sizeof( T ), align );
	if( nullptr == pointer )
		throw std::bad_alloc();

	return std::unique_ptr<T[], details::AlignedDeleter>{ pointer };
}

#ifndef _MSC_VER
// A few compatibility things for building with gcc or clang
#define __forceinline __attribute__((always_inline)) inline
#define __vectorcall
inline void __debugbreak()
{
	__asm__ volatile( "int $0x03" );
}
#endif