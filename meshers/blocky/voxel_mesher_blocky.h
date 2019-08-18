#ifndef VOXEL_MESHER_BLOCKY_H
#define VOXEL_MESHER_BLOCKY_H

#include "../../util/zprofiling.h"
#include "../../voxel.h"
#include "../../voxel_library.h"
#include "../voxel_mesher.h"
#include <core/reference.h>
#include <scene/resources/mesh.h>
#include <vector>

class VoxelMesherBlocky : public VoxelMesher {
	GDCLASS(VoxelMesherBlocky, VoxelMesher)

public:
	static const unsigned int MAX_MATERIALS = 8; // Arbitrary. Tweak if needed.
	static const int MINIMUM_PADDING = 1;

	VoxelMesherBlocky();

	void set_library(Ref<VoxelLibrary> library);
	Ref<VoxelLibrary> get_library() const { return _library; }

	void set_occlusion_darkness(float darkness);
	float get_occlusion_darkness() const { return _baked_occlusion_darkness; }

	void set_occlusion_enabled(bool enable);
	bool get_occlusion_enabled() const { return _bake_occlusion; }

	void build(VoxelMesher::Output &output, const VoxelBuffer &voxels, int padding) override;
	int get_minimum_padding() const override;

	VoxelMesher *clone() override;

protected:
	static void _bind_methods();

private:
	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		std::vector<Vector3> positions;
		std::vector<Vector3> normals;
		std::vector<Vector2> uvs;
		std::vector<Color> colors;
		std::vector<int> indices;
	};

	Ref<VoxelLibrary> _library;
	Arrays _arrays[MAX_MATERIALS];
	float _baked_occlusion_darkness;
	bool _bake_occlusion;
};

#endif // VOXEL_MESHER_BLOCKY_H
