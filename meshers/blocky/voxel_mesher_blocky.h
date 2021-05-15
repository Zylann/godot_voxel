#ifndef VOXEL_MESHER_BLOCKY_H
#define VOXEL_MESHER_BLOCKY_H

#include "../voxel_mesher.h"
#include "voxel_library.h"
#include <core/reference.h>
#include <scene/resources/mesh.h>
#include <vector>

// TODO Rename VoxelMesherModelBatch

// Interprets voxel values as indexes to models in a VoxelLibrary, and batches them together.
// Overlapping faces are removed from the final mesh.
class VoxelMesherBlocky : public VoxelMesher {
	GDCLASS(VoxelMesherBlocky, VoxelMesher)

public:
	static const unsigned int MAX_MATERIALS = 8; // Arbitrary. Tweak if needed.
	static const int PADDING = 1;

	VoxelMesherBlocky();
	~VoxelMesherBlocky();

	void set_library(Ref<VoxelLibrary> library);
	Ref<VoxelLibrary> get_library() const;

	void set_occlusion_darkness(float darkness);
	float get_occlusion_darkness() const;

	void set_occlusion_enabled(bool enable);
	bool get_occlusion_enabled() const;

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	Ref<Resource> duplicate(bool p_subresources = false) const override;
	int get_used_channels_mask() const override;

	bool supports_lod() const override { return false; }

	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		std::vector<Vector3> positions;
		std::vector<Vector3> normals;
		std::vector<Vector2> uvs;
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

protected:
	static void _bind_methods();

private:
	struct Parameters {
		float baked_occlusion_darkness = 0.8;
		bool bake_occlusion = true;
		Ref<VoxelLibrary> library;
	};

	struct Cache {
		FixedArray<Arrays, MAX_MATERIALS> arrays_per_material;
	};

	// Parameters
	Parameters _parameters;
	RWLock _parameters_lock;

	// Work cache
	static thread_local Cache _cache;
};

#endif // VOXEL_MESHER_BLOCKY_H
