#ifndef ZPROFILING_RECEIVE_BUFFER_H
#define ZPROFILING_RECEIVE_BUFFER_H

#include <core/ustring.h>
#include <vector>

class ZProfilingReceiveBuffer {
public:
	inline void resize(uint32_t size) {
		_data.resize(size);
	}

	inline uint8_t *data() {
		return _data.data();
	}

	inline uint8_t get_u8() {
		CRASH_COND(_position >= _data.size());
		return _data[_position++];
	}

	template <typename T>
	inline T get_t() {
		const size_t pos = _position;
		_position += sizeof(T);
		CRASH_COND(_position > _data.size());
		return *(T *)&_data[pos];
	}

	inline uint16_t get_u16() {
		return get_t<uint16_t>();
	}

	inline uint32_t get_u32() {
		return get_t<uint32_t>();
	}

	inline void get_data(uint8_t *dst, uint32_t size) {
		const size_t pos = _position;
		_position += size;
		CRASH_COND(_position > _data.size());
		memcpy(dst, _data.data() + pos, size);
	}

	inline void clear() {
		_data.clear();
		_position = 0;
	}

	inline bool is_end() const {
		return _position >= _data.size();
	}

	inline size_t size() const {
		return _data.size();
	}

private:
	// Capacity of this vector matters a lot for performance.
	std::vector<uint8_t> _data;
	size_t _position = 0;
};

#endif // ZPROFILING_RECEIVE_BUFFER_H
