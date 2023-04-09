#include "voxel_mesh_block_vlt.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/mesh.h"
#include "../../util/profiling.h"
#include "../free_mesh_task.h"

namespace zylann::voxel {

VoxelMeshBlockVLT::VoxelMeshBlockVLT(const Vector3i bpos, unsigned int size, unsigned int p_lod_index) :
		VoxelMeshBlock(bpos) {
	_position_in_voxels = bpos * (size << p_lod_index);
	lod_index = p_lod_index;

#ifdef VOXEL_DEBUG_LOD_MATERIALS
	Ref<SpatialMaterial> debug_material;
	debug_material.instance();
	int checker = (bpos.x + bpos.y + bpos.z) & 1;
	Color debug_color =
			Color(0.8, 0.4, 0.8).linear_interpolate(Color(0.0, 0.0, 0.5), static_cast<float>(p_lod_index) / 8.f);
	debug_color = debug_color.lightened(checker * 0.1f);
	debug_material->set_albedo(debug_color);
	block->_debug_material = debug_material;

	Ref<SpatialMaterial> debug_transition_material;
	debug_transition_material.instance();
	debug_transition_material->set_albedo(Color(1, 1, 0));
	block->_debug_transition_material = debug_transition_material;
#endif
}

VoxelMeshBlockVLT::~VoxelMeshBlockVLT() {
	// Make sure no material override is set, because it's possible the material will get destroyed before the mesh
	// instance, which would cause errors in RenderingServer. Our thin wrapper does not take ownership of the material.
	// TODO Eventually it would be better if we could just unref the material after having destroyed the mesh...
	// VoxelMeshBlock inheritance isn't helping us here
	_mesh_instance.set_material_override(Ref<Material>());

	for (unsigned int i = 0; i < _transition_mesh_instances.size(); ++i) {
		DirectMeshInstance &tmi = _transition_mesh_instances[i];
		if (tmi.is_valid()) {
			tmi.set_material_override(Ref<Material>());
			FreeMeshTask::try_add_and_destroy(tmi);
		}
	}
}

void VoxelMeshBlockVLT::set_mesh(
		Ref<Mesh> mesh, DirectMeshInstance::GIMode gi_mode, RenderingServer::ShadowCastingSetting shadow_casting) {
	// TODO Don't add mesh instance to the world if it's not visible.
	// I suspect Godot is trying to include invisible mesh instances into the culling process,
	// which is killing performance when LOD is used (i.e many meshes are in pool but hidden)
	// This needs investigation.

	if (mesh.is_valid()) {
		if (!_mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			_mesh_instance.create();
			_mesh_instance.set_gi_mode(gi_mode);
			_mesh_instance.set_cast_shadows_setting(shadow_casting);
			set_mesh_instance_visible(_mesh_instance, _visible && _parent_visible);
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

void VoxelMeshBlockVLT::set_gi_mode(DirectMeshInstance::GIMode mode) {
	VoxelMeshBlock::set_gi_mode(mode);
	for (unsigned int i = 0; i < _transition_mesh_instances.size(); ++i) {
		DirectMeshInstance &mi = _transition_mesh_instances[i];
		if (mi.is_valid()) {
			mi.set_gi_mode(mode);
		}
	}
}

void VoxelMeshBlockVLT::set_shadow_casting(RenderingServer::ShadowCastingSetting mode) {
	VoxelMeshBlock::set_shadow_casting(mode);
	for (unsigned int i = 0; i < _transition_mesh_instances.size(); ++i) {
		DirectMeshInstance &mi = _transition_mesh_instances[i];
		if (mi.is_valid()) {
			mi.set_cast_shadows_setting(mode);
		}
	}
}

void VoxelMeshBlockVLT::set_transition_mesh(Ref<Mesh> mesh, unsigned int side, DirectMeshInstance::GIMode gi_mode,
		RenderingServer::ShadowCastingSetting shadow_casting) {
	DirectMeshInstance &mesh_instance = _transition_mesh_instances[side];

	if (mesh.is_valid()) {
		if (!mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			mesh_instance.create();
			mesh_instance.set_gi_mode(gi_mode);
			mesh_instance.set_cast_shadows_setting(shadow_casting);
			set_mesh_instance_visible(mesh_instance, _visible && _parent_visible && _is_transition_visible(side));
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

void VoxelMeshBlockVLT::set_world(Ref<World3D> p_world) {
	if (_world != p_world) {
		_world = p_world;

		// To update world. I replaced visibility by presence in world because Godot 3 culling performance is horrible
		_set_visible(_visible && _parent_visible);

		if (_static_body.is_valid()) {
			_static_body.set_world(*p_world);
		}
	}
}

void VoxelMeshBlockVLT::set_visible(bool visible) {
	if (_visible == visible) {
		return;
	}
	_visible = visible;
	_set_visible(_visible && _parent_visible);
}

void VoxelMeshBlockVLT::_set_visible(bool visible) {
	VoxelMeshBlock::_set_visible(visible);
	for (unsigned int dir = 0; dir < _transition_mesh_instances.size(); ++dir) {
		DirectMeshInstance &mi = _transition_mesh_instances[dir];
		if (mi.is_valid()) {
			set_mesh_instance_visible(mi, visible && _is_transition_visible(dir));
		}
	}
}

void VoxelMeshBlockVLT::set_shader_material(Ref<ShaderMaterial> material) {
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
		const Transform3D local_transform(Basis(), _position_in_voxels);
		const VoxelStringNames &sn = VoxelStringNames::get_singleton();
		_shader_material->set_shader_parameter(sn.u_block_local_transform, local_transform);
		_shader_material->set_shader_parameter(sn.u_voxel_virtual_texture_offset_scale, Vector4(0, 0, 0, 1));
	}
}

// void VoxelMeshBlock::set_transition_bit(uint8_t side, bool value) {
//	CRASH_COND(side >= Cube::SIDE_COUNT);
//	uint32_t m = _transition_mask;
//	if (value) {
//		m |= (1 << side);
//	} else {
//		m &= ~(1 << side);
//	}
//	set_transition_mask(m);
// }

void VoxelMeshBlockVLT::set_transition_mask(uint8_t m) {
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
		_shader_material->set_shader_parameter(VoxelStringNames::get_singleton().u_transition_mask, tm);
	}
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		DirectMeshInstance &mi = _transition_mesh_instances[dir];
		if (mi.is_valid() && (diff & (1 << dir))) {
			set_mesh_instance_visible(mi, _visible && _parent_visible && _is_transition_visible(dir));
		}
	}
}

void VoxelMeshBlockVLT::set_parent_visible(bool parent_visible) {
	if (_parent_visible && parent_visible) {
		return;
	}
	_parent_visible = parent_visible;
	_set_visible(_visible && _parent_visible);
}

void VoxelMeshBlockVLT::set_parent_transform(const Transform3D &parent_transform) {
	ZN_PROFILE_SCOPE();

	if (_mesh_instance.is_valid() || _static_body.is_valid()) {
		// TODO Optimize: could be optimized due to the basis being identity
		const Transform3D local_transform(Basis(), _position_in_voxels);
		const Transform3D world_transform = parent_transform * local_transform;

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

void VoxelMeshBlockVLT::update_transition_mesh_transform(unsigned int side, const Transform3D &parent_transform) {
	DirectMeshInstance &mi = _transition_mesh_instances[side];
	if (mi.is_valid()) {
		// TODO Optimize: could be optimized due to the basis being identity
		const Transform3D local_transform(Basis(), _position_in_voxels);
		const Transform3D world_transform = parent_transform * local_transform;
		mi.set_transform(world_transform);
	}
}

// Returns `true` when finished
bool VoxelMeshBlockVLT::update_fading(float speed) {
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
		_shader_material->set_shader_parameter(VoxelStringNames::get_singleton().u_lod_fade, p);
	}

	return finished;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Ref<ArrayMesh> build_mesh(Span<const VoxelMesher::Output::Surface> surfaces, Mesh::PrimitiveType primitive, int flags,
		Ref<Material> material) {
	ZN_PROFILE_SCOPE();
	Ref<ArrayMesh> mesh;

	unsigned int surface_index = 0;
	for (unsigned int i = 0; i < surfaces.size(); ++i) {
		const VoxelMesher::Output::Surface &surface = surfaces[i];
		Array arrays = surface.arrays;

		if (arrays.is_empty()) {
			continue;
		}

		CRASH_COND(arrays.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(arrays)) {
			continue;
		}

		if (mesh.is_null()) {
			mesh.instantiate();
		}

		// TODO Use `add_surface`, it's about 20% faster after measuring in Tracy (though we may see if Godot 4 expects
		// the same)
		mesh->add_surface_from_arrays(primitive, arrays, Array(), Dictionary(), flags);
		mesh->surface_set_material(surface_index, material);
		// No multi-material supported yet
		++surface_index;
	}

	// Debug code to highlight vertex sharing
	/*if (mesh->get_surface_count() > 0) {
		Array wireframe_surface = generate_debug_seams_wireframe_surface(mesh, 0);
		if (wireframe_surface.size() > 0) {
			const int wireframe_surface_index = mesh->get_surface_count();
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, wireframe_surface);
			Ref<SpatialMaterial> line_material;
			line_material.instance();
			line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
			line_material->set_albedo(Color(1.0, 0.0, 1.0));
			mesh->surface_set_material(wireframe_surface_index, line_material);
		}
	}*/

	if (mesh.is_valid() && is_mesh_empty(**mesh)) {
		mesh = Ref<Mesh>();
	}

	return mesh;
}

} // namespace zylann::voxel
