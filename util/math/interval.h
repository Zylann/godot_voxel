#ifndef ZN_INTERVAL_H
#define ZN_INTERVAL_H

#include "../string/std_stringstream.h"
#include "funcs.h"
#include <limits>
#include <type_traits>

namespace zylann {
namespace math {

namespace interval_impl {
void check_range_once(float min, float max);
void check_range_once(double min, double max);
} // namespace interval_impl

// For interval arithmetic
template <typename T>
struct IntervalT {
	static_assert(std::is_floating_point<T>::value);

	// Both inclusive
	T min;
	T max;

	inline IntervalT() : min(0), max(0) {}

	inline IntervalT(T p_min, T p_max) : min(p_min), max(p_max) {
#if DEV_ENABLED
		ZN_ASSERT(p_min <= p_max);
#elif DEBUG_ENABLED
		// Don't crash but keep signaling
		interval_impl::check_range_once(p_min, p_max);
#endif
	}

	inline static IntervalT from_single_value(T p_val) {
		return IntervalT(p_val, p_val);
	}

	inline static IntervalT from_unordered_values(T a, T b) {
		return IntervalT(math::min(a, b), math::max(a, b));
	}

	inline static IntervalT from_infinity() {
		return IntervalT(-std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
	}

	inline static IntervalT from_union(const IntervalT a, const IntervalT b) {
		return IntervalT(math::min(a.min, b.min), math::max(a.max, b.max));
	}

	inline bool contains(T v) const {
		return v >= min && v <= max;
	}

	inline bool contains(IntervalT other) const {
		return other.min >= min && other.max <= max;
	}

	inline bool is_single_value() const {
		return min == max;
	}

	inline void add_point(T x) {
		if (x < min) {
			min = x;
		} else if (x > max) {
			max = x;
		}
	}

	inline IntervalT padded(T e) const {
		return IntervalT(min - e, max + e);
	}

	inline void add_interval(IntervalT other) {
		add_point(other.min);
		add_point(other.max);
	}

	inline T length() const {
		return max - min;
	}

	inline bool operator==(const IntervalT &other) const {
		return min == other.min && max == other.max;
	}

	inline bool operator!=(const IntervalT &other) const {
		return min != other.min || max != other.max;
	}

	inline IntervalT operator+(T x) const {
		return IntervalT{ min + x, max + x };
	}

	inline IntervalT operator+(const IntervalT &other) const {
		return IntervalT{ min + other.min, max + other.max };
	}

	inline void operator+=(const IntervalT &other) {
		*this = *this + other;
	}

	inline IntervalT operator-(T x) const {
		return IntervalT{ min - x, max - x };
	}

	inline IntervalT operator-(const IntervalT &other) const {
		return IntervalT{ min - other.max, max - other.min };
	}

	inline IntervalT operator-() const {
		return IntervalT{ -max, -min };
	}

	inline IntervalT operator*(T x) const {
		const T a = min * x;
		const T b = max * x;
		if (a < b) {
			return IntervalT(a, b);
		} else {
			return IntervalT(b, a);
		}
	}

	inline IntervalT operator*(const IntervalT &other) const {
		// Note, if the two operands have the same source (i.e you are doing x^2), this may lead to suboptimal results.
		// You may then prefer using a more dedicated function.
		const T a = min * other.min;
		const T b = min * other.max;
		const T c = max * other.min;
		const T d = max * other.max;
		return IntervalT{ math::min(a, b, c, d), math::max(a, b, c, d) };
	}

	inline void operator*=(T x) {
		*this = *this * x;
	}

	inline void operator*=(IntervalT x) {
		*this = *this * x;
	}

	inline IntervalT operator/(const IntervalT &other) const {
		if (other.is_single_value() && other.min == 0) {
			// Division by zero. In Voxel graph, we return 0.
			return IntervalT::from_single_value(0);
		}
		if (other.contains(0.f)) {
			// TODO May need something more precise
			return IntervalT::from_infinity();
		}
		const T a = min / other.min;
		const T b = min / other.max;
		const T c = max / other.min;
		const T d = max / other.max;
		return IntervalT{ math::min(a, b, c, d), math::max(a, b, c, d) };
	}

	inline IntervalT operator/(T x) const {
		// TODO Implement proper division by interval
		return *this * (T(1.0) / x);
	}

	inline void operator/=(T x) {
		*this = *this / x;
	}
};

template <typename T>
struct Interval2T {
	IntervalT<T> x;
	IntervalT<T> y;
};

template <typename T>
struct Interval3T {
	IntervalT<T> x;
	IntervalT<T> y;
	IntervalT<T> z;
};

// TODO Use float in graph stuff
using Interval = IntervalT<real_t>;
using Interval2 = Interval2T<real_t>;
using Interval3 = Interval3T<real_t>;

template <typename T>
inline IntervalT<T> operator*(float b, const IntervalT<T> &a) {
	return a * b;
}

// Functions declared outside, so using intervals or numbers can be the same code (templatable)

template <typename T>
inline IntervalT<T> min_interval(const IntervalT<T> &a, const IntervalT<T> &b) {
	return IntervalT<T>(min(a.min, b.min), min(a.max, b.max));
}

template <typename T>
inline IntervalT<T> max_interval(const IntervalT<T> &a, const IntervalT<T> &b) {
	return IntervalT<T>(max(a.min, b.min), max(a.max, b.max));
}

template <typename T>
inline IntervalT<T> min_interval(const IntervalT<T> &a, const T b) {
	return IntervalT<T>(min(a.min, b), min(a.max, b));
}

template <typename T>
inline IntervalT<T> max_interval(const IntervalT<T> &a, const T b) {
	return IntervalT<T>(max(a.min, b), max(a.max, b));
}

template <typename T>
inline IntervalT<T> sqrt(const IntervalT<T> &i) {
	// Avoiding negative numbers because they are undefined, also because VoxelGeneratorGraph defines its SQRT node this
	// way.
	// TODO Rename function `sqrt_or_zero` to be explicit about this?
	return IntervalT<T>{ sqrt(max<T>(0, i.min)), sqrt(max<T>(0, i.max)) };
}

template <typename T>
inline IntervalT<T> abs(const IntervalT<T> &i) {
	return IntervalT<T>{ i.contains(0) ? 0 : min(abs(i.min), abs(i.max)), max(abs(i.min), abs(i.max)) };
}

template <typename T>
inline IntervalT<T> clamp(const IntervalT<T> &i, const IntervalT<T> &p_min, const IntervalT<T> &p_max) {
	if (p_min.is_single_value() && p_max.is_single_value()) {
		return { clamp(i.min, p_min.min, p_max.min), clamp(i.max, p_min.min, p_max.min) };
	}
	if (i.min >= p_min.max && i.max <= p_max.min) {
		return i;
	}
	if (i.min >= p_max.max) {
		return IntervalT<T>::from_single_value(p_max.max);
	}
	if (i.max <= p_min.min) {
		return IntervalT<T>::from_single_value(p_min.min);
	}
	return IntervalT<T>(p_min.min, p_max.max);
}

template <typename T>
inline IntervalT<T> lerp(const IntervalT<T> &a, const IntervalT<T> &b, const IntervalT<T> &t) {
	if (t.is_single_value()) {
		return IntervalT<T>(lerp(a.min, b.min, t.min), lerp(a.max, b.max, t.min));
	}

	// TODO Could just write scalar `lerp` calls?
	const T v0 = a.min + t.min * (b.min - a.min);
	const T v1 = a.max + t.min * (b.min - a.max);
	const T v2 = a.min + t.max * (b.min - a.min);
	const T v3 = a.max + t.max * (b.min - a.max);
	const T v4 = a.min + t.min * (b.max - a.min);
	const T v5 = a.max + t.min * (b.max - a.max);
	const T v6 = a.min + t.max * (b.max - a.min);
	const T v7 = a.max + t.max * (b.max - a.max);

	return IntervalT<T>(min(v0, v1, v2, v3, v4, v5, v6, v7), max(v0, v1, v2, v3, v4, v5, v6, v7));
}

template <typename T>
inline IntervalT<T> sin(const IntervalT<T> &i) {
	if (i.is_single_value()) {
		return IntervalT<T>::from_single_value(sin(i.min));
	} else {
		// TODO more precision
		// Simplified
		return IntervalT<T>(-1, 1);
	}
}

template <typename T>
inline IntervalT<T> atan(const IntervalT<T> &t) {
	if (t.is_single_value()) {
		return IntervalT<T>::from_single_value(atan(t.min));
	}
	// arctan is monotonic
	return IntervalT<T>{ atan(t.min), atan(t.max) };
}

template <typename T>
struct OptionalIntervalT {
	IntervalT<T> value;
	bool valid = false;
};

using OptionalInterval = OptionalIntervalT<real_t>;

template <typename T>
inline IntervalT<T> atan2(const IntervalT<T> &y, const IntervalT<T> &x, OptionalIntervalT<T> *secondary_output) {
	if (y.is_single_value() && x.is_single_value()) {
		return Interval::from_single_value(atan2(y.min, x.max));
	}

	//          y
	//      1   |    0
	//          |
	//    ------o------x
	//          |
	//      2   |    3

	const bool in_nx = x.min <= 0;
	const bool in_px = x.max >= 0;
	const bool in_ny = y.min <= 0;
	const bool in_py = y.max >= 0;

	if (secondary_output != nullptr) {
		secondary_output->valid = false;
	}

	if (in_nx && in_px && in_ny && in_py) {
		// All quadrants
		return IntervalT<T>{ -Math_PI, Math_PI };
	}

	const bool in_q0 = in_px && in_py;
	const bool in_q1 = in_nx && in_py;
	const bool in_q2 = in_nx && in_ny;
	const bool in_q3 = in_px && in_ny;

	// Double-quadrants

	if (in_q0 && in_q1) {
		return IntervalT<T>(atan2(y.min, x.max), atan2(y.min, x.min));
	}
	if (in_q1 && in_q2) {
		if (secondary_output == nullptr) {
			// When crossing those two quadrants, the angle wraps from PI to -PI.
			// We would be forced to split the interval in two, but we have to return only one.
			// For correctness, we have to return the full range...
			return IntervalT<T>{ -Math_PI, Math_PI };
		} else {
			// But, sometimes we can afford splitting the interval,
			// especially if our use case joins it back to one.
			// Q1
			secondary_output->value = IntervalT<T>(atan2(y.max, x.max), Math_PI);
			secondary_output->valid = true;
			// Q2
			return IntervalT<T>(-Math_PI, atan2(y.min, x.max));
		}
	}
	if (in_q2 && in_q3) {
		return IntervalT<T>(atan2(y.max, x.min), atan2(y.max, x.max));
	}
	if (in_q3 && in_q0) {
		return IntervalT<T>(atan2(y.min, x.min), atan2(y.max, x.min));
	}

	// Single quadrants

	if (in_q0) {
		return IntervalT<T>(atan2(y.min, x.max), atan2(y.max, x.min));
	}
	if (in_q1) {
		return IntervalT<T>(atan2(y.max, x.max), atan2(y.min, x.min));
	}
	if (in_q2) {
		return IntervalT<T>(atan2(y.max, x.min), atan2(y.min, x.max));
	}
	if (in_q3) {
		return IntervalT<T>(atan2(y.min, x.min), atan2(y.max, x.max));
	}

	// Bwarf.
	return IntervalT<T>{ -Math_PI, Math_PI };
}

template <typename T>
inline IntervalT<T> floor(const IntervalT<T> &i) {
	// Floor is monotonic so I guess we can just do that?
	return IntervalT<T>(floor(i.min), floor(i.max));
}

template <typename T>
inline IntervalT<T> round(const IntervalT<T> &i) {
	// Floor is monotonic so I guess we can just do that?
	return IntervalT<T>(floor(i.min + T(0.5)), floor(i.max + T(0.5)));
}

template <typename T>
inline IntervalT<T> snapped(const IntervalT<T> &p_value, const IntervalT<T> &p_step) {
	// TODO Division by zero returns 0, which is different from Godot's stepify. May have to change that
	return floor(p_value / p_step + IntervalT<T>::from_single_value(T(0.5))) * p_step;
}

template <typename T>
inline IntervalT<T> wrapf(const IntervalT<T> &x, const IntervalT<T> &d) {
	return x - (d * floor(x / d));
}

template <typename T>
inline IntervalT<T> smoothstep(const T p_from, const T p_to, const IntervalT<T> p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return IntervalT<T>::from_single_value(p_from);
	}
	// Smoothstep is monotonic
	const T v0 = smoothstep(p_from, p_to, p_weight.min);
	const T v1 = smoothstep(p_from, p_to, p_weight.max);
	if (v0 <= v1) {
		return IntervalT<T>(v0, v1);
	} else {
		return IntervalT<T>(v1, v0);
	}
}

// Prefer this over x*x, this will provide a more optimal result
template <typename T>
inline IntervalT<T> squared(const IntervalT<T> &x) {
	if (x.min < 0 && x.max > 0) {
		// The interval includes 0
		return IntervalT<T>{ T(0), max(x.min * x.min, x.max * x.max) };
	}
	// The interval is only on one side of the parabola
	if (x.max <= 0) {
		// Negative side: monotonic descending
		return IntervalT<T>{ x.max * x.max, x.min * x.min };
	} else {
		// Positive side: monotonic ascending
		return IntervalT<T>{ x.min * x.min, x.max * x.max };
	}
}

// Prefer this instead of doing polynomials with a single interval, this will provide a more optimal result
template <typename T>
inline IntervalT<T> polynomial_second_degree(const IntervalT<T> x, T a, T b, T c) {
	// a*x*x + b*x + c

	if (a == 0) {
		if (b == 0) {
			return IntervalT<T>::from_single_value(c);
		} else {
			return b * x + c;
		}
	}

	const T parabola_x = -b / (T(2) * a);

	const T y0 = a * x.min * x.min + b * x.min + c;
	const T y1 = a * x.max * x.max + b * x.max + c;

	if (x.min < parabola_x && x.max > parabola_x) {
		// The interval includes the tip
		const T parabola_y = a * parabola_x * parabola_x + b * parabola_x + c;
		if (a < 0) {
			return IntervalT<T>(min(y0, y1), parabola_y);
		} else {
			return IntervalT<T>(parabola_y, max(y0, y1));
		}
	}
	// The interval is only on one side of the parabola
	if ((a >= 0 && x.min >= parabola_x) || (a < 0 && x.max < parabola_x)) {
		// Monotonic increasing
		return IntervalT<T>(y0, y1);
	} else {
		// Monotonic decreasing
		return IntervalT<T>(y1, y0);
	}
}

// Prefer this over x*x*x, this will provide a more optimal result
template <typename T>
inline IntervalT<T> cubed(const IntervalT<T> &x) {
	// x^3 is monotonic ascending
	const T minv = x.min * x.min * x.min;
	const T maxv = x.max * x.max * x.max;
	return IntervalT<T>{ minv, maxv };
}

template <typename T>
inline IntervalT<T> get_length(const IntervalT<T> &x, const IntervalT<T> &y) {
	return sqrt(squared(x) + squared(y));
}

template <typename T>
inline IntervalT<T> get_length(const IntervalT<T> &x, const IntervalT<T> &y, const IntervalT<T> &z) {
	return sqrt(squared(x) + squared(y) + squared(z));
}

template <typename T>
inline IntervalT<T> powi(IntervalT<T> x, int pi) {
	const T pf = pi;
	if (pi >= 0) {
		if (pi % 2 == 1) {
			// Positive odd powers: ascending
			return IntervalT<T>{ pow(x.min, pf), pow(x.max, pf) };
		} else {
			// Positive even powers: parabola
			if (x.min < 0 && x.max > 0) {
				// The interval includes 0
				return IntervalT<T>{ 0, max(pow(x.min, pf), pow(x.max, pf)) };
			}
			// The interval is only on one side of the parabola
			if (x.max <= 0) {
				// Negative side: monotonic descending
				return IntervalT<T>{ pow(x.max, pf), pow(x.min, pf) };
			} else {
				// Positive side: monotonic ascending
				return IntervalT<T>{ pow(x.min, pf), pow(x.max, pf) };
			}
		}
	} else {
		// TODO Negative integer powers
		return IntervalT<T>::from_infinity();
	}
}

template <typename T>
inline IntervalT<T> pow(IntervalT<T> x, float pf) {
	const int pi = pf;
	if (Math::is_equal_approx(pi, pf)) {
		return powi(x, pi);
	} else {
		// TODO Decimal powers
		return IntervalT<T>::from_infinity();
	}
}

template <typename T>
inline IntervalT<T> pow(IntervalT<T> x, IntervalT<T> p) {
	if (p.is_single_value()) {
		return pow(x, p.min);
	} else {
		// TODO Varying powers
		return IntervalT<T>::from_infinity();
	}
}

} // namespace math

StdStringStream &operator<<(StdStringStream &ss, const math::Interval &v);

} // namespace zylann

#endif // ZN_INTERVAL_H
