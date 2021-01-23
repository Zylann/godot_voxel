#ifndef VOXEL_FAST_NOISE_2_H
#define VOXEL_FAST_NOISE_2_H

#include "FastNoise/FastNoise.h"
#include <core/resource.h>

// Can't call it FastNoise? because FastNoise is a namespace already
class FastNoise2 : public Resource {
	GDCLASS(FastNoise2, Resource)
public:
	FastNoise2();

	void set_encoded_node_tree(String data);
	String get_encoded_node_tree() const;

	void get_noise_2d(unsigned int count, const float *src_x, const float *src_y, float *dst);
	void get_noise_3d(unsigned int count, const float *src_x, const float *src_y, const float *src_z, float *dst);

private:
	static void _bind_methods();

	FastNoise::SmartNode<> _generator;
	int _seed = 1337;
};

#endif // VOXEL_FAST_NOISE_2_H
