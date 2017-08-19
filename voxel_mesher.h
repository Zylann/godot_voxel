#ifndef VOXEL_MESHER
#define VOXEL_MESHER

#include "voxel.h"
#include "voxel_buffer.h"
#include "voxel_library.h"
#include "zprofiling.h"
#include <reference.h>
#include <scene/resources/mesh.h>

// TODO Should be renamed VoxelMesherCubic or something like that
class VoxelMesher : public Reference {
	GDCLASS(VoxelMesher, Reference)

public:
	static const unsigned int MAX_MATERIALS = 8; // Arbitrary. Tweak if needed.

	VoxelMesher();

	void set_material(Ref<Material> material, unsigned int id);
	Ref<Material> get_material(unsigned int id) const;

	void set_library(Ref<VoxelLibrary> library);
	Ref<VoxelLibrary> get_library() const { return _library; }

	void set_occlusion_darkness(float darkness);
	float get_occlusion_darkness() const { return _baked_occlusion_darkness; }

	void set_occlusion_enabled(bool enable);
	bool get_occlusion_enabled() const { return _bake_occlusion; }

	Ref<ArrayMesh> build(const VoxelBuffer &buffer_ref, unsigned int channel, Vector3i min, Vector3i max, Ref<ArrayMesh> mesh = Ref<Mesh>());
	Ref<ArrayMesh> build_ref(Ref<VoxelBuffer> buffer_ref, unsigned int channel, Ref<ArrayMesh> mesh = Ref<ArrayMesh>());

protected:
	static void _bind_methods();

private:
	struct Arrays {
		Vector<Vector3> positions;
		Vector<Vector3> normals;
		Vector<Vector2> uvs;
		Vector<Color> colors;
		Vector<int> indices;
	};

	Ref<VoxelLibrary> _library;
	Ref<Material> _materials[MAX_MATERIALS];
	Arrays _arrays[MAX_MATERIALS];
	float _baked_occlusion_darkness;
	bool _bake_occlusion;

#ifdef VOXEL_PROFILING
	ZProfiler _zprofiler;
	Dictionary get_profiling_info() const { return _zprofiler.get_all_serialized_info(); }
#endif
};

#endif // VOXEL_MESHER
