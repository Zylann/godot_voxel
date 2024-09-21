#ifndef VOXEL_INSTANCE_GENERATOR_H
#define VOXEL_INSTANCE_GENERATOR_H

// #include "../../storage/voxel_buffer.h"
#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/noise.h"
#include "../../util/math/transform3f.h"
#include "../../util/math/vector3i.h"
#include "../../util/thread/short_lock.h"
#include "up_mode.h"

#include <limits>

namespace zylann::voxel {

// TODO This may have to be moved to the meshing thread some day

// Decides where to spawn instances on top of a voxel surface.
// Note: to generate voxels themselves, see `VoxelGenerator`.
class VoxelInstanceGenerator : public Resource {
	GDCLASS(VoxelInstanceGenerator, Resource)
public:
	enum EmitMode {
		// Fastest, but can have noticeable patterns when using high densities or using simplified meshes
		EMIT_FROM_VERTICES,
		// Slower, but should have less noticeable patterns. Assumes all triangles use similar areas,
		// which is the case with non-simplified meshes obtained with marching cubes.
		EMIT_FROM_FACES_FAST,
		// Slower, but tries to not assume the area of triangles.
		EMIT_FROM_FACES,
		EMIT_ONE_PER_TRIANGLE,

		EMIT_MODE_COUNT
	};

	enum Distribution {
		DISTRIBUTION_LINEAR = 0,
		DISTRIBUTION_QUADRATIC,
		DISTRIBUTION_CUBIC,
		DISTRIBUTION_QUINTIC,
		DISTRIBUTION_COUNT
	};

	enum Dimension { //
		DIMENSION_2D = 0,
		DIMENSION_3D,
		DIMENSION_COUNT
	};

	// This API might change so for now it's not exposed to scripts.
	// Using 32-bit float transforms because those transforms are chunked, so their origins never really need to hold
	// large coordinates.
	void generate_transforms(
			StdVector<Transform3f> &out_transforms,
			Vector3i grid_position,
			int lod_index,
			int layer_id,
			Array surface_arrays,
			UpMode up_mode,
			// When generating a 2x2x2 data block area, bits in `octant_mask` tell which octant should be generated.
			// Bits set to zero will cause all instances in the corresponding octant to not be generated.
			uint8_t octant_mask,
			// This is block size in world space, not relative to LOD index
			float block_size
	);

	void set_density(float d);
	float get_density() const;

	void set_emit_mode(EmitMode mode);
	EmitMode get_emit_mode() const;

	void set_jitter(const float p_jitter);
	float get_jitter() const;

	void set_triangle_area_threshold(const float p_threshold);
	float get_triangle_area_threshold() const;

	void set_vertical_alignment(float valign);
	float get_vertical_alignment() const;

	void set_min_scale(float min_scale);
	float get_min_scale() const;

	void set_max_scale(float max_scale);
	float get_max_scale() const;

	void set_scale_distribution(Distribution distribution);
	Distribution get_scale_distribution() const;

	// TODO Add scale curve, in real life there are way more small items than big ones

	void set_offset_along_normal(float offset);
	float get_offset_along_normal() const;

	void set_min_slope_degrees(float degrees);
	float get_min_slope_degrees() const;

	void set_max_slope_degrees(float degrees);
	float get_max_slope_degrees() const;

	void set_min_height(float h);
	float get_min_height() const;

	void set_max_height(float h);
	float get_max_height() const;

	void set_random_vertical_flip(bool flip);
	bool get_random_vertical_flip() const;

	void set_random_rotation(bool enabled);
	bool get_random_rotation() const;

	void set_noise(Ref<Noise> noise);
	Ref<Noise> get_noise() const;

	void set_noise_graph(Ref<pg::VoxelGraphFunction> func);
	Ref<pg::VoxelGraphFunction> get_noise_graph() const;

	void set_noise_dimension(Dimension dim);
	Dimension get_noise_dimension() const;

	void set_noise_on_scale(float amount);
	float get_noise_on_scale() const;

	static inline int get_octant_index(const Vector3f pos, float half_block_size) {
		return get_octant_index(pos.x > half_block_size, pos.y > half_block_size, pos.z > half_block_size);
	}

	static inline int get_octant_index(bool x, bool y, bool z) {
		return int(x) | (int(y) << 1) | (int(z) << 2);
	}

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &warnings) const;
	void _validate_property(PropertyInfo &p_property) const;
#endif

private:
	void _on_noise_changed();
	void _on_noise_graph_changed();

	static void _bind_methods();

	float _density = 0.1f;
	float _jitter = 1.f;
	float _triangle_area_threshold_lod0 = 0.f;
	float _vertical_alignment = 1.f;
	float _min_scale = 1.f;
	float _max_scale = 1.f;
	float _offset_along_normal = 0.f;
	float _min_surface_normal_y = -1.f;
	float _max_surface_normal_y = 1.f;
	float _min_height = std::numeric_limits<float>::min();
	float _max_height = std::numeric_limits<float>::max();
	bool _random_vertical_flip = false;
	bool _random_rotation = true;
	EmitMode _emit_mode = EMIT_FROM_VERTICES;
	Distribution _scale_distribution = DISTRIBUTION_QUADRATIC;
	Ref<Noise> _noise;
	Dimension _noise_dimension = DIMENSION_3D;
	float _noise_on_scale = 0.f;

	// TODO Protect noise and noise graph members from multithreaded access

	// Required inputs:
	// - X
	// - Y
	// - Z
	// Possible outputs:
	// - Density
	Ref<pg::VoxelGraphFunction> _noise_graph;
	// TODO Sampling mode:
	// - Per vertex: recommended if many items with high density need it (will be shared among them)
	// - Per instance: recommended if items with low density need it

	// Stored separately for editor
	float _min_slope_degrees = 0.f;
	float _max_slope_degrees = 180.f;

	// Used when accessing pointer settings, since this generator can be used in a thread while the editor thread can
	// modify settings.
	mutable ShortLock _ptr_settings_lock;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelInstanceGenerator::EmitMode);
VARIANT_ENUM_CAST(zylann::voxel::VoxelInstanceGenerator::Distribution);
VARIANT_ENUM_CAST(zylann::voxel::VoxelInstanceGenerator::Dimension);

#endif // VOXEL_INSTANCE_GENERATOR_H
