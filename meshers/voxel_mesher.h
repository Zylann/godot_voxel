#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "../util/containers/span.h"
#include "../util/macros.h"
#include "voxel_mesher_input.h"
#include "voxel_mesher_output.h"

ZN_GODOT_FORWARD_DECLARE(class ShaderMaterial)

namespace zylann::voxel {

namespace godot {
class VoxelBuffer;
}

// Base class for algorithms that generate meshes from voxels.
class VoxelMesher : public Resource {
	GDCLASS(VoxelMesher, Resource)
public:
	static bool is_mesh_empty(const StdVector<VoxelMesherOutput::Surface> &surfaces);

	// This can be called from multiple threads at once. Make sure member vars are protected or thread-local.
	virtual void build(VoxelMesherOutput &output, const VoxelMesherInput &voxels);

	// Builds a mesh from the given voxels. This function is simplified to be used by the script API.
	Ref<Mesh> build_mesh(const VoxelBuffer &voxels, TypedArray<Material> materials, Dictionary additional_data);

	// TODO Rename "positive" and "negative" padding

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward negative axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_minimum_padding() const;

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward positive axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_maximum_padding() const;

	// Gets which channels this mesher is able to use in its current configuration.
	// This is returned as a bitmask where channel index corresponds to bit position.
	virtual int get_used_channels_mask() const {
		return 0;
	}

	// Returns true if this mesher supports generating voxel data at multiple levels of detail.
	virtual bool supports_lod() const {
		return true;
	}

	// Some meshers can provide materials themselves. The index may come from the built output. Returns null if the
	// index does not have a material assigned. If not provided here, a default material may be used.
	// An error can be produced if the index is out of bounds.
	virtual Ref<Material> get_material_by_index(unsigned int i) const;
	// Get the highest+1 material index
	virtual unsigned int get_material_index_count() const;

#ifdef TOOLS_ENABLED
	// If the mesher has problems, messages may be returned by this method so they can be shown to the user.
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const {}
#endif

	// Returns `true` if the mesher generates specific data for mesh collisions, which will be found in
	// `CollisionSurface`.
	// If `false`, the rendering mesh may be used as collider.
	virtual bool is_generating_collision_surface() const {
		return false;
	}

	// Gets a special default material to be used to render meshes produced with this mesher, when variable level of
	// detail is used. If null, standard materials or default Godot shaders can be used. This is mostly to provide a
	// default shader that looks ok. Users are still expected to tweak them if need be.
	// Such material is not meant to be modified.
	virtual Ref<ShaderMaterial> get_default_lod_material() const;

protected:
	Ref<Mesh> _b_build_mesh(Ref<godot::VoxelBuffer> voxels, TypedArray<Material> materials, Dictionary additional_data);
	static void _bind_methods();

	void set_padding(int minimum, int maximum);

private:
	// Set in constructor and never changed after.
	unsigned int _minimum_padding = 0;
	unsigned int _maximum_padding = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_H
