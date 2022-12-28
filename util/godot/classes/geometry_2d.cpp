#include "geometry_2d.h"
#include "../../math/conv.h"
#include "../../profiling.h"

namespace zylann {

void geometry_2d_make_atlas(Span<const Vector2i> p_sizes, std::vector<Vector2i> &r_result, Vector2i &r_size) {
	ZN_PROFILE_SCOPE();

#if defined(ZN_GODOT)

	Vector<Vector2i> sizes;
	sizes.resize(p_sizes.size());
	Vector2i *sizes_data = sizes.ptrw();
	ZN_ASSERT_RETURN(sizes_data != nullptr);
	memcpy(sizes_data, p_sizes.data(), p_sizes.size() * sizeof(Vector2i));

	Vector<Vector2i> result;
	Geometry2D::make_atlas(sizes, result, r_size);

	r_result.resize(result.size());
	memcpy(r_result.data(), result.ptr(), result.size() * sizeof(Vector2i));

#elif defined(ZN_GODOT_EXTENSION)

	// PackedVector2iArray doesn't exist, so have to convert to float. Internally Godot allocates another array and
	// converts back to ints. Then allocates another float array and converts results to floats, returns it, and then
	// we finally convert back to ints... so much for not having added `PackedVector2iArray`.

	PackedVector2Array sizes;
	sizes.resize(p_sizes.size());
	Vector2 *sizes_data = sizes.ptrw();
	ZN_ASSERT_RETURN(sizes_data != nullptr);
	for (unsigned int i = 0; i < p_sizes.size(); ++i) {
		sizes_data[i] = p_sizes[i];
	}

	Dictionary result = Geometry2D::get_singleton()->make_atlas(sizes);
	PackedVector2Array positions = result["points"];
	r_size = result["size"];

	r_result.resize(positions.size());
	const Vector2 *positions_data = positions.ptr();
	ZN_ASSERT_RETURN(positions_data != nullptr);
	for (unsigned int i = 0; i < r_result.size(); ++i) {
		r_result[i] = to_vec2i(positions_data[i]);
	}

#endif
}

} // namespace zylann
