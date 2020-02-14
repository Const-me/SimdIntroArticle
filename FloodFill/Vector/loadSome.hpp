#pragma once

inline __m128i load1( const uint32_t* pointer )
{
	return _mm_loadu_si32( pointer );
}
inline __m128i load2( const uint32_t* pointer )
{
	return _mm_loadl_epi64( ( const __m128i* )pointer );
}
inline __m128i load3( const uint32_t* pointer )
{
	__m128i res = load2( pointer );
	// That instruction can load values directly from memory: https://www.felixcloutier.com/x86/pinsrb:pinsrd:pinsrq
	return _mm_insert_epi32( res, *(const int*)( pointer + 2 ), 2 );
}
inline __m128i load4( const uint32_t* pointer )
{
	return _mm_loadu_si128( ( const __m128i* )pointer );
}

// When loading <= 4 lanes, the upper half is undefined.
// This is because VC++ fails to compile _mm256_insertf128_si256(a,b,0) into vmovdqa, i.e. there's no fast way to implement zero-extending version of _mm256_castsi128_si256 intrinsic.
// GCC does well, BTW: https://gcc.godbolt.org/z/3uMiEz
inline __m256i loadSome( const uint32_t* line, int count )
{
	assert( count > 0 && count < 8 );
	switch( count )
	{
	case 1:
		return _mm256_castsi128_si256( load1( line ) );
	case 2:
		return _mm256_castsi128_si256( load2( line ) );
	case 3:
		return _mm256_castsi128_si256( load3( line ) );
	case 4:
		return _mm256_castsi128_si256( load4( line ) );
	case 5:
		return _mm256_setr_m128i( load4( line ), load1( line + 4 ) );
	case 6:
		return _mm256_setr_m128i( load4( line ), load2( line + 4 ) );
	case 7:
		return _mm256_setr_m128i( load4( line ), load3( line + 4 ) );
	}
	__debugbreak();
	return _mm256_setzero_si256();
}