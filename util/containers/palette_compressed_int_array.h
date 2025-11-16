#ifndef ZN_PALETTE_COMPRESSED_INT_ARRAY_H
#define ZN_PALETTE_COMPRESSED_INT_ARRAY_H

#include "bit_packed_int_array.h"
#include "span.h"
#include "std_vector.h"
#include <utility>

namespace zylann {

// Stores 32-bit unsigned integers as palette indices to save space, at the cost of slower access
class PaletteCompressedIntArray {
public:
	// Empty construct
	PaletteCompressedIntArray() : _data(0, 0), _palette() {}
	// Move construct
	PaletteCompressedIntArray(PaletteCompressedIntArray &&other) : _data(other._data), _palette(other._palette) {}
	// Copy construct
	PaletteCompressedIntArray(const PaletteCompressedIntArray &other) : _data(other._data), _palette(other._palette) {}
	// Default construct
	PaletteCompressedIntArray(const uint32_t count, const uint32_t default_value) : _data(count, 0), _palette() {
		_palette.push_back(default_value);
	}

	PaletteCompressedIntArray &operator=(PaletteCompressedIntArray &&other) {
		_data = std::move(other._data);
		_palette = std::move(other._palette);
	}

	bool load(BitPackedIntArray &&data, StdVector<uint32_t> &&palette) {
		const uint32_t max_value = 1 << data.get_bits();
		ZN_ASSERT_RETURN_V(palette.size() < max_value, false);
		_data = std::move(data);
		_palette = std::move(palette);
		return true;
	}

	template <typename T>
	void load(Span<const T> src) {
		PaletteCompressedIntArray dst(src.size());
		unsigned int i = 0;
		for (const T v : src) {
			dst.set(i, v);
			++i;
		}
		*this = std::move(dst);
	}

	void fill(const uint32_t v) {
		_data.resize_element_bits(0);
		_data.fill_zero();
		_palette.clear();
		_palette.push_back(v);
	}

	bool is_uniform() const {
		// Testing uniformity of an empty buffer has no meaningful answer
		ZN_ASSERT_RETURN_V(size() > 0, false);

		if (_palette.size() == 1) {
			return true;
		}

		const uint32_t v0 = get(0);
		for (unsigned int i = 1; i < size(); ++i) {
			if (get(i) != v0) {
				return false;
			}
		}

		return true;
	}

	uint32_t size() const {
		return _data.size();
	}

	const BitPackedIntArray &data() const {
		return _data;
	}

	Span<const uint32_t> get_palette() const {
		return to_span(_palette);
	}

	void set(const unsigned int i, const uint32_t v) {
		uint32_t pi;
		if (get_palette_index(v, pi)) {
			if (_palette.size() == 1) {
				return;
			}
		} else {
			pi = _palette.size();
			_palette.push_back(v);
			const uint32_t required_bits = math::get_required_bits_u32(_palette.size());
			_data.resize_element_bits(required_bits);
		}
		_data.set(i, pi);
	}

	uint32_t get(const unsigned int i) const {
		const uint32_t pi = _data.get(i);
		return _palette[pi];
	}

	void fill_range(const unsigned int begin, const unsigned int end, const uint32_t v) {
		uint32_t pi;
		if (get_palette_index(v, pi)) {
			if (_palette.size() == 1) {
				return;
			}
		} else {
			pi = _palette.size();
			_palette.push_back(v);
			const uint32_t required_bits = math::get_required_bits_u32(_palette.size());
			_data.resize_element_bits(required_bits);
		}
		for (unsigned int i = begin; i < end; ++i) {
			_data.set(i, pi);
		}
	}

	// Asserts that `pi` is a palette index and sets it into the array
	void set_raw(const unsigned int i, const unsigned int pi) {
#ifdef DEV_ENABLED
		ZN_ASSERT(pi < _palette.size());
#endif
		_data.set(i, pi);
	}

	bool palette_contains(const uint32_t v) const {
		for (const uint32_t pv : _palette) {
			if (pv == v) {
				return true;
			}
		}
		return false;
	}

	bool get_palette_index(const uint32_t v, uint32_t &out) const {
		unsigned int i = 0;
		for (const uint32_t pv : _palette) {
			if (pv == v) {
				out = i;
				return true;
			}
			++i;
		}
		return false;
	}

	// Gets the palette index for value `v`. If it is not in the palette, a new entry is created. The data array will be
	// grown with more bits if necessary.
	uint32_t get_or_create_palette_index(const uint32_t v) {
		uint32_t pi;
		if (!get_palette_index(v, pi)) {
			pi = _palette.size();
			_palette.push_back(v);
			const uint32_t required_bits = math::get_required_bits_u32(_palette.size());
			_data.resize_element_bits(required_bits);
		}
		return pi;
	}

	// Inserts new values in the palette and increases data bits if necessary
	void combine_palette(const Span<const uint32_t> values) {
		for (const uint32_t v : values) {
			if (!palette_contains(v)) {
				_palette.push_back(v);
			}
		}
		const uint32_t required_bits = math::get_required_bits_u32(_palette.size());
		_data.resize_element_bits(required_bits);
	}

	// Removes unused entries in the palette and reduces data bits if necessary
	void compact_palette() {
		// TODO Candidate for temp allocator
		StdVector<uint32_t> occupancy;
		occupancy.resize(_palette.size(), 0);
		for (unsigned int i = 0; i < _data.size(); ++i) {
			const uint32_t pi = _data.get(i);
			occupancy[pi] += 1;
		}

		bool need_compact = false;
		for (const uint32_t c : occupancy) {
			if (c == 0) {
				need_compact = true;
				break;
			}
		}
		if (!need_compact) {
			return;
		}

		// Make new palette and lookup table to convert old indices to new ones
		StdVector<uint32_t> remap;
		StdVector<uint32_t> new_palette;
		remap.resize(_palette.size());
		for (unsigned int pi = 0; pi < occupancy.size(); ++pi) {
			if (occupancy[pi] > 0) {
				remap[pi] = new_palette.size();
				const uint32_t v = _palette[pi];
				new_palette.push_back(v);
			}
		}

		const uint32_t required_bits = math::get_next_power_of_two_32(new_palette.size());
		BitPackedIntArray new_data(_data.size(), required_bits);
		for (unsigned int i = 0; i < _data.size(); ++i) {
			const uint32_t src_pi = _data.get(i);
			const uint32_t dst_pi = remap[src_pi];
			new_data.set(i, dst_pi);
		}

		_data = std::move(new_data);
		_palette = std::move(new_palette);
	}

private:
	BitPackedIntArray _data;
	StdVector<uint32_t> _palette;
};

} // namespace zylann

#endif // ZN_PALETTE_COMPRESSED_INT_ARRAY_H
