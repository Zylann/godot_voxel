#ifndef ZN_DYNAMIC_BITSET_H
#define ZN_DYNAMIC_BITSET_H

#include "../errors.h"
#include "std_vector.h"
#include <cstdint>

namespace zylann {

// STL's bitset is fixed size, and I don't want to depend on Boost
class DynamicBitset {
public:
	inline unsigned int size() const {
		return _size;
	}

	inline void resize_no_init(unsigned int size) {
		// non-initializing resize (no guaranteed values)
		_bits.resize((size + 63) / 64);
		_size = size;
	}

	void clear() {
		_bits.clear();
		_size = 0;
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
		ZN_ASSERT(i < _size);
#endif
		return _bits[i >> 6] & (uint64_t(1) << (i & uint64_t(63)));
	}

	inline void set(uint64_t i) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < _size);
#endif
		_bits[i >> 6] |= uint64_t(1) << (i & uint64_t(63));
	}

	inline void unset(uint64_t i) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(i < _size);
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
	StdVector<uint64_t> _bits;
	unsigned int _size = 0;
};

} // namespace zylann

#endif // ZN_DYNAMIC_BITSET_H
