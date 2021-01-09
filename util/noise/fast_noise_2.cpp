#include "fast_noise_2.h"

FastNoise2::FastNoise2() {
	// TODO Testing
	set_encoded_node_tree("DQAFAAAAAAAAQAgAAAAAAD8=");
}

void FastNoise2::set_encoded_node_tree(String data) {
	CharString cs = data.utf8();
	_generator = FastNoise::NewFromEncodedNodeTree(cs.get_data());
}

String FastNoise2::get_encoded_node_tree() const {
	// TODO
	return "";
}

void FastNoise2::get_noise_2d(unsigned int count, const float *src_x, const float *src_y, float *dst) {
	_generator->GenPositionArray2D(dst, count, src_x, src_y, 0, 0, _seed);
}

void FastNoise2::get_noise_3d(
		unsigned int count, const float *src_x, const float *src_y, const float *src_z, float *dst) {
	_generator->GenPositionArray3D(dst, count, src_x, src_y, src_z, 0, 0, 0, _seed);
}

void FastNoise2::_bind_methods() {
	// TODO
}
