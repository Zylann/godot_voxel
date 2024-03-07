#include "test_fast_noise_2.h"
#include "../../util/godot/classes/image.h"
#include "../../util/noise/fast_noise_2.h"

namespace zylann::tests {

void test_fast_noise_2_basic() {
	// Very basic test. The point is to make sure it doesn't crash, so there is no special condition to check.
	Ref<FastNoise2> noise;
	noise.instantiate();
	float nv = noise->get_noise_2d_single(Vector2(42, 666));
	print_line(String("SIMD level: {0}").format(varray(FastNoise2::get_simd_level_name(noise->get_simd_level()))));
	print_line(String("Noise: {0}").format(varray(nv)));
	Ref<Image> im = godot::create_empty_image(256, 256, false, Image::FORMAT_RGB8);
	noise->generate_image(im, false);
	// im->save_png("zylann_test_fastnoise2.png");
}

void test_fast_noise_2_empty_encoded_node_tree() {
	Ref<FastNoise2> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoise2::TYPE_ENCODED_NODE_TREE);
	// This can print an error, but should not crash
	noise->update_generator();
}

} // namespace zylann::tests
