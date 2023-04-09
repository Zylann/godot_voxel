#include "voxel_mesh_block.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/classes/collision_shape_3d.h"
#include "../util/godot/classes/concave_polygon_shape_3d.h"
#include "../util/godot/classes/node_3d.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "free_mesh_task.h"

namespace zylann::voxel {

VoxelMeshBlock::VoxelMeshBlock(Vector3i bpos) {
	position = bpos;
}

VoxelMeshBlock::~VoxelMeshBlock() {
	FreeMeshTask::try_add_and_destroy(_mesh_instance);
}

void VoxelMeshBlock::set_world(Ref<World3D> p_world) {
	if (_world != p_world) {
		_world = p_world;

		// To update world. I replaced visibility by presence in world because Godot 3 culling performance is horrible
		_set_visible(_visible && _parent_visible);

		if (_static_body.is_valid()) {
			_static_body.set_world(*p_world);
		}
	}
}

void VoxelMeshBlock::set_gi_mode(DirectMeshInstance::GIMode mode) {
	if (_mesh_instance.is_valid()) {
		_mesh_instance.set_gi_mode(mode);
	}
}

void VoxelMeshBlock::set_shadow_casting(RenderingServer::ShadowCastingSetting setting) {
	if (_mesh_instance.is_valid()) {
		_mesh_instance.set_cast_shadows_setting(setting);
	}
}

void VoxelMeshBlock::set_mesh(
		Ref<Mesh> mesh, DirectMeshInstance::GIMode gi_mode, RenderingServer::ShadowCastingSetting setting) {
	// TODO Don't add mesh instance to the world if it's not visible.
	// I suspect Godot is trying to include invisible mesh instances into the culling process,
	// which is killing performance when LOD is used (i.e many meshes are in pool but hidden)
	// This needs investigation.

	if (mesh.is_valid()) {
		if (!_mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			_mesh_instance.create();
			_mesh_instance.set_gi_mode(gi_mode);
			_mesh_instance.set_cast_shadows_setting(setting);
			set_mesh_instance_visible(_mesh_instance, _visible && _parent_visible);
		}

		_mesh_instance.set_mesh(mesh);

#ifdef VOXEL_DEBUG_LOD_MATERIALS
		_mesh_instance.set_material_override(_debug_material);
#endif

	} else {
		if (_mesh_instance.is_valid()) {
			// Delete instance if it exists
			_mesh_instance.destroy();
		}
	}
}

Ref<Mesh> VoxelMeshBlock::get_mesh() const {
	if (_mesh_instance.is_valid()) {
		return _mesh_instance.get_mesh();
	}
	return Ref<Mesh>();
}

bool VoxelMeshBlock::has_mesh() const {
	return _mesh_instance.get_mesh().is_valid();
}

void VoxelMeshBlock::drop_mesh() {
	if (_mesh_instance.is_valid()) {
		_mesh_instance.destroy();
	}
}

void VoxelMeshBlock::set_visible(bool visible) {
	if (_visible == visible) {
		return;
	}
	_visible = visible;
	_set_visible(_visible && _parent_visible);
}

bool VoxelMeshBlock::is_visible() const {
	return _visible;
}

void VoxelMeshBlock::_set_visible(bool visible) {
	if (_mesh_instance.is_valid()) {
		set_mesh_instance_visible(_mesh_instance, visible);
	}
	if (_static_body.is_valid()) {
		_static_body.set_shape_enabled(0, visible);
	}
}

void VoxelMeshBlock::set_parent_visible(bool parent_visible) {
	if (_parent_visible && parent_visible) {
		return;
	}
	_parent_visible = parent_visible;
	_set_visible(_visible && _parent_visible);
}

void VoxelMeshBlock::set_parent_transform(const Transform3D &parent_transform) {
	ZN_PROFILE_SCOPE();

	if (_mesh_instance.is_valid() || _static_body.is_valid()) {
		const Transform3D local_transform(Basis(), _position_in_voxels);
		const Transform3D world_transform = parent_transform * local_transform;

		if (_mesh_instance.is_valid()) {
			_mesh_instance.set_transform(world_transform);
		}

		if (_static_body.is_valid()) {
			_static_body.set_transform(world_transform);
		}
	}
}

void VoxelMeshBlock::set_collision_shape(Ref<Shape3D> shape, bool debug_collision, Node3D *node, float margin) {
	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_COND_MSG(node->get_world_3d() != _world, "Physics body and attached node must be from the same world");

	if (shape.is_null()) {
		drop_collision();
		return;
	}

	if (!_static_body.is_valid()) {
		_static_body.create();
		_static_body.set_world(*_world);
		// This allows collision signals to provide the terrain node in the `collider` field
		_static_body.set_attached_object(node);

	} else {
		_static_body.remove_shape(0);
	}

	shape->set_margin(margin);

	_static_body.add_shape(shape);
	_static_body.set_debug(debug_collision, *_world);
	_static_body.set_shape_enabled(0, _visible);
}

void VoxelMeshBlock::set_collision_layer(int layer) {
	if (_static_body.is_valid()) {
		_static_body.set_collision_layer(layer);
	}
}

void VoxelMeshBlock::set_collision_mask(int mask) {
	if (_static_body.is_valid()) {
		_static_body.set_collision_mask(mask);
	}
}

void VoxelMeshBlock::set_collision_margin(float margin) {
	if (_static_body.is_valid()) {
		Ref<Shape3D> shape = _static_body.get_shape(0);
		if (shape.is_valid()) {
			shape->set_margin(margin);
		}
	}
}

void VoxelMeshBlock::drop_collision() {
	if (_static_body.is_valid()) {
		_static_body.destroy();
	}
}

Ref<ConcavePolygonShape3D> make_collision_shape_from_mesher_output(
		const VoxelMesher::Output &mesher_output, const VoxelMesher &mesher) {
	Ref<ConcavePolygonShape3D> shape;

	if (mesher.is_generating_collision_surface()) {
		if (mesher_output.collision_surface.submesh_vertex_end != -1) {
			// Use a sub-region of the render mesh
			if (mesher_output.surfaces.size() > 0) {
				shape = create_concave_polygon_shape(
						mesher_output.surfaces[0].arrays, mesher_output.collision_surface.submesh_index_end);
			}

		} else {
			// Use specialized collision mesh
			shape = create_concave_polygon_shape(to_span(mesher_output.collision_surface.positions),
					to_span(mesher_output.collision_surface.indices));
		}

	} else {
		// Use render mesh
		static thread_local std::vector<Array> tls_render_surfaces;
		ZN_ASSERT(tls_render_surfaces.size() == 0);

		for (unsigned int i = 0; i < mesher_output.surfaces.size(); ++i) {
			tls_render_surfaces.push_back(mesher_output.surfaces[i].arrays);
		}

		shape = create_concave_polygon_shape(to_span(tls_render_surfaces));

		tls_render_surfaces.clear();
	}

	return shape;
}

} // namespace zylann::voxel
