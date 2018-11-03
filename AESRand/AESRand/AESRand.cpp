// AESRand.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <intrin.h>
#include <windows.h>

__m128i state = _mm_setzero_si128();
__m128i key = _mm_set_epi64x(0xa5a5a5a5a5a5a5a5ll, 0x5a5a5a5a5a5a5a5all);
//__m128i key = _mm_setzero_si128();

void AESRand_increment(__m128i& state) {
	static __m128i increment = _mm_set_epi64x(0, 0x110D0B0705030201ll);
	static __m128i carryAdd = _mm_set_epi64x(1, 0);

	__m128i mask = _mm_cmpeq_epi64(state, _mm_setzero_si128());
	mask = _mm_slli_si128(mask, 8);
	__m128i mixedIncrement = _mm_blendv_epi8(increment, carryAdd, mask);
	state = _mm_add_epi64(state, mixedIncrement);
}

__m128i AESRand_randA(const __m128i state) {
	// return _mm_aesenc_si128(_mm_aesenc_si128(state, key), key); // 18.9 seconds

	__m128i left = _mm_aesenc_si128(state, key); // 16.3 seconds
	__m128i right = _mm_aesenc_si128(_mm_add_epi64(state, key), _mm_setzero_si128());
	return _mm_xor_si128(left, right);

	//return state; // 16.3 seconds
}

__m128i AESRand_randB(const __m128i state) {
	__m128i left = _mm_aesdec_si128(_mm_xor_si128(state, key), _mm_setzero_si128());
	__m128i right = _mm_aesdec_si128(state, key);
	return _mm_sub_epi64(left, right);
}

int main()
{
	LARGE_INTEGER start, end, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);
	__m128i total = _mm_setzero_si128();
	__m128i total2 = _mm_setzero_si128();


	for (long long i = 0; i < 5000000000; i++) {
		AESRand_increment(state);
		total = _mm_add_epi64(total, AESRand_randA(state));
		total2 = _mm_add_epi64(total2, AESRand_randB(state));
	}
	total = _mm_add_epi64(total, total2);
	QueryPerformanceCounter(&end);

	std::cout << "Time: " << (end.QuadPart - start.QuadPart)*1.0 / frequency.QuadPart << std::endl;
	std::cout << "GBps: " << (5000000000.0 * (32)) / (1 << 30) << std::endl;
	std::cout << "Total: " << total.m128i_u64[0] << std::endl;
}