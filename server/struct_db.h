#ifndef VOXEL_STRUCT_DB_H
#define VOXEL_STRUCT_DB_H

#include <core/error_macros.h>
#include <vector>

// Stores uniquely-identified structs in a packed array.
// Always use the IDs if you want to store a reference somewhere. Addresses aren't stable.
// IDs are made unique with a generation system.
template <typename T>
class StructDB {
private:
	struct Slot {
		T data;
		uint16_t version = 0;
		bool valid = false;
	};

public:
	static const uint16_t MAX_INDEX = 0xffff;
	static const uint16_t MAX_VERSION = 0xffff;

	static uint16_t get_index(uint32_t id) {
		return id & 0xffff;
	}

	static uint16_t get_version(uint32_t id) {
		return (id >> 16) & 0xffff;
	}

	static uint32_t make_id(uint16_t index, uint16_t version) {
		return index | (version << 16);
	}

	inline T &get(uint32_t id) {
		const uint16_t i = get_index(id);
		CRASH_COND(i >= _slots.size());
		const uint16_t v = get_version(id);
		Slot &s = _slots[i];
		CRASH_COND(s.version != v);
		CRASH_COND(s.valid == false);
		return s.data;
	}

	inline const T &get(uint32_t id) const {
		const uint16_t i = get_index(id);
		CRASH_COND(i >= _slots.size());
		const uint16_t v = get_version(id);
		const Slot &s = _slots[i];
		CRASH_COND(s.version != v);
		CRASH_COND(s.valid == false);
		return s.data;
	}

	inline T *try_get(uint32_t id) {
		const uint16_t i = get_index(id);
		if (i >= _slots.size()) {
			return nullptr;
		}
		const uint16_t v = get_version(id);
		Slot &s = _slots[i];
		if (s.version != v || s.valid == false) {
			return nullptr;
		}
		return &s.data;
	}

	inline const T *try_get(uint32_t id) const {
		const uint16_t i = get_index(id);
		if (i >= _slots.size()) {
			return nullptr;
		}
		const uint16_t v = get_version(id);
		const Slot &s = _slots[i];
		if (s.version != v) {
			return nullptr;
		}
		return &s.data;
	}

	inline uint32_t create(T data) {
		uint16_t i = find_free_index();
		uint16_t v;
		if (i == _slots.size()) {
			CRASH_COND(i == MAX_INDEX);
			v = 0;
			Slot s;
			s.data = data;
			s.valid = true;
			s.version = v;
			_slots.push_back(s);
		} else {
			Slot &s = _slots[i];
			CRASH_COND(s.version == MAX_VERSION);
			s.data = data;
			s.valid = true;
			v = s.version + 1;
			s.version = v;
		}
		return make_id(i, v);
	}

	inline void destroy(uint32_t id) {
		const uint16_t i = get_index(id);
		CRASH_COND(i >= _slots.size());
		Slot &s = _slots[i];
		CRASH_COND(s.valid == false);
		s.data = T();
		s.valid = false;
	}

	inline bool is_valid(uint32_t id) const {
		const uint16_t i = get_index(id);
		if (i >= _slots.size()) {
			return false;
		}
		const Slot &s = _slots[i];
		return s.valid && s.version == get_version(id);
	}

	inline uint32_t count() const {
		uint32_t c = 0;
		for (size_t i = 0; i < _slots.size(); ++i) {
			const Slot &s = _slots[i];
			if (s.valid) {
				++c;
			}
		}
		return c;
	}

	template <typename F>
	inline void for_each(F f) {
		for (size_t i = 0; i < _slots.size(); ++i) {
			Slot &s = _slots[i];
			if (s.valid) {
				f(s.data);
			}
		}
	}

	template <typename F>
	inline void for_each(F f) const {
		for (size_t i = 0; i < _slots.size(); ++i) {
			const Slot &s = _slots[i];
			if (s.valid) {
				f(s.data);
			}
		}
	}

	template <typename F>
	inline void for_each_with_id(F f) {
		for (size_t i = 0; i < _slots.size(); ++i) {
			Slot &s = _slots[i];
			if (s.valid) {
				f(s.data, make_id(i, s.version));
			}
		}
	}

	template <typename F>
	inline void for_each_with_id(F f) const {
		for (size_t i = 0; i < _slots.size(); ++i) {
			const Slot &s = _slots[i];
			if (s.valid) {
				f(s.data, make_id(i, s.version));
			}
		}
	}

	inline void clear() {
		_slots.clear();
	}

private:
	inline size_t find_free_index() const {
		for (size_t i = 0; i < _slots.size(); ++i) {
			if (_slots[i].valid == false) {
				return i;
			}
		}
		return _slots.size();
	}

	std::vector<Slot> _slots;
};

#endif // VOXEL_STRUCT_DB_H
