#include "float_buffer_3d.h"
#include "utility.h"

FloatBuffer3D::~FloatBuffer3D() {
	clear();
}

void FloatBuffer3D::clear() {
	if (_data) {
		memfree(_data);
		_data = nullptr;
	}
}

void FloatBuffer3D::create(Vector3i size) {
	ERR_FAIL_COND(size.x <= 0);
	ERR_FAIL_COND(size.y <= 0);
	ERR_FAIL_COND(size.z <= 0);
	clear();
	_data = (float *)memalloc(size.x * size.y * size.z * sizeof(float));
	_size = size;
}

void FloatBuffer3D::fill(float v) {
	ERR_FAIL_COND(_data == nullptr);
	int count = _size.x * _size.y * _size.z;
	for (int i = 0; i < count; ++i) {
		_data[i] = v;
	}
}

float FloatBuffer3D::get(int x, int y, int z) const {
	ERR_FAIL_COND_V(_data == nullptr, 0);
	ERR_FAIL_COND_V(x < 0 || x >= _size.x, 0);
	ERR_FAIL_COND_V(y < 0 || y >= _size.y, 0);
	ERR_FAIL_COND_V(z < 0 || z >= _size.z, 0);
	return _data[get_index(x, y, z)];
}

void FloatBuffer3D::set(int x, int y, int z, float v) {
	ERR_FAIL_COND(_data == nullptr);
	ERR_FAIL_COND(x < 0 || x >= _size.x);
	ERR_FAIL_COND(y < 0 || y >= _size.y);
	ERR_FAIL_COND(z < 0 || z >= _size.z);
	_data[get_index(x, y, z)] = v;
}

float FloatBuffer3D::get_clamped(int x, int y, int z) const {
	ERR_FAIL_COND_V(_data == nullptr, 0);
	x = x >= _size.x ? _size.x - 1 : x;
	y = y >= _size.y ? _size.y - 1 : y;
	z = z >= _size.z ? _size.z - 1 : z;
	return _data[get_index(x, y, z)];
}

float FloatBuffer3D::get_trilinear(float x, float y, float z) const {
	ERR_FAIL_COND_V(_data == nullptr, 0);

	int x0 = static_cast<int>(x);
	int y0 = static_cast<int>(y);
	int z0 = static_cast<int>(z);

	int x1 = static_cast<int>(Math::ceil(x));
	int y1 = static_cast<int>(Math::ceil(y));
	int z1 = static_cast<int>(Math::ceil(z));

	float v0 = get_clamped(x0, y0, z0);
	float v1 = get_clamped(x1, y0, z0);
	float v2 = get_clamped(x1, y0, z1);
	float v3 = get_clamped(x0, y0, z1);

	float v4 = get_clamped(x0, y1, z0);
	float v5 = get_clamped(x1, y1, z0);
	float v6 = get_clamped(x1, y1, z1);
	float v7 = get_clamped(x0, y1, z1);

	Vector3 rpos(x - x0, y - y0, z - z0);

	return interpolate(v0, v1, v2, v3, v4, v5, v6, v7, rpos);
}
