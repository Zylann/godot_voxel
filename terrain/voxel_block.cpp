#include "voxel_block.h"
#include "../util/zprofiling.h"
#include "../voxel_string_names.h"
#include <scene/3d/spatial.h>
#include <scene/resources/concave_polygon_shape.h>

// Faster version of Mesh::create_trimesh_shape()
// See https://github.com/Zylann/godot_voxel/issues/54
//
static Ref<ConcavePolygonShape> create_concave_polygon_shape(Vector<Array> surfaces) {

	PoolVector<Vector3> face_points;
	int face_points_size = 0;

	//find the correct size for face_points
	for (int i = 0; i < surfaces.size(); i++) {
		const Array &surface_arrays = surfaces[i];
		PoolVector<int> indices = surface_arrays[Mesh::ARRAY_INDEX];

		face_points_size += indices.size();
	}
	face_points.resize(face_points_size);

	//copy the points into it
	int face_points_offset = 0;
	for (int i = 0; i < surfaces.size(); i++) {
		const Array &surface_arrays = surfaces[i];

		PoolVector<Vector3> positions = surface_arrays[Mesh::ARRAY_VERTEX];
		PoolVector<int> indices = surface_arrays[Mesh::ARRAY_INDEX];

		ERR_FAIL_COND_V(positions.size() < 3, Ref<ConcavePolygonShape>());
		ERR_FAIL_COND_V(indices.size() < 3, Ref<ConcavePolygonShape>());
		ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ConcavePolygonShape>());

		int face_points_count = face_points_offset + indices.size();

		{
			PoolVector<Vector3>::Write w = face_points.write();
			PoolVector<int>::Read index_r = indices.read();
			PoolVector<Vector3>::Read position_r = positions.read();

			for (int p = face_points_offset; p < face_points_count; ++p) {
				w[p] = position_r[index_r[p - face_points_offset]];
			}
		}

		face_points_offset += indices.size();
	}

	Ref<ConcavePolygonShape> shape = memnew(ConcavePolygonShape);
	shape->set_faces(face_points);
	return shape;
}

// Helper
VoxelBlock *VoxelBlock::create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index) {
	const int bs = size;
	ERR_FAIL_COND_V(buffer.is_null(), nullptr);
	ERR_FAIL_COND_V(buffer->get_size() != Vector3i(bs, bs, bs), nullptr);

	VoxelBlock *block = memnew(VoxelBlock);
	block->position = bpos;
	block->lod_index = p_lod_index;
	block->_position_in_voxels = bpos * (size << p_lod_index);
	block->voxels = buffer;

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

VoxelBlock::VoxelBlock() {
}

VoxelBlock::~VoxelBlock() {
}

void VoxelBlock::set_world(Ref<World> p_world) {
	if (_world != p_world) {
		_world = p_world;

		// To update world. I replaced visibility by presence in world because Godot 3 culling performance is horrible
		_set_visible(_visible && _parent_visible);

		if (_static_body.is_valid()) {
			_static_body.set_world(*p_world);
		}
	}
}

void VoxelBlock::set_mesh(Ref<Mesh> mesh, Spatial *node, bool generate_collision, Vector<Array> surface_arrays, bool debug_collision) {
	// TODO Don't add mesh instance to the world if it's not visible.
	// I suspect Godot is trying to include invisible mesh instances into the culling process,
	// which is killing performance when LOD is used (i.e many meshes are in pool but hidden)
	// This needs investigation.

	if (mesh.is_valid()) {

		ERR_FAIL_COND(node == nullptr);
		ERR_FAIL_COND(node->get_world() != _world);

		Transform transform(Basis(), _position_in_voxels.to_vec3());

		if (!_mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			_mesh_instance.create();
			set_mesh_instance_visible(_mesh_instance, _visible && _parent_visible);
		}

		_mesh_instance.set_mesh(mesh);
		_mesh_instance.set_transform(transform);
		// TODO The day VoxelTerrain becomes a Spatial, this transform will need to be updatable separately

		if (_shader_material.is_valid()) {
			_mesh_instance.set_material_override(_shader_material);
		}
#ifdef VOXEL_DEBUG_LOD_MATERIALS
		_mesh_instance.set_material_override(_debug_material);
#endif

		if (generate_collision) {
			Ref<Shape> shape = create_concave_polygon_shape(surface_arrays);

			if (!_static_body.is_valid()) {
				_static_body.create();
				_static_body.set_world(*_world);
				_static_body.set_attached_object(node);
				_static_body.set_transform(transform);
			} else {
				_static_body.remove_shape(0);
			}
			_static_body.add_shape(shape);
			_static_body.set_debug(debug_collision, *_world);
			_static_body.set_shape_enabled(0, _visible);
		}

	} else {

		if (_mesh_instance.is_valid()) {
			// Delete instance if it exists
			_mesh_instance.destroy();
		}

		if (_static_body.is_valid()) {
			_static_body.destroy();
		}
	}

	++_mesh_update_count;

	//	if(_mesh_update_count > 1) {
	//		print_line(String("Block {0} was updated {1} times").format(varray(pos.to_vec3(), _mesh_update_count)));
	//	}
}

void VoxelBlock::set_transition_mesh(Ref<Mesh> mesh, int side) {

	DirectMeshInstance &mesh_instance = _transition_mesh_instances[side];

	if (mesh.is_valid()) {

		if (!mesh_instance.is_valid()) {
			// Create instance if it doesn't exist
			mesh_instance.create();
			set_mesh_instance_visible(mesh_instance, _visible && _parent_visible && _is_transition_visible(side));
		}

		Transform transform(Basis(), _position_in_voxels.to_vec3());

		mesh_instance.set_mesh(mesh);
		mesh_instance.set_transform(transform);

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

bool VoxelBlock::has_mesh() const {
	return _mesh_instance.get_mesh().is_valid();
}

void VoxelBlock::set_mesh_state(MeshState ms) {
	_mesh_state = ms;
}

VoxelBlock::MeshState VoxelBlock::get_mesh_state() const {
	return _mesh_state;
}

void VoxelBlock::set_visible(bool visible) {
	if (_visible == visible) {
		return;
	}
	_visible = visible;
	_set_visible(_visible && _parent_visible);
}

bool VoxelBlock::is_visible() const {
	return _visible;
}

void VoxelBlock::_set_visible(bool visible) {
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

void VoxelBlock::set_shader_material(Ref<ShaderMaterial> material) {
	_shader_material = material;
}

//void VoxelBlock::set_transition_bit(uint8_t side, bool value) {
//	CRASH_COND(side >= Cube::SIDE_COUNT);
//	uint32_t m = _transition_mask;
//	if (value) {
//		m |= (1 << side);
//	} else {
//		m &= ~(1 << side);
//	}
//	set_transition_mask(m);
//}

void VoxelBlock::set_transition_mask(uint8_t m) {
	CRASH_COND(m >= (1 << Cube::SIDE_COUNT));
	uint8_t diff = _transition_mask ^ m;
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

		_shader_material->set_shader_param(VoxelStringNames::get_singleton()->u_transition_mask, tm);
	}
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		DirectMeshInstance &mi = _transition_mesh_instances[dir];
		if ((diff & (1 << dir)) && mi.is_valid()) {
			set_mesh_instance_visible(mi, _visible && _parent_visible && _is_transition_visible(dir));
		}
	}
}

void VoxelBlock::set_parent_visible(bool parent_visible) {
	if (_parent_visible && parent_visible) {
		return;
	}
	_parent_visible = parent_visible;
	_set_visible(_visible && _parent_visible);
}

void VoxelBlock::set_needs_lodding(bool need_lodding) {
	_needs_lodding = need_lodding;
}

bool VoxelBlock::is_modified() const {
	return _modified;
}

void VoxelBlock::set_modified(bool modified) {
	//	if (_modified != modified) {
	//		print_line(String("Marking block {0}[lod{1}] as modified").format(varray(bpos.to_vec3(), lod_index)));
	//	}
	_modified = modified;
}
