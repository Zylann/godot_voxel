#ifndef INTERVAL_H
#define INTERVAL_H

#include "../util/utility.h"

// For interval arithmetic
struct Interval {

	// Both inclusive
	float min;
	float max;

	inline Interval() :
			min(0),
			max(0) {}

	inline Interval(float p_min, float p_max) :
			min(p_min),
			max(p_max) {
#if DEBUG_ENABLED
		CRASH_COND(p_min > p_max);
#endif
	}

	inline Interval(const Interval &other) :
			min(other.min),
			max(other.max) {}

	inline static Interval from_single_value(float p_val) {
		return Interval(p_val, p_val);
	}

	inline bool contains(float v) const {
		return v >= min && v <= max;
	}

	inline bool is_single_value() const {
		return min == max;
	}

	inline void add_point(float x) {
		if (x < min) {
			min = x;
		} else if (x > max) {
			max = x;
		}
	}

	inline Interval operator+(float x) const {
		return Interval{ min + x, max + x };
	}

	inline Interval operator+(const Interval &other) const {
		return Interval{ min + other.min, max + other.max };
	}

	inline Interval operator-(float x) const {
		return Interval{ min - x, max - x };
	}

	inline Interval operator-(const Interval &other) const {
		return Interval{ min - other.max, max - other.min };
	}

	inline Interval operator*(float x) const {
		const float a = min * x;
		const float b = max * x;
		if (a < b) {
			return Interval(a, b);
		} else {
			return Interval(b, a);
		}
	}

	inline Interval operator*(const Interval &other) const {
		const float a = min * other.min;
		const float b = min * other.max;
		const float c = max * other.min;
		const float d = max * other.max;
		return Interval{ ::min(a, b, c, d), ::max(a, b, c, d) };
	}

	inline Interval abs() const {
		return Interval{
			contains(0) ? 0 : ::min(Math::abs(min), Math::abs(max)),
			::max(Math::abs(min), Math::abs(max))
		};
	}

	inline Interval sqrt() const {
		return Interval{
			Math::sqrt(::max(0.f, min)),
			Math::sqrt(::max(0.f, max))
		};
	}

	inline Interval clamp(const Interval &p_min, const Interval &p_max) const {
		if (p_min.is_single_value() && p_max.is_single_value()) {
			return {
				::clamp(min, p_min.min, p_max.min),
				::clamp(max, p_min.min, p_max.min)
			};
		}
		if (min >= p_min.max && max <= p_max.min) {
			return *this;
		}
		if (min >= p_max.max) {
			return Interval::from_single_value(p_max.max);
		}
		if (max <= p_min.min) {
			return Interval::from_single_value(p_min.min);
		}
		return Interval(p_min.min, p_max.max);
	}
};

#endif // INTERVAL_H
