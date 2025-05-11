#ifndef ZN_CUBEMAP_H
#define ZN_CUBEMAP_H

#include "../math/box3f.h"
#include "../math/color.h"
#include "../math/interval.h"
#include "../math/vector2f.h"
#include "../math/vector2i.h"
#include "../math/vector3f.h"
#include "classes/image.h"
#include "core/typed_array.h"
#include "macros.h"
#include <array>

ZN_GODOT_NAMESPACE_BEGIN
class Cubemap;
ZN_GODOT_NAMESPACE_END

namespace zylann {

// CPU implementation of a cubemap (equivalent of Image[6] but with proper API).
// I could almost inherit Godot's Cubemap, but APIs would start to conflict in awkward ways...
class ZN_Cubemap : public Resource {
	GDCLASS(ZN_Cubemap, Resource)
public:
	enum SideIndex {
		// OpenGL convention
		SIDE_POSITIVE_X = 0,
		SIDE_NEGATIVE_X = 1,
		SIDE_POSITIVE_Y = 2,
		SIDE_NEGATIVE_Y = 3,
		SIDE_POSITIVE_Z = 4,
		SIDE_NEGATIVE_Z = 5,
		SIDE_COUNT = 6
	};

	void create(const unsigned int resolution, const Image::Format format);
	void create_from_images(TypedArray<Image> p_images);
	bool is_valid() const;
	unsigned int get_resolution() const;
	Image::Format get_format() const;

	Image &get_image(const unsigned int side);
	const Image &get_image(const unsigned int side) const;
	const Ref<Image> get_image_ref(const unsigned int side) const;

	Ref<Cubemap> create_texture() const;

	Color sample_nearest(const Vector3f position) const;
	Color sample_nearest_prepad(const Vector3f position) const;

	// Pads each image by 1 pixel containing copies of neighbor pixels for fast linear sampling
	void make_linear_filterable();

	Color sample_linear_prepad(const Vector3f position) const;

	void sample_linear_prepad(
			Span<const float> x_array,
			Span<const float> y_array,
			Span<const float> z_array,
			Span<float> out_r,
			Span<float> out_g,
			Span<float> out_b,
			Span<float> out_a
	) const;

	// void compute_ranges();

	struct Range {
		std::array<math::Interval, 4> combined;
		std::array<uint8_t, SIDE_COUNT> sampled_sides;
		uint8_t sampled_sides_count = 0;
	};

	Range get_range(const Box3f box) const;

	// TODO GDX: Godot's `Resource::duplicate` method cannot be overriden by extensions.
	// It is necessary here, because we dont expose underlying images as properties at the moment.
	// Initially needed because of Voxel graph compilation using duplication as thread-safety measure.
	virtual Ref<ZN_Cubemap> zn_duplicate() const;

	struct UV {
		SideIndex side;
		// Normalized 0..1 coordinates into the side, with (0,0) being top-left
		Vector2f position;
	};

	static UV get_uv_from_xyz(const Vector3f pos);

	// Get -1..1 position on the cube. If you need a position on a sphere, you may normalize the result.
	template <int SIDE>
	static Vector3f get_xyz_from_uv(const Vector2f uv) {
		static_assert(SIDE >= 0 && SIDE < SIDE_COUNT);
		// convert range 0 to 1 to -1 to 1
		const float uc = 2.0f * uv.x - 1.0f;
		const float vc = 2.0f * uv.y - 1.0f;
		switch (SIDE) {
			case SIDE_POSITIVE_X:
				return Vector3f(1.f, vc, -uc);
			case SIDE_NEGATIVE_X:
				return Vector3f(-1.f, vc, uc);
			case SIDE_POSITIVE_Y:
				return Vector3f(uc, 1.f, vc);
			case SIDE_NEGATIVE_Y:
				return Vector3f(uc, -1.f, -vc);
			case SIDE_POSITIVE_Z:
				return Vector3f(uc, vc, 1.f);
			case SIDE_NEGATIVE_Z:
				return Vector3f(-uc, vc, -1.f);
			default:
				return Vector3f();
		}
	}

	static Vector3f get_xyz_from_uv(const Vector2f uv, const SideIndex face);

protected:
	void duplicate_to(ZN_Cubemap &d) const;

private:
	static void _bind_methods();

	std::array<Ref<Image>, SIDE_COUNT> _images;
	bool _padded = false;
};

} // namespace zylann

#endif // ZN_CUBEMAP_H
