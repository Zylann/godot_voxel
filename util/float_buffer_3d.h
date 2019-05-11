#ifndef FLOAT_BUFFER_3D_H
#define FLOAT_BUFFER_3D_H

#include "../math/vector3i.h"

// Simple 3D array of floats, until VoxelBuffer supports higher-precision
class FloatBuffer3D {
public:
	~FloatBuffer3D();

	void clear();
	void create(Vector3i size);
	void fill(float v);

	inline const Vector3i &get_size() const { return _size; }

	float get(int x, int y, int z) const;
	float get_clamped(int x, int y, int z) const;
	float get_trilinear(float x, float y, float z) const;

	void set(int x, int y, int z, float v);

private:
	inline int get_index(int x, int y, int z) const {
		return y + _size.y * (x + _size.x * z);
	}

	float *_data = nullptr;
	Vector3i _size;
};

#endif // FLOAT_BUFFER_3D_H
