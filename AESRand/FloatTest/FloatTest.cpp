// FloatTest.cpp : Testing an idea to generate 23-bit floating point numbers quickly through SIMD
//

#include "pch.h"
#include <iostream>
#include <intrin.h>

// Use all 32-bits to create a 32-bit floating point result between [0, 1).
// This isn't strictly uniform, because of the chance of the 0-denormals
// "fastResult" would be uniform, faster to execute, but only provide 23-bits
// of randomness.
__m128 toFloats(__m128i input) {
	// Isolate the sign and exponent bits
	__m128i isolate = _mm_andnot_si128(_mm_set1_epi32(0xff800000), input);

	// 0x3f800000 is the magic number representing floating point 1.0
	__m128i addExponent = _mm_or_si128(_mm_set1_epi32(0x3f800000), isolate);

	// Numbers are now in between [1.0, 2.0)
	__m128 one = _mm_set1_ps(1.0);

	// Result is now in [0, 1), but we may have lost some bits.
	// We could return now, but... we can regain 9-lost bits without much effort
	__m128 fastResult = _mm_sub_ps(_mm_castsi128_ps(addExponent), one);

	// We can use these 9 bits to improve the bottom 9-bits ... if we are fine returning
	// denormals in the case of zero, then a simple "bitshift + xor" would do the job
	__m128i unused9bits = _mm_and_si128(_mm_set1_epi32(0xff800000), input);
	unused9bits = _mm_srli_epi32(unused9bits, 23);

	//Doing an _mm_xor_ps with those 9-bits results in a NAN error. Do xors 
	// in the integer domain, then convert back.

	return _mm_xor_ps(fastResult, _mm_castsi128_ps(unused9bits));

	//return _mm_castsi128_ps(rawResult);
}

int main()
{
	// Lets generate all possible 2^32 values and see what happens.

	//__m128i bits = _mm_set_epi32(0xff800003,2,1, 0); This comment to test out high-order bits getting used on the bottom
	__m128i bits = _mm_set_epi32(3, 2, 1, 0);
	__m128i bitIncrement = _mm_set_epi32(1, 1, 1, 1);

	do {
		__m128 randomFloats1_to_2 = toFloats(bits);
		// Step through this statement with a debugger: you can see
		// that the floats are being converted correctly
		bits = _mm_add_epi32(bits, bitIncrement);
	} while (bits.m128i_u32[0] != 0);
}