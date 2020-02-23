#ifndef DYNAMIC_BITSET_H
#define DYNAMIC_BITSET_H

#include <core/error_macros.h>
#include <vector>

// STL's bitset is fixed size, and I don't want to depend on Boost
class DynamicBitset {
public:
	inline unsigned int size() const {
		return _size;
	}

	inline void resize(unsigned int size) {
		// non-initializing resize
		_bits.resize((size + 63) / 64);
		_size = size;
	}

	void fill(bool v) {
		// Note: padding bits will also be set
		uint64_t m = v ? 0xffffffffffffffff : 0;
		for (auto it = _bits.begin(); it != _bits.end(); ++it) {
			*it = m;
		}
	}

	inline bool get(uint64_t i) const {
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _size);
#endif
		return _bits[i >> 6] & (uint64_t(1) << (i & uint64_t(63)));
	}

	inline void set(uint64_t i) {
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _size);
#endif
		_bits[i >> 6] |= uint64_t(1) << (i & uint64_t(63));
	}

	inline void unset(uint64_t i) {
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _size);
#endif
		_bits[i >> 6] &= ~(uint64_t(1) << (i & uint64_t(63)));
	}

	inline void set(uint64_t i, bool v) {
		if (v) {
			set(i);
		} else {
			unset(i);
		}
	}

private:
	std::vector<uint64_t> _bits;
	unsigned int _size = 0;
};

#endif // DYNAMIC_BITSET_H
