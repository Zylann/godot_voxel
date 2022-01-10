#ifndef VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_H

#include "voxel_instance_library_item.h"
#include <scene/resources/mesh.h>
#include <scene/resources/shape_3d.h>

namespace zylann::voxel {

// Settings for a model that can be used by VoxelInstancer
class VoxelInstanceLibraryMultiMeshItem : public VoxelInstanceLibraryItem {
	GDCLASS(VoxelInstanceLibraryMultiMeshItem, VoxelInstanceLibraryItem)
public:
	static const int MAX_MESH_LODS = 4;

	struct CollisionShapeInfo {
		Transform3D transform;
		Ref<Shape3D> shape;
	};

	void set_mesh(Ref<Mesh> mesh, int mesh_lod_index);
	Ref<Mesh> get_mesh(int mesh_lod_index) const;
	int get_mesh_lod_count() const;

	void set_material_override(Ref<Material> material);
	Ref<Material> get_material_override() const;

	void set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode);
	RenderingServer::ShadowCastingSetting get_cast_shadows_setting() const;

	void set_collision_layer(int collision_layer);
	int get_collision_layer() const;

	void set_collision_mask(int collision_mask);
	int get_collision_mask() const;

	void setup_from_template(Node *root);

	// Internal

	inline const Vector<CollisionShapeInfo> &get_collision_shapes() const {
		return _collision_shapes;
	}

	Array serialize_multimesh_item_properties() const;
	void deserialize_multimesh_item_properties(Array a);

private:
	static void _bind_methods();

	void _b_set_collision_shapes(Array shape_infos);
	Array _b_get_collision_shapes() const;

	Ref<Mesh> _b_get_mesh_lod0() const {
		return get_mesh(0);
	}
	Ref<Mesh> _b_get_mesh_lod1() const {
		return get_mesh(1);
	}
	Ref<Mesh> _b_get_mesh_lod2() const {
		return get_mesh(2);
	}
	Ref<Mesh> _b_get_mesh_lod3() const {
		return get_mesh(3);
	}

	void _b_set_mesh_lod0(Ref<Mesh> mesh) {
		set_mesh(mesh, 0);
	}
	void _b_set_mesh_lod1(Ref<Mesh> mesh) {
		set_mesh(mesh, 1);
	}
	void _b_set_mesh_lod2(Ref<Mesh> mesh) {
		set_mesh(mesh, 2);
	}
	void _b_set_mesh_lod3(Ref<Mesh> mesh) {
		set_mesh(mesh, 3);
	}

	FixedArray<Ref<Mesh>, MAX_MESH_LODS> _mesh_lods;
	unsigned int _mesh_lod_count = 1;

	// It is preferred to have materials on the mesh already,
	// but this is in case OBJ meshes are used, which often dont have a material of their own
	Ref<Material> _material_override;

	RenderingServer::ShadowCastingSetting _shadow_casting_setting = RenderingServer::SHADOW_CASTING_SETTING_ON;

	int _collision_mask = 1;
	int _collision_layer = 1;
	Vector<CollisionShapeInfo> _collision_shapes;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_MULTIMESH_ITEM_H
