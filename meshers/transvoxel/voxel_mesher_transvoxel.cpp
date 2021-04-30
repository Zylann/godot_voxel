#include "voxel_mesher_transvoxel.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/funcs.h"
#include "../../util/profiling.h"
#include "transvoxel_tables.cpp"

namespace {
static const unsigned int MESH_COMPRESSION_FLAGS =
		Mesh::ARRAY_COMPRESS_NORMAL |
		Mesh::ARRAY_COMPRESS_TANGENT |
		//Mesh::ARRAY_COMPRESS_COLOR | // Using color as 4 full floats to transfer extra attributes for now...
		//Mesh::ARRAY_COMPRESS_TEX_UV | // Not compressing UV, we use it for different texturing information
		Mesh::ARRAY_COMPRESS_TEX_UV2 |
		Mesh::ARRAY_COMPRESS_WEIGHTS;
}

VoxelMesherTransvoxel::VoxelMesherTransvoxel() {
	set_padding(Transvoxel::MIN_PADDING, Transvoxel::MAX_PADDING);
}

VoxelMesherTransvoxel::~VoxelMesherTransvoxel() {
}

Ref<Resource> VoxelMesherTransvoxel::duplicate(bool p_subresources) const {
	return memnew(VoxelMesherTransvoxel);
}

int VoxelMesherTransvoxel::get_used_channels_mask() const {
	if (_texture_mode == TEXTURES_BLEND_4_OVER_16) {
		return (1 << VoxelBuffer::CHANNEL_SDF) |
			   (1 << VoxelBuffer::CHANNEL_INDICES) |
			   (1 << VoxelBuffer::CHANNEL_WEIGHTS);
	}
	return (1 << VoxelBuffer::CHANNEL_SDF);
}

void VoxelMesherTransvoxel::fill_surface_arrays(Array &arrays, const Transvoxel::MeshArrays &src) {
	PoolVector<Vector3> vertices;
	PoolVector<Vector3> normals;
	PoolVector<Color> extra;
	PoolVector<Vector2> uv;
	PoolVector<int> indices;

	raw_copy_to(vertices, src.vertices);
	raw_copy_to(extra, src.extra);
	raw_copy_to(indices, src.indices);

	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (src.normals.size() != 0) {
		raw_copy_to(normals, src.normals);
		arrays[Mesh::ARRAY_NORMAL] = normals;
	}
	if (src.uv.size() != 0) {
		raw_copy_to(uv, src.uv);
		arrays[Mesh::ARRAY_TEX_UV] = uv;
	}
	arrays[Mesh::ARRAY_COLOR] = extra;
	arrays[Mesh::ARRAY_INDEX] = indices;
}

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	VOXEL_PROFILE_SCOPE();

	static thread_local Transvoxel::Cache s_cache;
	static thread_local Transvoxel::MeshArrays s_mesh_arrays;

	const int sdf_channel = VoxelBuffer::CHANNEL_SDF;

	// Initialize dynamic memory:
	// These vectors are re-used.
	// We don't know in advance how much geometry we are going to produce.
	// Once capacity is big enough, no more memory should be allocated
	s_mesh_arrays.clear();

	const VoxelBuffer &voxels = input.voxels;
	if (voxels.is_uniform(sdf_channel)) {
		// There won't be anything to polygonize since the SDF has no variations, so it can't cross the isolevel
		return;
	}

	// const uint64_t time_before = OS::get_singleton()->get_ticks_usec();

	Transvoxel::DefaultTextureIndicesData default_texture_indices_data = Transvoxel::build_regular_mesh(
			voxels, sdf_channel, input.lod, static_cast<Transvoxel::TexturingMode>(_texture_mode), s_cache,
			s_mesh_arrays);

	if (s_mesh_arrays.vertices.size() == 0) {
		// The mesh can be empty
		return;
	}

	Array regular_arrays;
	fill_surface_arrays(regular_arrays, s_mesh_arrays);
	output.surfaces.push_back(regular_arrays);

	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		VOXEL_PROFILE_SCOPE();
		s_mesh_arrays.clear();

		Transvoxel::build_transition_mesh(voxels, sdf_channel, dir, input.lod,
				static_cast<Transvoxel::TexturingMode>(_texture_mode), s_cache, s_mesh_arrays,
				default_texture_indices_data);

		if (s_mesh_arrays.vertices.size() == 0) {
			continue;
		}

		Array transition_arrays;
		fill_surface_arrays(transition_arrays, s_mesh_arrays);
		output.transition_surfaces[dir].push_back(transition_arrays);
	}

	// const uint64_t time_spent = OS::get_singleton()->get_ticks_usec() - time_before;
	// print_line(String("VoxelMesherTransvoxel spent {0} us").format(varray(time_spent)));

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
	output.compression_flags = MESH_COMPRESSION_FLAGS;
}

// TODO For testing at the moment
Ref<ArrayMesh> VoxelMesherTransvoxel::build_transition_mesh(Ref<VoxelBuffer> voxels, int direction) {
	static thread_local Transvoxel::Cache s_cache;
	static thread_local Transvoxel::MeshArrays s_mesh_arrays;

	s_mesh_arrays.clear();

	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	// TODO We need to output transition meshes through the generic interface, they are part of the result
	// For now we can't support proper texture indices in this specific case
	Transvoxel::DefaultTextureIndicesData default_texture_indices_data;
	default_texture_indices_data.use = false;
	Transvoxel::build_transition_mesh(**voxels, VoxelBuffer::CHANNEL_SDF, direction, 0,
			static_cast<Transvoxel::TexturingMode>(_texture_mode), s_cache, s_mesh_arrays,
			default_texture_indices_data);

	Ref<ArrayMesh> mesh;

	if (s_mesh_arrays.vertices.size() == 0) {
		return mesh;
	}

	Array arrays;
	fill_surface_arrays(arrays, s_mesh_arrays);
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), MESH_COMPRESSION_FLAGS);
	return mesh;
}

void VoxelMesherTransvoxel::set_texturing_mode(TexturingMode mode) {
	if (mode != _texture_mode) {
		_texture_mode = mode;
		emit_changed();
	}
}

VoxelMesherTransvoxel::TexturingMode VoxelMesherTransvoxel::get_texturing_mode() const {
	return _texture_mode;
}

void VoxelMesherTransvoxel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_transition_mesh", "voxel_buffer", "direction"),
			&VoxelMesherTransvoxel::build_transition_mesh);

	ClassDB::bind_method(D_METHOD("set_texturing_mode", "mode"), &VoxelMesherTransvoxel::set_texturing_mode);
	ClassDB::bind_method(D_METHOD("get_texturing_mode"), &VoxelMesherTransvoxel::get_texturing_mode);

	ADD_PROPERTY(PropertyInfo(
						 Variant::INT, "texturing_mode", PROPERTY_HINT_ENUM, "None,4-blend over 16 textures (4 bits)"),
			"set_texturing_mode", "get_texturing_mode");

	BIND_ENUM_CONSTANT(TEXTURES_NONE);
	BIND_ENUM_CONSTANT(TEXTURES_BLEND_4_OVER_16);
}
