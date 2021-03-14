#ifndef INTERVAL_H
#define INTERVAL_H

#include "funcs.h"
#include <limits>

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

	inline static Interval from_infinity() {
		return Interval(
				-std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
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

	inline void add_interval(Interval other) {
		add_point(other.min);
		add_point(other.max);
	}

	inline float length() const {
		return max - min;
	}

	inline Interval operator+(float x) const {
		return Interval{ min + x, max + x };
	}

	inline Interval operator+(const Interval &other) const {
		return Interval{ min + other.min, max + other.max };
	}

	inline void operator+=(const Interval &other) {
		*this = *this + other;
	}

	inline Interval operator-(float x) const {
		return Interval{ min - x, max - x };
	}

	inline Interval operator-(const Interval &other) const {
		return Interval{ min - other.max, max - other.min };
	}

	inline Interval operator-() const {
		return Interval{ -max, -min };
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

	inline void operator*=(float x) {
		*this = *this * x;
	}

	inline void operator*=(Interval x) {
		*this = *this * x;
	}

	inline Interval operator/(const Interval &other) const {
		if (other.is_single_value() && other.min == 0.f) {
			return Interval::from_single_value(0.f);
		}
		if (other.contains(0.f)) {
			// TODO May need something more precise
			return Interval::from_infinity();
		}
		const float a = min / other.min;
		const float b = min / other.max;
		const float c = max / other.min;
		const float d = max / other.max;
		return Interval{ ::min(a, b, c, d), ::max(a, b, c, d) };
	}

	inline Interval operator/(float x) const {
		// TODO Implement proper division by interval
		return *this * (1.f / x);
	}

	inline void operator/=(float x) {
		*this = *this / x;
	}
};

inline Interval operator*(float b, const Interval &a) {
	return a * b;
}

// Functions declared outside, so using intervals or numbers can be the same code (templatable)

inline Interval min_interval(const Interval &a, const Interval &b) {
	return Interval(::min(a.min, b.min), ::min(a.max, b.max));
}

inline Interval max_interval(const Interval &a, const Interval &b) {
	return Interval(::max(a.min, b.min), ::max(a.max, b.max));
}

inline Interval min_interval(const Interval &a, const float b) {
	return Interval(::min(a.min, b), ::min(a.max, b));
}

inline Interval max_interval(const Interval &a, const float b) {
	return Interval(::max(a.min, b), ::max(a.max, b));
}

inline Interval sqrt(const Interval &i) {
	return Interval{
		Math::sqrt(::max(0.f, i.min)),
		Math::sqrt(::max(0.f, i.max))
	};
}

inline Interval squared(const Interval a) {
	return a * a;
}

inline Interval abs(const Interval &i) {
	return Interval{
		i.contains(0) ? 0 : ::min(Math::abs(i.min), Math::abs(i.max)),
		::max(Math::abs(i.min), Math::abs(i.max))
	};
}

inline Interval clamp(const Interval &i, const Interval &p_min, const Interval &p_max) {
	if (p_min.is_single_value() && p_max.is_single_value()) {
		return {
			::clamp(i.min, p_min.min, p_max.min),
			::clamp(i.max, p_min.min, p_max.min)
		};
	}
	if (i.min >= p_min.max && i.max <= p_max.min) {
		return i;
	}
	if (i.min >= p_max.max) {
		return Interval::from_single_value(p_max.max);
	}
	if (i.max <= p_min.min) {
		return Interval::from_single_value(p_min.min);
	}
	return Interval(p_min.min, p_max.max);
}

inline Interval lerp(const Interval &a, const Interval &b, const Interval &t) {
	if (t.is_single_value()) {
		return Interval(Math::lerp(a.min, b.min, t.min), Math::lerp(a.max, b.max, t.min));
	} else {
		return a + t * (b - a);
	}
}

inline Interval sin(const Interval &i) {
	if (i.is_single_value()) {
		return Interval::from_single_value(Math::sin(i.min));
	} else {
		// TODO more precision
		// Simplified
		return Interval(-1.f, 1.f);
	}
}

inline Interval atan(const Interval &t) {
	if (t.is_single_value()) {
		return Interval::from_single_value(Math::atan(t.min));
	}
	// arctan is monotonic
	return Interval{ Math::atan(t.min), Math::atan(t.max) };
}

struct OptionalInterval {
	Interval value;
	bool valid;
};

inline Interval atan2(const Interval &y, const Interval &x, OptionalInterval *secondary_output) {
	if (y.is_single_value() && x.is_single_value()) {
		return Interval::from_single_value(Math::atan2(y.min, x.max));
	}

	//          y
	//      1   |    0
	//          |
	//    ------o------x
	//          |
	//      2   |    3

	bool in_nx = x.min <= 0.f;
	bool in_px = x.max >= 0.f;
	bool in_ny = y.min <= 0.f;
	bool in_py = y.max >= 0.f;

	if (secondary_output != nullptr) {
		secondary_output->valid = false;
	}

	if (in_nx && in_px && in_ny && in_py) {
		// All quadrants
		return Interval{ -Math_PI, Math_PI };
	}

	bool in_q0 = in_px && in_py;
	bool in_q1 = in_nx && in_py;
	bool in_q2 = in_nx && in_ny;
	bool in_q3 = in_px && in_ny;

	// Double-quadrants

	if (in_q0 && in_q1) {
		return Interval(Math::atan2(y.min, x.max), Math::atan2(y.min, x.min));
	}
	if (in_q1 && in_q2) {
		if (secondary_output == nullptr) {
			// When crossing those two quadrants, the angle wraps from PI to -PI.
			// We would be forced to split the interval in two, but we have to return only one.
			// For correctness, we have to return the full range...
			return Interval{ -Math_PI, Math_PI };
		} else {
			// But, sometimes we can afford splitting the interval,
			// especially if our use case joins it back to one.
			// Q1
			secondary_output->value = Interval(Math::atan2(y.max, x.max), Math_PI);
			secondary_output->valid = true;
			// Q2
			return Interval(-Math_PI, Math::atan2(y.min, x.max));
		}
	}
	if (in_q2 && in_q3) {
		return Interval(Math::atan2(y.max, x.min), Math::atan2(y.max, x.max));
	}
	if (in_q3 && in_q0) {
		return Interval(Math::atan2(y.min, x.min), Math::atan2(y.max, x.min));
	}

	// Single quadrants

	if (in_q0) {
		return Interval(Math::atan2(y.min, x.max), Math::atan2(y.max, x.min));
	}
	if (in_q1) {
		return Interval(Math::atan2(y.max, x.max), Math::atan2(y.min, x.min));
	}
	if (in_q2) {
		return Interval(Math::atan2(y.max, x.min), Math::atan2(y.min, x.max));
	}
	if (in_q3) {
		return Interval(Math::atan2(y.min, x.min), Math::atan2(y.max, x.max));
	}

	// Bwarf.
	return Interval{ -Math_PI, Math_PI };
}

inline Interval floor(const Interval &i) {
	// Floor is monotonic so I guess we can just do that?
	return Interval(Math::floor(i.min), Math::floor(i.max));
}

inline Interval round(const Interval &i) {
	// Floor is monotonic so I guess we can just do that?
	return Interval(Math::floor(i.min + 0.5f), Math::floor(i.max + 0.5f));
}

inline Interval stepify(const Interval &p_value, const Interval &p_step) {
	// TODO Division by zero returns 0, which is different from Godot's stepify. May have to change that
	return floor(p_value / p_step + Interval::from_single_value(0.5f)) * p_step;
}

inline Interval wrapf(const Interval &x, const Interval &d) {
	return x - (d * floor(x / d));
}

inline Interval smoothstep(float p_from, float p_to, Interval p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return Interval::from_single_value(p_from);
	}
	// Smoothstep is monotonic
	float v0 = smoothstep(p_from, p_to, p_weight.min);
	float v1 = smoothstep(p_from, p_to, p_weight.max);
	if (v0 <= v1) {
		return Interval(v0, v1);
	} else {
		return Interval(v1, v0);
	}
}

inline Interval get_length(const Interval &x, const Interval &y) {
	return sqrt(x * x + y * y);
}

inline Interval get_length(const Interval &x, const Interval &y, const Interval &z) {
	return sqrt(x * x + y * y + z * z);
}

#endif // INTERVAL_H
