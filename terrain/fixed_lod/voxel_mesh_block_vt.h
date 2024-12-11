#ifndef VOXEL_MESH_BLOCK_VT_H
#define VOXEL_MESH_BLOCK_VT_H

#include "../../util/godot/classes/material.h"
#include "../voxel_mesh_block.h"

namespace zylann::voxel {

// Stores mesh and collider for one chunk of `VoxelLodTerrain`.
// It doesn't store voxel data, because it may be using different block size, or different data structure.
//
// Note that such a block can also not contain a mesh, in case voxels in this area do not produce geometry. For example,
// it can be used to check if an area has been loaded (so we *know* that it should or should not have a mesh, as opposed
// to not knowing while threads are still computing the mesh).
class VoxelMeshBlockVT : public VoxelMeshBlock {
public:
	// See VoxelMesherBlocky.
	// This unfortunately has to be a whole separate mesh instance because Godot doesn't support setting
	// `cast_shadow` mode per mesh surface. This might have an impact on performance.
	zylann::godot::DirectMeshInstance shadow_occluder;

	RefCount mesh_viewers;
	RefCount collision_viewers;

	// True if this block is in the update list of `VoxelTerrain`, so multiple edits done before it processes will not
	// add it multiple times
	bool is_in_update_list = false;

	// Will be true if the block has ever been processed by meshing (regardless of there being a mesh or not).
	// This is needed to know if the area is loaded, in terms of collisions. If the game uses voxels directly for
	// collision, it may be a better idea to use `is_area_editable` and not use mesh blocks
	bool is_loaded = false;

	VoxelMeshBlockVT(const Vector3i bpos, unsigned int size) : VoxelMeshBlock(bpos) {
		_position_in_voxels = bpos * size;
	}

	void set_world(Ref<World3D> p_world) {
		if (_world != p_world) {
			_world = p_world;

			// To update world. I replaced visibility by presence in world because Godot 3 culling performance is
			// horrible
			_set_visible(_visible && _parent_visible);

			if (_static_body.is_valid()) {
				_static_body.set_world(*p_world);
			}
		}
	}

	void set_material_override(Ref<Material> material) {
		// Can be invalid if the mesh is empty, we don't create instances for empty meshes
		if (_mesh_instance.is_valid()) {
			_mesh_instance.set_material_override(material);
		}
	}

	void set_mesh(
			Ref<Mesh> mesh,
			GeometryInstance3D::GIMode gi_mode,
			RenderingServer::ShadowCastingSetting shadow_setting,
			int render_layers_mask,
			Ref<Mesh> shadow_occluder_mesh
#ifdef TOOLS_ENABLED
			,
			RenderingServer::ShadowCastingSetting shadow_occluder_mode
#endif
	) {
		if (shadow_occluder_mesh.is_null()) {
			if (shadow_occluder.is_valid()) {
				shadow_occluder.destroy();
			}
		} else {
			if (!shadow_occluder.is_valid()) {
				// Create instance if it doesn't exist
				shadow_occluder.create();
				shadow_occluder.set_render_layers_mask(render_layers_mask);
#ifdef TOOLS_ENABLED
				shadow_occluder.set_cast_shadows_setting(shadow_occluder_mode);
#else
				shadow_occluder.set_cast_shadows_setting(RenderingServer::SHADOW_CASTING_SETTING_SHADOWS_ONLY);
#endif
				set_mesh_instance_visible(shadow_occluder, _visible && _parent_visible);
			}
			shadow_occluder.set_mesh(shadow_occluder_mesh);
		}

		VoxelMeshBlock::set_mesh(mesh, gi_mode, shadow_setting, render_layers_mask);
	}

	void drop_mesh() {
		if (shadow_occluder.is_valid()) {
			shadow_occluder.destroy();
		}
		VoxelMeshBlock::drop_mesh();
	}

	void set_render_layers_mask(int mask) {
		if (shadow_occluder.is_valid()) {
			shadow_occluder.set_render_layers_mask(mask);
		}
		VoxelMeshBlock::set_render_layers_mask(mask);
	}

	void set_visible(bool visible) {
		if (_visible == visible) {
			return;
		}
		_visible = visible;
		_set_visible(_visible && _parent_visible);
	}

	void set_parent_visible(bool parent_visible) {
		if (_parent_visible && parent_visible) {
			return;
		}
		_parent_visible = parent_visible;
		_set_visible(_visible && _parent_visible);
	}

	void set_parent_transform(const Transform3D &parent_transform) {
		ZN_PROFILE_SCOPE();

		if (shadow_occluder.is_valid()) {
			const Transform3D local_transform(Basis(), _position_in_voxels);
			const Transform3D world_transform = parent_transform * local_transform;

			if (shadow_occluder.is_valid()) {
				shadow_occluder.set_transform(world_transform);
			}
		}

		VoxelMeshBlock::set_parent_transform(parent_transform);
	}

protected:
	void _set_visible(bool visible) {
		if (shadow_occluder.is_valid()) {
			set_mesh_instance_visible(shadow_occluder, visible);
		}
		VoxelMeshBlock::_set_visible(visible);
	}
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_VT_H
