#ifndef VOXEL_UTIL_SERIALIZATION_H
#define VOXEL_UTIL_SERIALIZATION_H

#include "span.h"
#include <cstring>

namespace zylann {

enum Endianess { //
	ENDIANESS_BIG_ENDIAN,
	ENDIANESS_LITTLE_ENDIAN
};

inline Endianess get_platform_endianess() {
	// https://stackoverflow.com/questions/4181951/how-to-check-whether-a-system-is-big-endian-or-little-endian#4181991
	const int n = 1;
	if (*(char *)&n == 1) {
		return ENDIANESS_LITTLE_ENDIAN;
	}
	return ENDIANESS_BIG_ENDIAN;
	// TODO In C++20 we'll be able to use std::endian
}

template <typename Container_T>
struct MemoryWriterTemplate {
	Container_T &data;
	// Using network-order by default
	// TODO Apparently big-endian is dead
	// I chose it originally to match "network byte order",
	// but as I read comments about it there seem to be no reason to continue using it. Needs a version increment.
	Endianess endianess = ENDIANESS_BIG_ENDIAN;

	MemoryWriterTemplate(Container_T &p_data, Endianess p_endianess) : data(p_data), endianess(p_endianess) {}

	inline void store_8(uint8_t v) {
		data.push_back(v);
	}

	inline void store_16(uint16_t v) {
		if (endianess == ENDIANESS_BIG_ENDIAN) {
			data.push_back(v >> 8);
			data.push_back(v & 0xff);
		} else {
			data.push_back(v & 0xff);
			data.push_back(v >> 8);
		}
	}

	inline void store_32(uint32_t v) {
		if (endianess == ENDIANESS_BIG_ENDIAN) {
			data.push_back(v >> 24);
			data.push_back(v >> 16);
			data.push_back(v >> 8);
			data.push_back(v & 0xff);
		} else {
			data.push_back(v & 0xff);
			data.push_back(v >> 8);
			data.push_back(v >> 16);
			data.push_back(v >> 24);
		}
	}

	inline void store_64(uint64_t v) {
		if (endianess == ENDIANESS_BIG_ENDIAN) {
			data.push_back(v >> 56);
			data.push_back(v >> 48);
			data.push_back(v >> 40);
			data.push_back(v >> 32);
			data.push_back(v >> 24);
			data.push_back(v >> 16);
			data.push_back(v >> 8);
			data.push_back(v & 0xff);
		} else {
			data.push_back(v & 0xff);
			data.push_back(v >> 8);
			data.push_back(v >> 16);
			data.push_back(v >> 24);
			data.push_back(v >> 32);
			data.push_back(v >> 40);
			data.push_back(v >> 48);
			data.push_back(v >> 56);
		}
	}

	inline void store_float(float v) {
		union M {
			uint32_t i;
			float f;
		} m;
		m.f = v;
		store_32(m.i);
	}

	inline void store_buffer(Span<const uint8_t> p_data) {
		const size_t begin = data.size();
		data.resize(data.size() + p_data.size());
		memcpy(&data[begin], p_data.data(), p_data.size());
	}
};

// Default
typedef MemoryWriterTemplate<std::vector<uint8_t>> MemoryWriter;

struct ByteSpanWithPosition {
	Span<uint8_t> data;
	size_t pos = 0;

	ByteSpanWithPosition(Span<uint8_t> p_data, size_t initial_pos) : data(p_data), pos(initial_pos) {}

	inline void push_back(uint8_t v) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(pos != data.size());
#endif
		data[pos++] = v;
	}

	inline size_t size() const {
		return pos;
	}

	inline void resize(size_t new_size) {
		ZN_ASSERT(new_size <= data.size());
		pos = new_size;
	}

	inline uint8_t &operator[](size_t i) {
		return data[i];
	}
};

typedef MemoryWriterTemplate<ByteSpanWithPosition> MemoryWriterExistingBuffer;

struct MemoryReader {
	Span<const uint8_t> data;
	size_t pos = 0;
	// Using network-order by default
	Endianess endianess = ENDIANESS_BIG_ENDIAN;

	MemoryReader(Span<const uint8_t> p_data, Endianess p_endianess) : data(p_data), endianess(p_endianess) {}

	inline uint8_t get_8() {
		// ERR_FAIL_COND_V(pos >= data.size(), 0);
		return data[pos++];
	}

	inline uint16_t get_16() {
		// ERR_FAIL_COND_V(pos + 1 >= data.size(), 0);
		uint16_t v;
		if (endianess == ENDIANESS_BIG_ENDIAN) {
			v = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
		} else {
			v = (static_cast<uint16_t>(data[pos]) | static_cast<uint16_t>(data[pos + 1]) << 8);
		}
		pos += sizeof(uint16_t);
		return v;
	}

	inline uint32_t get_32() {
		// ERR_FAIL_COND_V(pos + 3 >= data.size(), 0);
		uint32_t v;
		if (endianess == ENDIANESS_BIG_ENDIAN) {
			v = //
					(static_cast<uint32_t>(data[pos]) << 24) | //
					(static_cast<uint32_t>(data[pos + 1]) << 16) | //
					(static_cast<uint32_t>(data[pos + 2]) << 8) | data[pos + 3];
		} else {
			v = //
					static_cast<uint32_t>(data[pos]) | //
					(static_cast<uint32_t>(data[pos + 1]) << 8) | //
					(static_cast<uint32_t>(data[pos + 2]) << 16) | (static_cast<uint32_t>(data[pos + 3]) << 24);
		}
		pos += sizeof(uint32_t);
		return v;
	}

	inline uint64_t get_64() {
		// ERR_FAIL_COND_V(pos + 3 >= data.size(), 0);
		uint64_t v;
		if (endianess == ENDIANESS_BIG_ENDIAN) {
			v = //
					(static_cast<uint64_t>(data[pos]) << 56) | //
					(static_cast<uint64_t>(data[pos + 1]) << 48) | //
					(static_cast<uint64_t>(data[pos + 2]) << 40) | //
					(static_cast<uint64_t>(data[pos + 3]) << 32) | //
					(static_cast<uint64_t>(data[pos + 4]) << 24) | //
					(static_cast<uint64_t>(data[pos + 5]) << 16) | //
					(static_cast<uint64_t>(data[pos + 6]) << 8) | //
					data[pos + 7];
		} else {
			v = //
					static_cast<uint64_t>(data[pos]) | //
					(static_cast<uint64_t>(data[pos + 1]) << 8) | //
					(static_cast<uint64_t>(data[pos + 2]) << 16) | //
					(static_cast<uint64_t>(data[pos + 3]) << 24) | //
					(static_cast<uint64_t>(data[pos + 4]) << 32) | //
					(static_cast<uint64_t>(data[pos + 5]) << 40) | //
					(static_cast<uint64_t>(data[pos + 6]) << 48) | //
					(static_cast<uint64_t>(data[pos + 7]) << 56);
		}
		pos += sizeof(uint64_t);
		return v;
	}

	inline float get_float() {
		union M {
			uint32_t i;
			float f;
		} m;
		m.i = get_32();
		return m.f;
	}

	inline size_t get_buffer(Span<uint8_t> p_dst) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(pos <= data.size());
#endif
		size_t end = pos + p_dst.size();
		if (end > data.size()) {
			end = data.size();
		}
		const size_t len = end - pos;
		memcpy(p_dst.data(), &data[pos], len);
		pos += len;
		return len;
	}

	// For API compatibility
	inline size_t get_position() const {
		return pos;
	}
};

} // namespace zylann

#endif // VOXEL_UTIL_SERIALIZATION_H
