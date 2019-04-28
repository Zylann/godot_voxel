#ifndef VOXEL_MESHER_BLOCKY_H
#define VOXEL_MESHER_BLOCKY_H

#include "../../util/zprofiling.h"
#include "../../voxel.h"
#include "../../voxel_library.h"
#include "../voxel_mesher.h"
#include <core/reference.h>
#include <scene/resources/mesh.h>

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

protected:
	static void _bind_methods();

private:
	// TODO Replace those with std::vector, it's faster
	struct Arrays {
		Vector<Vector3> positions;
		Vector<Vector3> normals;
		Vector<Vector2> uvs;
		Vector<Color> colors;
		Vector<int> indices;
	};

	Ref<VoxelLibrary> _library;
	Arrays _arrays[MAX_MATERIALS];
	float _baked_occlusion_darkness;
	bool _bake_occlusion;

#ifdef VOXEL_PROFILING
	ZProfiler _zprofiler;
	Dictionary get_profiling_info() const { return _zprofiler.get_all_serialized_info(); }
#endif
};

#endif // VOXEL_MESHER_BLOCKY_H
