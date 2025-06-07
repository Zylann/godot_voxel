#ifndef VOXEL_MESHER_BLOCKY_H
#define VOXEL_MESHER_BLOCKY_H

#include "../../util/godot/classes/mesh.h"
#include "../../util/math/color.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_mesher.h"
#include "blocky_tint_sampler.h"
#include "voxel_blocky_library_base.h"

#include <vector>

namespace zylann::voxel {

// Interprets voxel values as indexes to models in a VoxelBlockyLibrary, and batches them together.
// Overlapping faces are removed from the final mesh.
class VoxelMesherBlocky : public VoxelMesher {
	GDCLASS(VoxelMesherBlocky, VoxelMesher)

public:
	static const int PADDING = 1;

	VoxelMesherBlocky();
	~VoxelMesherBlocky();

	void set_library(Ref<VoxelBlockyLibraryBase> library);
	Ref<VoxelBlockyLibraryBase> get_library() const;

	void set_occlusion_darkness(float darkness);
	float get_occlusion_darkness() const;

	void set_occlusion_enabled(bool enable);
	bool get_occlusion_enabled() const;

	enum Side {
		SIDE_NEGATIVE_X = 0,
		SIDE_POSITIVE_X,
		SIDE_NEGATIVE_Y,
		SIDE_POSITIVE_Y,
		SIDE_NEGATIVE_Z,
		SIDE_POSITIVE_Z,
		SIDE_COUNT
	};

	void set_shadow_occluder_side(Side side, bool enabled);
	bool get_shadow_occluder_side(Side side) const;
	uint8_t get_shadow_occluder_mask() const;

	enum TintMode {
		TINT_NONE = blocky::TintSampler::MODE_NONE,
		TINT_RAW_COLOR = blocky::TintSampler::MODE_RAW,
		TINT_MODE_COUNT = blocky::TintSampler::MODE_COUNT
	};

	TintMode get_tint_mode() const;
	void set_tint_mode(const TintMode new_mode);

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	// TODO GDX: Resource::duplicate() cannot be overriden (while it can in modules).
	// This will lead to performance degradation and maybe unexpected behavior.
	// The way it works has also changed in Godot 4.5 so I gave up trying to implement it.
	//
	// #if defined(ZN_GODOT)
	// 	Ref<Resource> duplicate(bool p_subresources = false) const override;
	// #elif defined(ZN_GODOT_EXTENSION)
	// 	Ref<Resource> duplicate(bool p_subresources = false) const;
	// #endif

	int get_used_channels_mask() const override;

	bool supports_lod() const override {
		return true;
	}

	Ref<Material> get_material_by_index(unsigned int index) const override;
	unsigned int get_material_index_count() const override;

	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		StdVector<Vector3f> positions;
		StdVector<Vector3f> normals;
		StdVector<Vector2f> uvs;
		StdVector<Color> colors;
		StdVector<int> indices;
		StdVector<float> tangents;

		void clear() {
			positions.clear();
			normals.clear();
			uvs.clear();
			colors.clear();
			indices.clear();
			tangents.clear();
		}
	};

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &out_warnings) const override;
#endif

	bool is_generating_collision_surface() const override {
		return true;
	}

protected:
	static void _bind_methods();

private:
	struct Parameters {
		float baked_occlusion_darkness = 0.8;
		bool bake_occlusion = true;
		uint8_t shadow_occluders_mask = 0;
		Ref<VoxelBlockyLibraryBase> library;
		TintMode tint_mode = TINT_NONE;
	};

	struct Cache {
		StdVector<Arrays> arrays_per_material;
	};

	// Parameters
	Parameters _parameters;
	RWLock _parameters_lock;

	// Work cache
	static Cache &get_tls_cache();
};

namespace blocky {

inline bool is_face_visible_regardless_of_shape(const BakedModel &vt, const BakedModel &other_vt) {
	// TODO Maybe we could get rid of `empty` here and instead set `culls_neighbors` to false during baking
	return other_vt.empty || (other_vt.transparency_index > vt.transparency_index) || !other_vt.culls_neighbors;
}

// Does not account for other factors
inline bool is_face_visible_according_to_shape(
		const BakedLibrary &lib,
		const BakedModel &vt,
		const BakedModel &other_vt,
		const int side
) {
	const unsigned int ai = vt.model.side_pattern_indices[side];
	const unsigned int bi = other_vt.model.side_pattern_indices[Cube::g_opposite_side[side]];
	// Patterns are not the same, and B does not occlude A
	return (ai != bi) && !lib.get_side_pattern_occlusion(bi, ai);
}

inline bool is_face_visible(
		const BakedLibrary &lib,
		const BakedModel &vt,
		const uint32_t other_voxel_id,
		const int side
) {
	if (other_voxel_id < lib.models.size()) {
		const BakedModel &other_vt = lib.models[other_voxel_id];
		if (is_face_visible_regardless_of_shape(vt, other_vt)) {
			return true;
		} else {
			return is_face_visible_according_to_shape(lib, vt, other_vt, side);
		}
	}
	return true;
}

} // namespace blocky

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherBlocky::Side)
VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherBlocky::TintMode)

#endif // VOXEL_MESHER_BLOCKY_H
