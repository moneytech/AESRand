// AESRand.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <intrin.h>
#include <windows.h>
#include <array>

#if 0
// This 2-register XMM version has a 128-bit cycle and 256-bits of state.
// While this 256-bit version is faster in my tests, I feel it is practically less
// useful than the 64-bit version. First: 64-bit cycle-length is sufficient for most
// users, so 128-bit cycle is overkill. Especially when you consider that the top
// 64-bits can still be used to build 2^64 parallel streams of random numbers.

// Second: the 128-cycle version uses many more
// XMM registers (2-registers for return, 1-register for increment, 4-registers
// as a return value), and therefore is far less likely to remain in register space
// perpetually.

// I leave the 128-bit cycle / 256-bit version here in case anyone is interested
// to see how I plan to extend the cycles. But really, who needs more than 2^64 
// parallel random streams of 2^64 cycle length? If you really need more speed
// then you can use instruction-level parallelism to run two different 64-bit cycle
// versions instead.

__m128i increment64cycle = _mm_set_epi64x(0x2f2b29251f1d1713, 0x110D0B0705030201ll);

// The two 128-bit integers are {high[0], low[0]}, and {high[1], low[1]}.
// When put together, I call them left, and right, later on.
struct m256 {
	__m128i high;
	__m128i low;
};

// These 4-instructions should achieve 2-cycles of latency due to instruction-level parallelism.
// 
void AESRand128_inc(m256& state) {
	// Cycle 1: calculate negative-carry + change the high bits.
	__m128i negative_carry = _mm_cmpeq_epi64(state.low, _mm_setzero_si128());
	state.high = _mm_sub_epi64(state.high, increment64cycle);

	// Cycle 2: change the low-bits, and subtract negative carry to the high-bits.
	state.low = _mm_add_epi64(state.low, increment64cycle);
	state.high = _mm_sub_epi64(state.high, negative_carry);

	// Combined, these four instructions have a 64-bit cycle in the low (and high) bits, with a
	// simple +1 every 2^64 cycles to the high bits. This should cause a 128-bit cycle. I couldn't
	// think of a way to achieve a 128-bit cycle with 2-cycles of latency using only one XMM register
}

std::array<__m128i, 4> AESRand128_rand(m256& state) {
	__m128i left = _mm_unpackhi_epi64(state.low, state.high);
	__m128i right = _mm_unpacklo_epi64(state.low, state.high);

	return { _mm_aesenc_si128(_mm_aesenc_si128(left, increment64cycle), increment64cycle),
				_mm_aesenc_si128(_mm_aesenc_si128(right, increment64cycle), increment64cycle),
				_mm_aesdec_si128(_mm_aesdec_si128(left, increment64cycle), increment64cycle),
				_mm_aesdec_si128(_mm_aesdec_si128(right, increment64cycle), increment64cycle) };
}
#endif

/* Here lies the 64-bit cycle / 128-bit state version. All operations are implemented on 
128-bit __m128 / mmx registers, using intrinsics.
*/

// The first 15 prime numbers + 1 on the end: 1, 2, 3, 5, 7, 11 (0xB), 13 (0xD), (etc. etc)
// The goal for this increment is: to provide an easily provable 2^64 cycle 
// (trivial: the last bit is 1, combined with a 64-bit add in the AESRand_increment, provides a 64-bit cycle).
// And second: to change every byte before the AESRand_rand function. The primeIncrement serves as an
// XOR-key, and changing every byte works well with the AES "SubBytes" routine. A little change to a byte results
// in a dramatically different number.
__m128i primeIncrement = _mm_set_epi64x(0x2f2b29251f1d1713, 0x110D0B0705030201ll);

// This increment is the bottleneck: 1-cycle of latency, with self-dependent state. This
// cannot execute any faster than once-per-cycle, even though AMD Zen / Intel Skylake can 
// execute 3 or 4 128-bit instructions per cycle.
void AESRand_increment(__m128i& state) {
	__m128i primeIncrement = _mm_set_epi64x(0x0807060504030201, 0x0f0e0d0c0b0a09); // Bad version for failing test asap

	state = _mm_add_epi64(state, primeIncrement);
}

// A singular AES Round is actually quite bad against 
// single bit changes. It requires 4+ rounds of AES before
// an adequate 1-bit change propagates to pass 16+ GB of PractRand's 
// statistical tests.

// Ultimately, I decided upon two AES Rounds.

std::array<__m128i, 2> AESRand_rand(const __m128i state) {
	__m128i penultimate = _mm_aesenc_si128(state, primeIncrement);
	return { _mm_aesenc_si128(penultimate, primeIncrement),_mm_aesdec_si128(penultimate, primeIncrement)};
}

// A simple timer routine. Run this to benchmark the GBps on your machine.
// There are two tests here, which helps demonstrate the massive parallelism on modern
// machines, even within a single core. First, a "single state" version of 
// the RNG will be run. Second, a "double-state" version will be run, and you'll notice
// that the double-state version (despite generating 2x the data) will run nearly the same speed.
void timerTest() {
	LARGE_INTEGER start, end, frequency;
	constexpr long long ITERATIONS = 5000000000;

	// "total" variables are used to ensure that the random-numbers are used
	// to prevent the optimizer from ignoring our code. We will print the grand-total
	// to std::cout at the end of the timer benchmark.
	__m128i total = _mm_setzero_si128();
	__m128i total2 = _mm_setzero_si128();
	__m128i total3 = _mm_setzero_si128();
	__m128i total4 = _mm_setzero_si128();

	std::cout << "Beginning Single-state 'serial' test" << std::endl;

	__m128i singleState = _mm_setzero_si128();

	QueryPerformanceFrequency(&frequency);

	QueryPerformanceCounter(&start);
		for (long long i = 0; i < ITERATIONS; i++) {
			AESRand_increment(singleState);
			auto randVals = AESRand_rand(singleState);
			total = _mm_add_epi64(total, randVals[0]);
			total2 = _mm_add_epi64(total2, randVals[1]);
		}
	QueryPerformanceCounter(&end);
	total = _mm_add_epi64(total, total2);

	double time = (end.QuadPart - start.QuadPart)*1.0 / frequency.QuadPart;
	std::cout << "Total Seconds (5-billion iterations): " << time << std::endl;

	{
		constexpr double BYTES_PER_ITERATION = 32.0;
		constexpr long long GIGABYTE = 1 << 30;
		std::cout << "GBps: " << (ITERATIONS * BYTES_PER_ITERATION) / (GIGABYTE * time) << std::endl;
		std::cout << "Dummy Benchmark anti-optimizer print: " << total.m128i_u64[0] + total.m128i_u64[1] << std::endl;
	}

	// Run two states in parallel (on a single core) for maximum throughput!

	// The first seed is zero. The 2nd seed is "Deadbeef Food Cafe" + "Elite Food Dude" in l33tspeak.
	__m128i parallel_state[2] = { _mm_setzero_si128(), _mm_set_epi64x(0xDEADBEEFF00DCAFE, 0x31337F00DD00D) };

	// More "parallel totals".
	total = _mm_setzero_si128();
	total2 = _mm_setzero_si128();
	total3 = _mm_setzero_si128();
	total4 = _mm_setzero_si128();

	std::cout << "Beginning Parallel (2x) test: instruction-level parallelism for the win" << std::endl;

	QueryPerformanceCounter(&start);
	
	for (long long i = 0; i < ITERATIONS; i++) {
		AESRand_increment(parallel_state[0]);
		AESRand_increment(parallel_state[1]);
		auto randVals1 = AESRand_rand(parallel_state[0]);
		auto randVals2 = AESRand_rand(parallel_state[1]);
		total = _mm_add_epi64(total, randVals1[0]);
		total2 = _mm_add_epi64(total2, randVals1[1]);
		total3 = _mm_add_epi64(total3, randVals2[0]);
		total4 = _mm_add_epi64(total4, randVals2[1]);
	}

	QueryPerformanceCounter(&end);

	total = _mm_add_epi64(total, total2);
	total = _mm_add_epi64(total, total3);
	total = _mm_add_epi64(total, total4);

	time = (end.QuadPart - start.QuadPart)*1.0 / frequency.QuadPart;

	{
		constexpr double BYTES_PER_ITERATION = 64.0; // The number of bytes-per-iteration has grown 2x in the parallel test.
		constexpr long long GIGABYTE = 1 << 30;

		std::cout << "Time: " << time << std::endl;
		std::cout << "GBps: " << (ITERATIONS * BYTES_PER_ITERATION) / (GIGABYTE * time) << std::endl;
		std::cout << "Dummy Benchmark anti-optimizer print: " << total.m128i_u64[0] + total.m128i_u64[1] << std::endl;
	}
}

/* Some party tricks which are actually quite useful! I personally learned these tricks
from https://github.com/clMathLibraries/clRNG and xoroshiro128plus, so I had to make sure I could implement them
in my RNG!! */

// "Jump" the RNG forward by a huge jump. Helpful if you want to "reserve" numbers in a simulation.
// Ex: spawn a new thread, "reserve" 10,000 Random numbers to the sub-thread, and then carry on a 
// parallel thread with someone who owns the state. The subthread only need to be "passed" the __m128 state,
// and can generate its own RNGs in parallel. Passing a single 128-bit value is WAY faster than passing 
// 10,000+ random numbers across threads.
void jumpState(__m128i& state, long long jumpLength) {
	// A large number of additions is simply a single multiplication.
	long long lowJump = primeIncrement.m128i_u64[0] * jumpLength;
	long long highJump = primeIncrement.m128i_u64[1] * jumpLength;

	state.m128i_u64[0] += lowJump;
	state.m128i_u64[1] += highJump;
}

// Return a fully parallel stream, one guarenteed to not "intersect" with the originalStream
__m128i parallelStream(__m128i originalStream) {
	__m128i copy = originalStream;

	// While a line like the following would be "obvious":
	// copy.m128i_u64[1] += 1;
	// return copy;
	// The above two lines are horrible for only two rounds of AES.
	// AES is actually rather bad at propagating bits around by itself, it only works well
	// if the bytes are changing dramatically (see the primeIncrement, which changes every byte).

	copy.m128i_u64[1] = 6364136223846793005ULL * copy.m128i_u64[1] + 1442695040888963407ULL;
	return copy;

	// This LCGRNG has a cycle of 2^64, and changes every byte. LCGRNGs are somewhat weak, but
	// the AES-rounds will be able to do a good job of mixing after the LCGRNG has shuffled the bytes
	// around severely.
}

#include <io.h>  
#include <fcntl.h>  
#include <stdio.h>

int main()
{
	// Set the "if 0" to 1 if you want to run the benchmark and calculate GBps on your machine.
#if 0
	timerTest();
	return 0;
#else

	__m128i output[16384];
	__m128i rngState = _mm_setzero_si128();

	_setmode(_fileno(stdout), _O_BINARY);

	while (!ferror(stdout)) {
		for (int i = 0; i < 16384; i += 2) {
			AESRand_increment(rngState);
			auto randVals = AESRand_rand(rngState);
			output[i + 0] = randVals[0];
			output[i + 1] = randVals[1];
		}

		fwrite(output, 1, sizeof(output), stdout);
	}
#endif
}