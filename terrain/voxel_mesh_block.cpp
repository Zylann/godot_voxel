#include "voxel_mesh_block.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"
#include "../util/profiling.h"

#include <scene/3d/spatial.h>
#include <scene/resources/concave_polygon_shape.h>

VoxelMeshBlock *VoxelMeshBlock::create(Vector3i bpos, unsigned int size, unsigned int p_lod_index) {
	VoxelMeshBlock *block = memnew(VoxelMeshBlock);
	block->position = bpos;
	block->lod_index = p_lod_index;
	block->_position_in_voxels = bpos * (size << p_lod_index);

#ifdef VOXEL_DEBUG_LOD_MATERIALS
	Ref<SpatialMaterial> debug_material;
	debug_material.instance();
	int checker = (bpos.x + bpos.y + bpos.z) & 1;
	Color debug_color = Color(0.8, 0.4, 0.8).linear_interpolate(Color(0.0, 0.0, 0.5), static_cast<float>(p_lod_index) / 8.f);
	debug_color = debug_color.lightened(checker * 0.1f);
	debug_material->set_albedo(debug_color);
	block->_debug_material = debug_material;

	Ref<SpatialMaterial> debug_transition_material;
	debug_transition_material.instance();
	debug_transition_material->set_albedo(Color(1, 1, 0));
	block->_debug_transition_material = debug_transition_material;
#endif

	return block;
}

VoxelMeshBlock::VoxelMeshBlock() {
}

VoxelMeshBlock::~VoxelMeshBlock() {
}

void VoxelMeshBlock::set_world(Ref<World> p_world) {
	if (_world != p_world) {
		_world = p_world;

		// To update world. I replaced visibility by presence in world because Godot 3 culling performance is horrible
		_set_visible(_visible && _parent_visible);

		if (_static_body.is_valid()) {
			_static_body.set_world(*p_world);
		}
	}
}

void VoxelMeshBlock::set_mesh(Ref<Mesh> mesh) {
	// TODO Don't add mesh instance to the world if it's not visible.
	// I suspect Godot is trying to include invisible mesh instances into the culling process,
	// which is killing performance when LOD is used (i.e many meshes are in pool but hidden)
	// This needs investigation.

	if (mesh.is_valid()) {
		if (!_mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			_mesh_instance.create();
			set_mesh_instance_visible(_mesh_instance, _visible && _parent_visible);
			_mesh_instance.set_portal_mode(VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);
		}

		_mesh_instance.set_mesh(mesh);

		if (_shader_material.is_valid()) {
			_mesh_instance.set_material_override(_shader_material);
		}
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

void VoxelMeshBlock::set_transition_mesh(Ref<Mesh> mesh, int side) {
	DirectMeshInstance &mesh_instance = _transition_mesh_instances[side];

	if (mesh.is_valid()) {
		if (!mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			mesh_instance.create();
			set_mesh_instance_visible(mesh_instance, _visible && _parent_visible && _is_transition_visible(side));
			mesh_instance.set_portal_mode(VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);
		}

		mesh_instance.set_mesh(mesh);

		if (_shader_material.is_valid()) {
			mesh_instance.set_material_override(_shader_material);
		}
#ifdef VOXEL_DEBUG_LOD_MATERIALS
		mesh_instance.set_material_override(_debug_transition_material);
#endif

	} else {
		if (mesh_instance.is_valid()) {
			// Delete instance if it exists
			mesh_instance.destroy();
		}
	}
}

bool VoxelMeshBlock::has_mesh() const {
	return _mesh_instance.get_mesh().is_valid();
}

void VoxelMeshBlock::drop_mesh() {
	if (_mesh_instance.is_valid()) {
		_mesh_instance.destroy();
	}
}

void VoxelMeshBlock::set_mesh_state(MeshState ms) {
	_mesh_state = ms;
}

VoxelMeshBlock::MeshState VoxelMeshBlock::get_mesh_state() const {
	return _mesh_state;
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
	for (unsigned int dir = 0; dir < _transition_mesh_instances.size(); ++dir) {
		DirectMeshInstance &mi = _transition_mesh_instances[dir];
		if (mi.is_valid()) {
			set_mesh_instance_visible(mi, visible && _is_transition_visible(dir));
		}
	}
	if (_static_body.is_valid()) {
		_static_body.set_shape_enabled(0, visible);
	}
}

void VoxelMeshBlock::set_shader_material(Ref<ShaderMaterial> material) {
	_shader_material = material;

	if (_mesh_instance.is_valid()) {
		_mesh_instance.set_material_override(_shader_material);

		for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			DirectMeshInstance &mi = _transition_mesh_instances[dir];
			if (mi.is_valid()) {
				mi.set_material_override(_shader_material);
			}
		}
	}

	if (_shader_material.is_valid()) {
		const Transform local_transform(Basis(), _position_in_voxels.to_vec3());
		_shader_material->set_shader_param(VoxelStringNames::get_singleton()->u_block_local_transform, local_transform);
	}
}

//void VoxelMeshBlock::set_transition_bit(uint8_t side, bool value) {
//	CRASH_COND(side >= Cube::SIDE_COUNT);
//	uint32_t m = _transition_mask;
//	if (value) {
//		m |= (1 << side);
//	} else {
//		m &= ~(1 << side);
//	}
//	set_transition_mask(m);
//}

void VoxelMeshBlock::set_transition_mask(uint8_t m) {
	CRASH_COND(m >= (1 << Cube::SIDE_COUNT));
	const uint8_t diff = _transition_mask ^ m;
	if (diff == 0) {
		return;
	}
	_transition_mask = m;
	if (_shader_material.is_valid()) {
		// TODO Needs translation here, because Cube:: tables use slightly different order...
		// We may get rid of this once cube tables respects -x+x-y+y-z+z order
		uint8_t bits[Cube::SIDE_COUNT];
		for (unsigned int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			bits[dir] = (m >> dir) & 1;
		}
		uint8_t tm = bits[Cube::SIDE_NEGATIVE_X];
		tm |= bits[Cube::SIDE_POSITIVE_X] << 1;
		tm |= bits[Cube::SIDE_NEGATIVE_Y] << 2;
		tm |= bits[Cube::SIDE_POSITIVE_Y] << 3;
		tm |= bits[Cube::SIDE_NEGATIVE_Z] << 4;
		tm |= bits[Cube::SIDE_POSITIVE_Z] << 5;

		// TODO Godot 4: we may replace this with a per-instance parameter so we can lift material access limitation
		_shader_material->set_shader_param(VoxelStringNames::get_singleton()->u_transition_mask, tm);
	}
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		DirectMeshInstance &mi = _transition_mesh_instances[dir];
		if ((diff & (1 << dir)) && mi.is_valid()) {
			set_mesh_instance_visible(mi, _visible && _parent_visible && _is_transition_visible(dir));
		}
	}
}

void VoxelMeshBlock::set_parent_visible(bool parent_visible) {
	if (_parent_visible && parent_visible) {
		return;
	}
	_parent_visible = parent_visible;
	_set_visible(_visible && _parent_visible);
}

void VoxelMeshBlock::set_parent_transform(const Transform &parent_transform) {
	VOXEL_PROFILE_SCOPE();

	if (_mesh_instance.is_valid() || _static_body.is_valid()) {
		const Transform local_transform(Basis(), _position_in_voxels.to_vec3());
		const Transform world_transform = parent_transform * local_transform;

		if (_mesh_instance.is_valid()) {
			_mesh_instance.set_transform(world_transform);

			for (unsigned int i = 0; i < _transition_mesh_instances.size(); ++i) {
				DirectMeshInstance &mi = _transition_mesh_instances[i];
				if (mi.is_valid()) {
					mi.set_transform(world_transform);
				}
			}
		}

		if (_static_body.is_valid()) {
			_static_body.set_transform(world_transform);
		}
	}
}

void VoxelMeshBlock::set_collision_mesh(Vector<Array> surface_arrays, bool debug_collision, Spatial *node, float margin) {
	if (surface_arrays.size() == 0) {
		drop_collision();
		return;
	}

	ERR_FAIL_COND(node == nullptr);
	ERR_FAIL_COND_MSG(node->get_world() != _world, "Physics body and attached node must be from the same world");

	Ref<Shape> shape = create_concave_polygon_shape(surface_arrays);
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
		Ref<Shape> shape = _static_body.get_shape(0);
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

// Returns `true` when finished
bool VoxelMeshBlock::update_fading(float speed) {
	// TODO Should probably not be on the block directly?
	// Because we may want to fade transition meshes only

	bool finished = false;

	// x is progress in 0..1
	// y is direction: 1 fades in, 0 fades out
	Vector2 p;

	switch (fading_state) {
		case FADING_IN:
			fading_progress += speed;
			if (fading_progress >= 1.f) {
				fading_progress = 1.f;
				fading_state = FADING_NONE;
				finished = true;
			}
			p.x = fading_progress;
			p.y = 1.f;
			break;

		case FADING_OUT:
			fading_progress -= speed;
			if (fading_progress < 0.f) {
				fading_progress = 0.f;
				fading_state = FADING_NONE;
				finished = true;
				set_visible(false);
			}
			p.x = 1.f - fading_progress;
			p.y = 0.f;
			break;

		case FADING_NONE:
			p.x = 1.f;
			p.y = active ? 1.f : 0.f;
			break;

		default:
			CRASH_NOW();
			break;
	}

	if (_shader_material.is_valid()) {
		_shader_material->set_shader_param(VoxelStringNames::get_singleton()->u_lod_fade, p);
	}

	return finished;
}
