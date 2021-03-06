// IntegerRangeTest.cpp : 16-bit tests for unbiased random number generation

#include "pch.h"
#include <iostream>
#include <cstdint>
#include <optional>
#include <assert.h>

constexpr int RANGE = 11;

std::optional<uint32_t> cap_to_range(int range, uint16_t rand) {
	int keepMask = range;
	keepMask |= keepMask >> 1;
	keepMask |= keepMask >> 2;
	keepMask |= keepMask >> 4;
	keepMask |= keepMask >> 8;

	// Worst case 50% chance of rejection.
	if ((rand & keepMask) >= range) {
		return std::nullopt;
	}
	return rand & keepMask;
}

// This does NOT WORK yet.
std::optional<uint32_t> cap_to_range2(int range, uint16_t rand) {
	double invCalc = (1 << 16);
	invCalc /= range;
	uint16_t inverse = floor(invCalc);

	if (inverse*range <= rand) {
		return std::nullopt;
	}

	uint16_t middle = ((rand) * inverse) >> 16;
	uint16_t toSub = middle * range;

	return rand - toSub;
}

int main()
{
	auto functionToTest = cap_to_range;
	uint32_t histogram[RANGE];
	uint32_t rejects = 0;
	memset(histogram, 0, sizeof(histogram));
	for (uint32_t i = 0; i <= UINT16_MAX; i++) {
		auto val = functionToTest(RANGE, i);
		if (val.has_value()) {
			assert(*val < RANGE);
			histogram[*val]++;
		}
		else {
			rejects++;
		}
	}

	std::cout << "Rejects: " << rejects << std::endl;
	std::cout << "Rejects %: " << (rejects * 1.0) / (1<<16) << std::endl;

	uint32_t allEqual = histogram[0];
	for (int i = 1; i < RANGE; i++) {
		if (histogram[i] != allEqual) {
			std::cout << "Location fails!: " << i << " " << histogram[i] << "\n";
			std::cout << "Expected @0: " << histogram[0] << std::endl;
		}
	}

	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
