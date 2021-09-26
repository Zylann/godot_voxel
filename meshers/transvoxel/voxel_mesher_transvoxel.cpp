#include "voxel_mesher_transvoxel.h"
#include "../../storage/voxel_buffer.h"
#include "../../thirdparty/meshoptimizer/meshoptimizer.h"
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
		return (1 << VoxelBufferInternal::CHANNEL_SDF) |
			   (1 << VoxelBufferInternal::CHANNEL_INDICES) |
			   (1 << VoxelBufferInternal::CHANNEL_WEIGHTS);
	}
	return (1 << VoxelBufferInternal::CHANNEL_SDF);
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

template <typename T>
static void remap_vertex_array(const std::vector<T> &src_data, std::vector<T> &dst_data,
		const std::vector<unsigned int> &remap_indices, unsigned int unique_vertex_count) {
	if (src_data.size() == 0) {
		dst_data.clear();
		return;
	}
	dst_data.resize(unique_vertex_count);
	meshopt_remapVertexBuffer(&dst_data[0], &src_data[0], src_data.size(), sizeof(T), remap_indices.data());
}

static void simplify(const Transvoxel::MeshArrays &src_mesh, Transvoxel::MeshArrays &dst_mesh,
		float p_target_ratio, float p_error_threshold) {
	VOXEL_PROFILE_SCOPE();

	// Gather and check input

	ERR_FAIL_COND(p_target_ratio < 0.f || p_target_ratio > 1.f);
	ERR_FAIL_COND(p_error_threshold < 0.f || p_error_threshold > 1.f);
	ERR_FAIL_COND(src_mesh.vertices.size() < 3);
	ERR_FAIL_COND(src_mesh.indices.size() < 3);

	const unsigned int target_index_count = p_target_ratio * src_mesh.indices.size();

	static thread_local std::vector<unsigned int> lod_indices;
	lod_indices.clear();
	lod_indices.resize(src_mesh.indices.size());

	float lod_error = 0.f;

	// Simplify
	{
		VOXEL_PROFILE_SCOPE_NAMED("meshopt_simplify");

		const unsigned int lod_index_count = meshopt_simplify(
				&lod_indices[0], reinterpret_cast<const unsigned int *>(src_mesh.indices.data()), src_mesh.indices.size(),
				&src_mesh.vertices[0].x, src_mesh.vertices.size(),
				sizeof(Vector3), target_index_count, p_error_threshold, &lod_error);

		lod_indices.resize(lod_index_count);
	}

	// Produce output

	Array surface;
	surface.resize(Mesh::ARRAY_MAX);

	static thread_local std::vector<unsigned int> remap_indices;
	remap_indices.clear();
	remap_indices.resize(src_mesh.vertices.size());

	const unsigned int unique_vertex_count = meshopt_optimizeVertexFetchRemap(
			&remap_indices[0], lod_indices.data(), lod_indices.size(), src_mesh.vertices.size());

	remap_vertex_array(src_mesh.vertices, dst_mesh.vertices, remap_indices, unique_vertex_count);
	remap_vertex_array(src_mesh.normals, dst_mesh.normals, remap_indices, unique_vertex_count);
	remap_vertex_array(src_mesh.extra, dst_mesh.extra, remap_indices, unique_vertex_count);
	remap_vertex_array(src_mesh.uv, dst_mesh.uv, remap_indices, unique_vertex_count);

	dst_mesh.indices.resize(lod_indices.size());
	// TODO Not sure if arguments are correct
	meshopt_remapIndexBuffer(reinterpret_cast<unsigned int *>(dst_mesh.indices.data()),
			lod_indices.data(), lod_indices.size(), remap_indices.data());
}

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	VOXEL_PROFILE_SCOPE();

	static thread_local Transvoxel::Cache s_cache;
	static thread_local Transvoxel::MeshArrays s_mesh_arrays;
	static thread_local Transvoxel::MeshArrays s_simplified_mesh_arrays;

	const int sdf_channel = VoxelBufferInternal::CHANNEL_SDF;

	// Initialize dynamic memory:
	// These vectors are re-used.
	// We don't know in advance how much geometry we are going to produce.
	// Once capacity is big enough, no more memory should be allocated
	s_mesh_arrays.clear();

	const VoxelBufferInternal &voxels = input.voxels;
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

	if (_mesh_optimization_params.enabled) {
		// TODO When voxel texturing is enabled, this will decrease quality a lot.
		// There is no support yet for taking textures into account when simplifying.
		// See https://github.com/zeux/meshoptimizer/issues/158
		simplify(s_mesh_arrays, s_simplified_mesh_arrays,
				_mesh_optimization_params.target_ratio, _mesh_optimization_params.error_threshold);

		fill_surface_arrays(regular_arrays, s_simplified_mesh_arrays);

	} else {
		fill_surface_arrays(regular_arrays, s_mesh_arrays);
	}

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

	if (voxels->is_uniform(VoxelBufferInternal::CHANNEL_SDF)) {
		// Uniform SDF won't produce any surface
		return Ref<ArrayMesh>();
	}

	// TODO We need to output transition meshes through the generic interface, they are part of the result
	// For now we can't support proper texture indices in this specific case
	Transvoxel::DefaultTextureIndicesData default_texture_indices_data;
	default_texture_indices_data.use = false;
	Transvoxel::build_transition_mesh(voxels->get_buffer(), VoxelBufferInternal::CHANNEL_SDF, direction, 0,
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

void VoxelMesherTransvoxel::set_mesh_optimization_enabled(bool enabled) {
	_mesh_optimization_params.enabled = enabled;
}

bool VoxelMesherTransvoxel::is_mesh_optimization_enabled() const {
	return _mesh_optimization_params.enabled;
}

void VoxelMesherTransvoxel::set_mesh_optimization_error_threshold(float threshold) {
	_mesh_optimization_params.error_threshold = clamp(threshold, 0.f, 1.f);
}

float VoxelMesherTransvoxel::get_mesh_optimization_error_threshold() const {
	return _mesh_optimization_params.error_threshold;
}

void VoxelMesherTransvoxel::set_mesh_optimization_target_ratio(float ratio) {
	_mesh_optimization_params.target_ratio = clamp(ratio, 0.f, 1.f);
}

float VoxelMesherTransvoxel::get_mesh_optimization_target_ratio() const {
	return _mesh_optimization_params.target_ratio;
}

void VoxelMesherTransvoxel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_transition_mesh", "voxel_buffer", "direction"),
			&VoxelMesherTransvoxel::build_transition_mesh);

	ClassDB::bind_method(D_METHOD("set_texturing_mode", "mode"), &VoxelMesherTransvoxel::set_texturing_mode);
	ClassDB::bind_method(D_METHOD("get_texturing_mode"), &VoxelMesherTransvoxel::get_texturing_mode);

	ClassDB::bind_method(D_METHOD("set_mesh_optimization_enabled", "enabled"),
			&VoxelMesherTransvoxel::set_mesh_optimization_enabled);
	ClassDB::bind_method(D_METHOD("is_mesh_optimization_enabled"),
			&VoxelMesherTransvoxel::is_mesh_optimization_enabled);

	ClassDB::bind_method(D_METHOD("set_mesh_optimization_error_threshold", "threshold"),
			&VoxelMesherTransvoxel::set_mesh_optimization_error_threshold);
	ClassDB::bind_method(D_METHOD("get_mesh_optimization_error_threshold"),
			&VoxelMesherTransvoxel::get_mesh_optimization_error_threshold);

	ClassDB::bind_method(D_METHOD("set_mesh_optimization_target_ratio", "ratio"),
			&VoxelMesherTransvoxel::set_mesh_optimization_target_ratio);
	ClassDB::bind_method(D_METHOD("get_mesh_optimization_target_ratio"),
			&VoxelMesherTransvoxel::get_mesh_optimization_target_ratio);

	ADD_PROPERTY(PropertyInfo(
						 Variant::INT, "texturing_mode", PROPERTY_HINT_ENUM, "None,4-blend over 16 textures (4 bits)"),
			"set_texturing_mode", "get_texturing_mode");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mesh_optimization_enabled"),
			"set_mesh_optimization_enabled", "is_mesh_optimization_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "mesh_optimization_error_threshold"),
			"set_mesh_optimization_error_threshold", "get_mesh_optimization_error_threshold");

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "mesh_optimization_target_ratio"),
			"set_mesh_optimization_target_ratio", "get_mesh_optimization_target_ratio");

	BIND_ENUM_CONSTANT(TEXTURES_NONE);
	BIND_ENUM_CONSTANT(TEXTURES_BLEND_4_OVER_16);
}
