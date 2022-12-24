#ifndef ZN_SLOT_MAP_H
#define ZN_SLOT_MAP_H

#include "errors.h"
#include <cstdint>
#include <limits>
#include <vector>

namespace zylann {

// Version part of a SlotMap key. The last bit is used to represent the version of an unused slot.
template <typename TVersion>
struct SlotMapVersion {
	static const uint32_t UNUSED_BIT = 1 << (8 * sizeof(TVersion) - 1);
	static const uint32_t MASK = UNUSED_BIT - 1;
	static const uint32_t MAX_VALUE = MASK;

	TVersion value = 0;

	inline bool operator==(const SlotMapVersion &other) const {
		return value == other.value;
	}

	inline bool operator!=(const SlotMapVersion &other) const {
		return value != other.value;
	}

	inline bool is_valid() const {
		return (value & UNUSED_BIT) == 0;
	}

	inline bool is_invalid() const {
		return (value & UNUSED_BIT) != 0;
	}

	inline void make_valid() {
		value &= MASK;
	}

	inline void make_invalid() {
		ZN_ASSERT(is_valid());
#if DEBUG_ENABLED
		if (value == MAX_VALUE) {
			ZN_PRINT_WARNING("SlotMapVersion overflow");
		}
#endif
		value = (value + 1) | UNUSED_BIT;
	}
};

// Combination of an index and a version, identifying a specific occupancy of a slot.
template <typename TIndex, typename TVersion>
struct SlotMapKey {
	// Index in the array of slots
	TIndex index = 0;
	// Version the slot must have in order to match the key
	SlotMapVersion<TVersion> version;

	inline bool operator==(const SlotMapKey &other) const {
		return index == other.index && version == other.version;
	}

	inline bool operator!=(const SlotMapKey &other) const {
		return index != other.index || version != other.version;
	}
};

// Stores uniquely-identified values in an O(1) access container.
// Always use the IDs if you want to store a reference somewhere. The address of values aren't stable.
// IDs are made unique with a generation system, they are never re-used.
// The concept is similar to https://docs.rs/slotmap/latest/slotmap/
template <typename T, typename TIndex = uint32_t, typename TVersion = uint32_t>
class SlotMap {
private:
	struct Slot {
		T value;
		SlotMapVersion<TVersion> version;
	};

public:
	typedef SlotMapKey<TIndex, TVersion> Key;

	Key add(T value) {
		if (_free_list.size() > 0) {
			const TIndex i = _free_list.back();
			_free_list.pop_back();
			Slot &slot = _slots[i];
			slot.value = value;
			slot.version.make_valid();
			++_count;
			return Key{ i, slot.version };
		} else {
			const TIndex i = _slots.size();
			// Start versions at 1, so we can represent 0 as invalid
			const uint32_t v = 1;
			_slots.push_back(Slot{ value, v });
			++_count;
			return Key{ i, v };
		}
	}

	T *try_get(Key key) {
		ZN_ASSERT_RETURN_V(key.version.is_valid(), nullptr);
		if (key.index >= _slots.size()) {
			return nullptr;
		}
		Slot &slot = _slots[key.index];
		if (slot.version != key.version) {
			return nullptr;
		}
		return &slot.value;
	}

	const T *try_get(Key key) const {
		ZN_ASSERT_RETURN_V(key.version.is_valid(), nullptr);
		if (key.index >= _slots.size()) {
			return nullptr;
		}
		const Slot &slot = _slots[key.index];
		if (slot.version != key.version) {
			return nullptr;
		}
		return &slot.value;
	}

	bool exists(Key key) const {
		ZN_ASSERT_RETURN_V(key.version.is_valid(), false);
		if (key.index >= _slots.size()) {
			return false;
		}
		const Slot &slot = _slots[key.index];
		if (slot.version != key.version) {
			return false;
		}
		return true;
	}

	bool try_remove(Key key) {
		ZN_ASSERT_RETURN_V(key.version.is_valid(), false);
		if (key.index >= _slots.size()) {
			return false;
		}
		Slot &slot = _slots[key.index];
		if (slot.version != key.version) {
			return false;
		}
		slot.value = T();
		slot.version.make_invalid();
		ZN_ASSERT(_count > 0);
		--_count;
		_free_list.push_back(key.index);
		return true;
	}

	T &get(Key key) {
		T *v = try_get(key);
		ZN_ASSERT(v != nullptr);
		return *v;
	}

	const T &get(Key key) const {
		const T *v = try_get(key);
		ZN_ASSERT(v != nullptr);
		return *v;
	}

	void remove(Key key) {
		ZN_ASSERT(try_remove(key));
	}

	void clear() {
		_slots.clear();
		_free_list.clear();
		_count = 0;
	}

	uint32_t count() const {
		return _count;
	}

	template <typename F>
	void for_each_value(F f) {
		for (Slot &slot : _slots) {
			if (slot.version.is_valid()) {
				f(slot.value);
			}
		}
	}

	template <typename F>
	void for_each_value(F f) const {
		for (const Slot &slot : _slots) {
			if (slot.version.is_valid()) {
				f(slot.value);
			}
		}
	}

	template <typename F>
	void for_each_key_value(F f) {
		TIndex i = 0;
		for (Slot &slot : _slots) {
			if (slot.version.is_valid()) {
				f(Key{ i, slot.version }, slot.value);
			}
			++i;
		}
	}

	template <typename F>
	void for_each_key_value(F f) const {
		TIndex i = 0;
		for (const Slot &slot : _slots) {
			if (slot.version.is_valid()) {
				f(Key{ i, slot.version }, slot.value);
			}
			++i;
		}
	}

private:
	std::vector<Slot> _slots;
	// TODO The free list could be implemented more efficiently without a vector, but would create some complexity.
	// If each value in slots is a union of a TIndex and a T, invalid slots could interpret the value as a TIndex to the
	// next free slot. However putting T in a union isn't possible when T has a non-trivial constructor/destructor.
	std::vector<TIndex> _free_list;
	uint32_t _count = 0;
};

} // namespace zylann

#endif // ZN_SLOT_MAP_H
