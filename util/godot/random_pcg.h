/*************************************************************************/
/*  random_pcg.h                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
// Zylann: added the `godot` namespace, adapted so it can compile with GodotCpp

#ifndef ZN_RANDOM_PCG_H
#define ZN_RANDOM_PCG_H

#ifdef ZN_GODOT

// Use built-in version
#include <core/math/random_pcg.h>

#else

#include "../macros.h"
#include "../math/constants.h"
#include "pcg.h"
#include <math.h>

#if defined(__GNUC__)
#define CLZ32(x) __builtin_clz(x)
#elif defined(_MSC_VER)
#include <intrin.h>
static int __bsr_clz32(uint32_t x) {
	unsigned long index;
	_BitScanReverse(&index, x);
	return 31 - index;
}
#define CLZ32(x) __bsr_clz32(x)
#else
#endif

#if defined(__GNUC__)
#define LDEXP(s, e) __builtin_ldexp(s, e)
#define LDEXPF(s, e) __builtin_ldexpf(s, e)
#else
#include <math.h>
#define LDEXP(s, e) ldexp(s, e)
#define LDEXPF(s, e) ldexp(s, e)
#endif

namespace godot {

class RandomPCG {
	pcg32_random_t pcg;
	uint64_t current_seed = 0; // The seed the current generator state started from.
	uint64_t current_inc = 0;

public:
	static const uint64_t DEFAULT_SEED = 12047754176567800795U;
	static const uint64_t DEFAULT_INC = PCG_DEFAULT_INC_64;

	RandomPCG(uint64_t p_seed = DEFAULT_SEED, uint64_t p_inc = DEFAULT_INC);

	inline void seed(uint64_t p_seed) {
		current_seed = p_seed;
		pcg32_srandom_r(&pcg, current_seed, current_inc);
	}
	inline uint64_t get_seed() {
		return current_seed;
	}

	inline void set_state(uint64_t p_state) {
		pcg.state = p_state;
	}
	inline uint64_t get_state() const {
		return pcg.state;
	}

	void randomize();
	inline uint32_t rand() {
		return pcg32_random_r(&pcg);
	}
	inline uint32_t rand(uint32_t bounds) {
		return pcg32_boundedrand_r(&pcg, bounds);
	}

	// Obtaining floating point numbers in [0, 1] range with "good enough" uniformity.
	// These functions sample the output of rand() as the fraction part of an infinite binary number,
	// with some tricks applied to reduce ops and branching:
	// 1. Instead of shifting to the first 1 and connecting random bits, we simply set the MSB and LSB to 1.
	//    Provided that the RNG is actually uniform bit by bit, this should have the exact same effect.
	// 2. In order to compensate for exponent info loss, we count zeros from another random number,
	//    and just add that to the initial offset.
	//    This has the same probability as counting and shifting an actual bit stream: 2^-n for n zeroes.
	// For all numbers above 2^-96 (2^-64 for floats), the functions should be uniform.
	// However, all numbers below that threshold are floored to 0.
	// The thresholds are chosen to minimize rand() calls while keeping the numbers within a totally subjective quality
	// standard. If clz or ldexp isn't available, fall back to bit truncation for performance, sacrificing uniformity.
	inline double randd() {
#if defined(CLZ32)
		uint32_t proto_exp_offset = rand();
		if (ZN_UNLIKELY(proto_exp_offset == 0)) {
			return 0;
		}
		uint64_t significand = (((uint64_t)rand()) << 32) | rand() | 0x8000000000000001U;
		return LDEXP((double)significand, -64 - CLZ32(proto_exp_offset));
#else
#pragma message("RandomPCG::randd - intrinsic clz is not available, falling back to bit truncation")
		return (double)(((((uint64_t)rand()) << 32) | rand()) & 0x1FFFFFFFFFFFFFU) / (double)0x1FFFFFFFFFFFFFU;
#endif
	}
	inline float randf() {
#if defined(CLZ32)
		uint32_t proto_exp_offset = rand();
		if (ZN_UNLIKELY(proto_exp_offset == 0)) {
			return 0;
		}
		return LDEXPF((float)(rand() | 0x80000001), -32 - CLZ32(proto_exp_offset));
#else
#pragma message("RandomPCG::randf - intrinsic clz is not available, falling back to bit truncation")
		return (float)(rand() & 0xFFFFFF) / (float)0xFFFFFF;
#endif
	}

	inline double randfn(double p_mean, double p_deviation) {
		return p_mean +
				p_deviation * (cos(zylann::math::TAU_64 * randd()) * sqrt(-2.0 * log(randd()))); // Box-Muller transform
	}
	inline float randfn(float p_mean, float p_deviation) {
		return p_mean +
				p_deviation * (cos(zylann::math::TAU_32 * randf()) * sqrt(-2.0 * log(randf()))); // Box-Muller transform
	}

	double random(double p_from, double p_to);
	float random(float p_from, float p_to);
	int random(int p_from, int p_to);
};

} // namespace godot

#endif // ZN_GODOT
#endif // RANDOM_PCG_H
