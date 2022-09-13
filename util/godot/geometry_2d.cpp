#include "geometry_2d.h"
#include "../math/conv.h"
#include "../profiling.h"

namespace zylann {

void geometry_2d_make_atlas(Span<const Vector2i> p_sizes, std::vector<Vector2i> &r_result, Vector2i &r_size) {
	ZN_PROFILE_SCOPE();

#if defined(ZN_GODOT)

#error "TODO Implement portable `make_atlas` for Godot modules

#elif defined(ZN_GODOT_EXTENSION)

	PackedVector2Array sizes;
	sizes.resize(p_sizes.size());
	Vector2 *sizes_data = sizes.ptrw();
	ZN_ASSERT_RETURN(sizes_data != nullptr);
	for (unsigned int i = 0; i < p_sizes.size(); ++i) {
		sizes_data[i] = p_sizes[i];
	}

	Dictionary result = Geometry2D::get_singleton()->make_atlas(sizes);
	PackedVector2Array positions = result["points"];
	Vector2 size = result["size"];

	r_result.resize(positions.size());
	const Vector2 *positions_data = positions.ptr();
	ZN_ASSERT_RETURN(positions_data != nullptr);
	for (unsigned int i = 0; i < r_result.size(); ++i) {
		r_result[i] = to_vec2i(positions_data[i]);
	}

	r_size = to_vec2i(size);

#endif
}

} // namespace zylann
