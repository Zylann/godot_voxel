#ifndef ZPROFILING_SEND_BUFFER_H
#define ZPROFILING_SEND_BUFFER_H

#include <core/ustring.h>
#include <vector>

// Serialization helper with re-usable memory
class ZProfilingSendBuffer {
public:
	inline void put_u8(uint8_t v) {
		_data.push_back(v);
	}

	template <typename T>
	inline void put_t(T v) {
		const size_t a = _data.size();
		_data.resize(_data.size() + sizeof(T));
		*(T *)(&_data[a]) = v;
	}

	inline void put_u16(uint16_t v) {
		put_t<uint16_t>(v);
	}

	inline void put_u32(uint32_t v) {
		put_t<uint32_t>(v);
	}

	inline void put_data(const uint8_t *bytes, uint32_t size) {
		const size_t a = _data.size();
		_data.resize(_data.size() + size);
		memcpy(&_data[a], bytes, size);
	}

	inline void put_utf8_string(const String str) {
		const CharString cs = str.utf8();
		put_u32(cs.length());
		put_data((const uint8_t *)cs.get_data(), cs.length());
	}

	inline size_t size() const {
		return _data.size();
	}

	inline const uint8_t *data() const {
		return _data.data();
	}

	inline void clear() {
		_data.clear();
	}

private:
	std::vector<uint8_t> _data;
};

#endif // ZPROFILING_SEND_BUFFER_H
