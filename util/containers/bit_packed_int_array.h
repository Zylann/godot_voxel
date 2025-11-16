#ifndef ZN_BIT_PACKED_INT_ARRAY_H
#define ZN_BIT_PACKED_INT_ARRAY_H

#include "../math/funcs.h"
#include "../memory/memory.h"
#include "span.h"

namespace zylann {

// Stores a runtime-known amount of up-to-32-bit unsigned integers with runtime-known number of bits.
class BitPackedIntArray {
public:
	static const unsigned int MAX_BITS = sizeof(uint32_t) * 8;

	BitPackedIntArray(const uint32_t count, const uint32_t bits) {
		ZN_ASSERT(MAX_BITS <= 32);
		if (count > 0 && bits > 0) {
			// Pad by an extra u32 to ensure we don't go OOB when looking up last elements
			const size_t u32_count = math::ceildiv_st(count * bits, 32) + 1;
			_data = static_cast<uint32_t *>(ZN_ALLOC(u32_count * sizeof(uint32_t)));
			_data_len = u32_count;
		}
		_count = count;
		_bits = bits;
		fill_zero();
	}

	BitPackedIntArray(const BitPackedIntArray &other) {
		_data = static_cast<uint32_t *>(ZN_ALLOC(other._data_len * sizeof(uint32_t)));
		_data_len = other._data_len;
		memcpy(_data, other._data, _data_len * sizeof(uint32_t));
		_count = other._count;
		_bits = other._bits;
	}

	BitPackedIntArray(BitPackedIntArray &&other) {
		_data = other._data;
		_data_len = other._data_len;
		_count = other._count;
		_bits = other._bits;

		other._data = nullptr;
		other._data_len = 0;
		other._count = 0;
	}

	~BitPackedIntArray() {
		if (_data != nullptr) {
			ZN_FREE(_data);
		}
	}

	BitPackedIntArray &operator=(const BitPackedIntArray &other) {
		if (this == &other) {
			return *this;
		}

		if (_data_len != other._data_len) {
			if (_data != nullptr) {
				ZN_FREE(_data);
				_data = static_cast<uint32_t *>(ZN_ALLOC(other._data_len * sizeof(uint32_t)));
			}
			_data_len = other._data_len;
		}

		_bits = other._bits;
		_count = other._count;

		if (_data != nullptr) {
			memcpy(_data, other._data, other._data_len * sizeof(uint32_t));
		}
	}

	BitPackedIntArray &operator=(BitPackedIntArray &&other) {
		if (this == &other) {
			return *this;
		}
		if (_data != nullptr) {
			ZN_FREE(_data);
		}
		_data = other._data;
		_data_len = other._data_len;
		_bits = other._bits;
		_count = other._count;

		other._data = nullptr;
		other._data_len = 0;
		other._count = 0;

		return *this;
	}

	void fill_zero() {
		for (unsigned int i = 0; i < _data_len; ++i) {
			_data[i] = 0;
		}
	}

	uint32_t get_bits() const {
		return _bits;
	}

	uint32_t size() const {
		return _count;
	}

	Span<const uint32_t> data() const {
		return Span<const uint32_t>(_data, _data_len);
	}

	uint32_t get(unsigned int i) const {
		ZN_ASSERT(i < _count);
		if (_bits == 0) {
			return 0;
		}

		const unsigned int bit_index = i * _bits;
		const unsigned int backing_index = bit_index / (sizeof(uint32_t) * 8);
		const unsigned int bit_rel_index = bit_index & (sizeof(uint32_t) * 8 - 1);
		const unsigned int element_mask = (1 << _bits) - 1;

#ifdef DEV_ENABLED
		ZN_ASSERT(backing_index + 1 < _data_len);
#endif
		// It is possible for elements to exist across two backing ints (biggest being 31 bits)
		const uint64_t a = _data[backing_index];
		const uint64_t b = _data[backing_index + 1];
		const uint64_t c = a | (b << (sizeof(uint32_t) * 8));
		return (c >> bit_rel_index) & element_mask;

		// This assumes little-endian.
		// We reinterpret as 64-bits because if elements have more than 24 bits they can be located across 5 bytes
		// const uint64_t *ptr = reinterpret_cast<const uint64_t *>(&_data[backing_index]);
		// return (*ptr >> bit_rel_index) & element_mask;
	}

	void set(const unsigned int i, const uint32_t v) {
		ZN_ASSERT(i < _count);
		ZN_ASSERT(_bits > 0);

		const unsigned int bit_index = i * _bits;
		const unsigned int backing_index = bit_index / (sizeof(uint32_t) * 8);
		const unsigned int bit_rel_index = bit_index & (sizeof(uint32_t) * 8 - 1);
		const unsigned int element_mask = (1 << _bits) - 1;

#ifdef DEV_ENABLED
		ZN_ASSERT(backing_index + 1 < _data_len);
#endif

		const uint32_t a = _data[backing_index];
		const uint32_t a2 = (a & ~(element_mask << bit_rel_index)) | (v << bit_rel_index);
		_data[backing_index] = a2;

		if (bit_rel_index + _bits > 32) {
			// Value is across two backing ints
			const uint32_t b = _data[backing_index + 1];
			const uint32_t rem = 32 - bit_rel_index;
			const uint32_t b2 = (b & ~(element_mask >> rem)) | (v >> rem);
			_data[backing_index + 1] = b2;
		}

		// This assumes little-endian.
		// ZN_ASSERT(v < element_mask);
		// uint64_t *ptr = reinterpret_cast<uint64_t *>(&_data[backing_index]);
		// // Clear existing element
		// *ptr &= (element_mask << bit_rel_index);
		// // Write new element
		// *ptr |= v << bit_rel_index;
	}

	// Sets the number of bits per element.
	// Decreasing the number of bits can truncate existing values, if they are above the maximum value.
	void resize_element_bits(const uint32_t new_bits) {
		if (new_bits == _bits) {
			return;
		}
		BitPackedIntArray dst(_count, new_bits);
		for (unsigned int i = 0; i < _count; ++i) {
			dst.set(i, get(i));
		}
		*this = std::move(dst);
	}

	class ConstForwardIterator {
	public:
		// Start from beginning
		ConstForwardIterator(const BitPackedIntArray &p_self) :
				_self(p_self),
				_backing_int(p_self._data[0] | (static_cast<uint64_t>(p_self._data[1]) << 32)),
				_element_mask((1 << p_self._bits) - 1) {}

		// Start from an element index
		ConstForwardIterator(const BitPackedIntArray &p_self, const unsigned int from) :
				_self(p_self),
				_element_mask((1 << p_self._bits) - 1),
				_element_index(from) //
		{
			const unsigned int bit_index = from * p_self._bits;
			const unsigned int backing_index = bit_index / (sizeof(uint32_t) * 8);
			_bit_rel_index = bit_index & (sizeof(uint32_t) * 8 - 1);
#ifdef DEV_ENABLED
			ZN_ASSERT(backing_index + 1 < p_self._data_len);
#endif
			const uint64_t a = p_self._data[backing_index];
			const uint64_t b = p_self._data[backing_index + 1];
			_backing_int = a | (b << 32);
		}

		// Copy
		ConstForwardIterator(const ConstForwardIterator &p_it) :
				_self(p_it._self),
				_backing_int(p_it._backing_int),
				_element_mask(p_it._element_mask),
				_bit_rel_index(p_it._bit_rel_index) {}

		inline const uint32_t operator*() const {
#ifdef DEV_ENABLED
			ZN_ASSERT(_element_index < _self._count);
#endif
			return (_backing_int >> _bit_rel_index) & _element_mask;
		}

		inline ConstForwardIterator &operator++() {
#ifdef DEV_ENABLED
			ZN_ASSERT(_element_index < _self._count);
#endif
			_bit_rel_index += _self._bits;
			++_element_index;

			if (_bit_rel_index >= MAX_BITS) {
				// Fetch next 32 bits

				const unsigned int bit_index = _element_index * _self._bits;
				const unsigned int backing_u32_index = bit_index / (sizeof(uint32_t) * 8);
#ifdef DEV_ENABLED
				ZN_ASSERT(backing_u32_index + 1 < _self._data_len);
#endif
				const uint64_t b = _self._data[backing_u32_index + 1];
				_backing_int = (_backing_int >> 32) | (b << 32);
				_bit_rel_index -= MAX_BITS;
			}

			return *this;
		}

		inline bool operator==(const ConstForwardIterator &b) const {
			return (&_self) == (&b._self) && _element_index == b._element_index;
		}

		inline bool operator!=(const ConstForwardIterator &b) const {
			return (&_self) != (&b._self) || _element_index != b._element_index;
		}

		inline bool next() {
			// (*this)++; // What the hell?
			(*this).operator++();
			return !is_end();
		}

		inline bool is_end() const {
			return _element_index == _self.size();
		}

	private:
		const BitPackedIntArray &_self;
		uint64_t _backing_int;
		const uint32_t _element_mask;
		uint32_t _element_index = 0;
		uint8_t _bit_rel_index = 0;
	};

	ConstForwardIterator cbegin() const {
		return ConstForwardIterator(*this);
	}

	ConstForwardIterator cbegin_from(const unsigned int from) const {
		return ConstForwardIterator(*this, from);
	}

	template <typename TUInt>
	void read(Span<TUInt> dst, const unsigned int from) {
		ZN_ASSERT(from + dst.size() <= _count);
		ConstForwardIterator src_it(*this, from);
		for (unsigned int i = 0; i < dst.size(); ++i, ++src_it) {
			dst[i] = *src_it;
		}
	}

private:
	uint32_t *_data = nullptr;
	// Actual allocated number of backing ints
	size_t _data_len = 0;

	// Bits per element. Up to 31 bits are supported.
	uint32_t _bits = 0;
	uint32_t _count = 0;
};

} // namespace zylann

#endif // ZN_BIT_PACKED_INT_ARRAY_H
