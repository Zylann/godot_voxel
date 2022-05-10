#ifndef ZN_VECTOR3D_H
#define ZN_VECTOR3D_H

namespace zylann {

struct Vector3d {
	static const unsigned int AXIS_X = 0;
	static const unsigned int AXIS_Y = 1;
	static const unsigned int AXIS_Z = 2;
	static const unsigned int AXIS_COUNT = 3;

	union {
		struct {
			double x;
			double y;
			double z;
		};
		double coords[3];
	};

	Vector3d() : x(0), y(0), z(0) {}

	// It is recommended to use `explicit` because otherwise it would open the door to plenty of implicit conversions
	// which would make many cases ambiguous.
	explicit Vector3d(double p_v) : x(p_v), y(p_v), z(p_v) {}

	Vector3d(double p_x, double p_y, double p_z) : x(p_x), y(p_y), z(p_z) {}

	inline Vector3d &operator+=(const Vector3d &p_v) {
		x += p_v.x;
		y += p_v.y;
		z += p_v.z;
		return *this;
	}

	inline Vector3d operator+(const Vector3d &p_v) const {
		return Vector3d(x + p_v.x, y + p_v.y, z + p_v.z);
	}

	inline Vector3d &operator-=(const Vector3d &p_v) {
		x -= p_v.x;
		y -= p_v.y;
		z -= p_v.z;
		return *this;
	}

	inline Vector3d operator-(const Vector3d &p_v) const {
		return Vector3d(x - p_v.x, y - p_v.y, z - p_v.z);
	}

	inline Vector3d &operator*=(const Vector3d &p_v) {
		x *= p_v.x;
		y *= p_v.y;
		z *= p_v.z;
		return *this;
	}

	inline Vector3d operator*(const Vector3d &p_v) const {
		return Vector3d(x * p_v.x, y * p_v.y, z * p_v.z);
	}

	inline Vector3d &operator/=(const Vector3d &p_v) {
		x /= p_v.x;
		y /= p_v.y;
		z /= p_v.z;
		return *this;
	}

	inline Vector3d operator/(const Vector3d &p_v) const {
		return Vector3d(x / p_v.x, y / p_v.y, z / p_v.z);
	}

	inline Vector3d &operator*=(const double p_scalar) {
		x *= p_scalar;
		y *= p_scalar;
		z *= p_scalar;
		return *this;
	}

	inline Vector3d operator*(const double p_scalar) const {
		return Vector3d(x * p_scalar, y * p_scalar, z * p_scalar);
	}

	inline Vector3d &operator/=(const double p_scalar) {
		x /= p_scalar;
		y /= p_scalar;
		z /= p_scalar;
		return *this;
	}

	inline Vector3d operator/(const double p_scalar) const {
		return Vector3d(x / p_scalar, y / p_scalar, z / p_scalar);
	}

	inline Vector3d operator-() const {
		return Vector3d(-x, -y, -z);
	}
};

namespace math {

inline Vector3d cross(const Vector3d &a, const Vector3d &b) {
	const Vector3d ret( //
			(a.y * b.z) - (a.z * b.y), //
			(a.z * b.x) - (a.x * b.z), //
			(a.x * b.y) - (a.y * b.x));
	return ret;
}

inline double dot(const Vector3d &a, const Vector3d &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace math
} // namespace zylann

#endif // ZN_VECTOR3D_H
