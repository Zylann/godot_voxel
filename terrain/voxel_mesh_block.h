#ifndef VOXEL_MESH_BLOCK_H
#define VOXEL_MESH_BLOCK_H

#include "../constants/cube_tables.h"
#include "../meshers/voxel_mesher.h"
#include "../util/containers/fixed_array.h"
#include "../util/containers/span.h"
#include "../util/godot/classes/world_3d.h"
#include "../util/godot/direct_mesh_instance.h"
#include "../util/godot/direct_static_body.h"
#include "../util/ref_count.h"

#include <atomic>

ZN_GODOT_FORWARD_DECLARE(class Node3D);
ZN_GODOT_FORWARD_DECLARE(class ConcavePolygonShape3D);

namespace zylann::voxel {

// Stores mesh and collider for one chunk of the rendered volume.
// It doesn't store voxel data, because it may be using different block size, or different data structure.
// IMPORTANT: This is not an abstract class. It exists to share common code between variants of it.
// Only explicit instances are used, no virtuals.
class VoxelMeshBlock : public NonCopyable {
public:
	Vector3i position; // In blocks

protected:
	VoxelMeshBlock(Vector3i bpos);

public:
	~VoxelMeshBlock();

	void set_world(Ref<World3D> p_world);

	// Visuals

	void set_mesh(Ref<Mesh> mesh, GeometryInstance3D::GIMode gi_mode,
			RenderingServer::ShadowCastingSetting shadow_setting, int render_layers_mask);
	Ref<Mesh> get_mesh() const;
	bool has_mesh() const;
	void drop_mesh();

	// Note, GIMode is not stored per block, it is a shared option so we provide it in several functions.
	// Call this function only if the mesh block already exists and has not changed mesh
	void set_gi_mode(GeometryInstance3D::GIMode mode);

	// Note, ShadowCastingSetting is not stored per block, it is a shared option so we provide it in several functions.
	// Call this function only if the mesh block already exists and has not changed mesh
	void set_shadow_casting(RenderingServer::ShadowCastingSetting setting);

	// Note, render layers is not stored per block, it is a shared option so we provide it in several functions.
	// Call this function only if the mesh block already exists and has not changed mesh
	void set_render_layers_mask(int mask);

	void set_visible(bool visible);
	bool is_visible() const;

	void set_parent_visible(bool parent_visible);
	void set_parent_transform(const Transform3D &parent_transform);

	// Collisions

	void set_collision_shape(Ref<Shape3D> shape, bool debug_collision, Node3D *node, float margin);
	bool has_collision_shape() const;
	void set_collision_layer(int layer);
	void set_collision_mask(int mask);
	void set_collision_margin(float margin);
	void drop_collision();
	// TODO Collision layer and mask

	void set_collision_enabled(bool enable);
	bool is_collision_enabled() const;

protected:
	void _set_visible(bool visible);

	inline void set_mesh_instance_visible(zylann::godot::DirectMeshInstance &mi, bool visible) {
		if (visible) {
			mi.set_world(*_world);
		} else {
			mi.set_world(nullptr);
		}
	}

	Vector3i _position_in_voxels;

	zylann::godot::DirectMeshInstance _mesh_instance;
	zylann::godot::DirectStaticBody _static_body;
	Ref<World3D> _world;

	// Must match default value of `active`
	bool _visible = false;
	bool _collision_enabled = false;

	bool _parent_visible = true;
};

Ref<ConcavePolygonShape3D> make_collision_shape_from_mesher_output(
		const VoxelMesher::Output &mesher_output, const VoxelMesher &mesher);

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_H
