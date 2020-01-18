#ifndef FIXED_ARRAY_H
#define FIXED_ARRAY_H

#include <core/error_macros.h>

// TODO Could use std::array, but due to how Godot compiles,
// I couldn't find a way to enable boundary checks without failing to link my module with the rest of Godot...
template <typename T, unsigned int N>
class FixedArray {
public:
	inline FixedArray() {}

	inline FixedArray(T defval) {
		fill(defval);
	}

	inline void fill(T v) {
		for (unsigned int i = 0; i < N; ++i) {
			_data[i] = v;
		}
	}

	inline T &operator[](unsigned int i) {
#ifdef TOOLS_ENABLED
		CRASH_COND(i >= N);
#endif
		return _data[i];
	}

	inline const T &operator[](unsigned int i) const {
#ifdef TOOLS_ENABLED
		CRASH_COND(i >= N);
#endif
		return _data[i];
	}

	inline T *data() {
		return _data;
	}

	inline unsigned int size() const {
		return N;
	}

private:
	T _data[N];
};

#endif // FIXED_ARRAY_H
