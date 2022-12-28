#ifndef VOXEL_MESHER_BLOCKY_H
#define VOXEL_MESHER_BLOCKY_H

#include "../../util/godot/classes/mesh.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_mesher.h"
#include "voxel_blocky_library.h"

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

	void set_library(Ref<VoxelBlockyLibrary> library);
	Ref<VoxelBlockyLibrary> get_library() const;

	void set_occlusion_darkness(float darkness);
	float get_occlusion_darkness() const;

	void set_occlusion_enabled(bool enable);
	bool get_occlusion_enabled() const;

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	Ref<Resource> duplicate(bool p_subresources = false) const ZN_OVERRIDE_UNLESS_GODOT_EXTENSION;

	int get_used_channels_mask() const override;

	bool supports_lod() const override {
		return false;
	}

	Ref<Material> get_material_by_index(unsigned int index) const override;

	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		std::vector<Vector3f> positions;
		std::vector<Vector3f> normals;
		std::vector<Vector2f> uvs;
		std::vector<Color> colors;
		std::vector<int> indices;
		std::vector<float> tangents;

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
		Ref<VoxelBlockyLibrary> library;
	};

	struct Cache {
		std::vector<Arrays> arrays_per_material;
	};

	// Parameters
	Parameters _parameters;
	RWLock _parameters_lock;

	// Work cache
	static Cache &get_tls_cache();
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_BLOCKY_H
