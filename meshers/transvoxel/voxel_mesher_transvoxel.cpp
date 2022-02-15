#include "voxel_mesher_transvoxel.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../thirdparty/meshoptimizer/meshoptimizer.h"
#include "../../util/funcs.h"
#include "../../util/godot/funcs.h"
#include "../../util/profiling.h"
#include "transvoxel_tables.cpp"

namespace zylann::voxel {

VoxelMesherTransvoxel::VoxelMesherTransvoxel() {
	set_padding(transvoxel::MIN_PADDING, transvoxel::MAX_PADDING);
}

VoxelMesherTransvoxel::~VoxelMesherTransvoxel() {}

Ref<Resource> VoxelMesherTransvoxel::duplicate(bool p_subresources) const {
	return memnew(VoxelMesherTransvoxel);
}

int VoxelMesherTransvoxel::get_used_channels_mask() const {
	if (_texture_mode == TEXTURES_BLEND_4_OVER_16) {
		return (1 << VoxelBufferInternal::CHANNEL_SDF) | (1 << VoxelBufferInternal::CHANNEL_INDICES) |
				(1 << VoxelBufferInternal::CHANNEL_WEIGHTS);
	}
	return (1 << VoxelBufferInternal::CHANNEL_SDF);
}

void VoxelMesherTransvoxel::fill_surface_arrays(Array &arrays, const transvoxel::MeshArrays &src) {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedFloat32Array lod_data; // 4*float32
	PackedFloat32Array texturing_data; // 2*4*uint8 as 2*float32
	PackedInt32Array indices;

	copy_to(vertices, src.vertices);

	//raw_copy_to(lod_data, src.lod_data);
	lod_data.resize(src.lod_data.size() * 4);
	memcpy(lod_data.ptrw(), src.lod_data.data(), lod_data.size() * sizeof(float));

	raw_copy_to(indices, src.indices);

	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (src.normals.size() != 0) {
		copy_to(normals, src.normals);
		arrays[Mesh::ARRAY_NORMAL] = normals;
	}
	if (src.texturing_data.size() != 0) {
		//raw_copy_to(texturing_data, src.texturing_data);
		texturing_data.resize(src.texturing_data.size() * 2);
		memcpy(texturing_data.ptrw(), src.texturing_data.data(), texturing_data.size() * sizeof(float));
		arrays[Mesh::ARRAY_CUSTOM1] = texturing_data;
	}
	arrays[Mesh::ARRAY_CUSTOM0] = lod_data;
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
	zylannmeshopt::meshopt_remapVertexBuffer(
			&dst_data[0], &src_data[0], src_data.size(), sizeof(T), remap_indices.data());
}

static void simplify(const transvoxel::MeshArrays &src_mesh, transvoxel::MeshArrays &dst_mesh, float p_target_ratio,
		float p_error_threshold) {
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

		// TODO See build script about the `zylannmeshopt::` namespace
		const unsigned int lod_index_count = zylannmeshopt::meshopt_simplify(&lod_indices[0],
				reinterpret_cast<const unsigned int *>(src_mesh.indices.data()), src_mesh.indices.size(),
				&src_mesh.vertices[0].x, src_mesh.vertices.size(), sizeof(Vector3f), target_index_count,
				p_error_threshold, &lod_error);

		lod_indices.resize(lod_index_count);
	}

	// Produce output

	Array surface;
	surface.resize(Mesh::ARRAY_MAX);

	static thread_local std::vector<unsigned int> remap_indices;
	remap_indices.clear();
	remap_indices.resize(src_mesh.vertices.size());

	const unsigned int unique_vertex_count = zylannmeshopt::meshopt_optimizeVertexFetchRemap(
			&remap_indices[0], lod_indices.data(), lod_indices.size(), src_mesh.vertices.size());

	remap_vertex_array(src_mesh.vertices, dst_mesh.vertices, remap_indices, unique_vertex_count);
	remap_vertex_array(src_mesh.normals, dst_mesh.normals, remap_indices, unique_vertex_count);
	remap_vertex_array(src_mesh.lod_data, dst_mesh.lod_data, remap_indices, unique_vertex_count);
	remap_vertex_array(src_mesh.texturing_data, dst_mesh.texturing_data, remap_indices, unique_vertex_count);

	dst_mesh.indices.resize(lod_indices.size());
	// TODO Not sure if arguments are correct
	zylannmeshopt::meshopt_remapIndexBuffer(reinterpret_cast<unsigned int *>(dst_mesh.indices.data()),
			lod_indices.data(), lod_indices.size(), remap_indices.data());
}

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	VOXEL_PROFILE_SCOPE();

	static thread_local transvoxel::Cache s_cache;
	static thread_local transvoxel::MeshArrays s_mesh_arrays;
	static thread_local transvoxel::MeshArrays s_simplified_mesh_arrays;

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

	// const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	transvoxel::DefaultTextureIndicesData default_texture_indices_data = transvoxel::build_regular_mesh(voxels,
			sdf_channel, input.lod, static_cast<transvoxel::TexturingMode>(_texture_mode), s_cache, s_mesh_arrays);

	if (s_mesh_arrays.vertices.size() == 0) {
		// The mesh can be empty
		return;
	}

	Array regular_arrays;

	if (_mesh_optimization_params.enabled) {
		// TODO When voxel texturing is enabled, this will decrease quality a lot.
		// There is no support yet for taking textures into account when simplifying.
		// See https://github.com/zeux/meshoptimizer/issues/158
		simplify(s_mesh_arrays, s_simplified_mesh_arrays, _mesh_optimization_params.target_ratio,
				_mesh_optimization_params.error_threshold);

		fill_surface_arrays(regular_arrays, s_simplified_mesh_arrays);

	} else {
		fill_surface_arrays(regular_arrays, s_mesh_arrays);
	}

	output.surfaces.push_back(regular_arrays);

	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		VOXEL_PROFILE_SCOPE();
		s_mesh_arrays.clear();

		transvoxel::build_transition_mesh(voxels, sdf_channel, dir, input.lod,
				static_cast<transvoxel::TexturingMode>(_texture_mode), s_cache, s_mesh_arrays,
				default_texture_indices_data);

		if (s_mesh_arrays.vertices.size() == 0) {
			continue;
		}

		Array transition_arrays;
		fill_surface_arrays(transition_arrays, s_mesh_arrays);
		output.transition_surfaces[dir].push_back(transition_arrays);
	}

	// const uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	// print_line(String("VoxelMesherTransvoxel spent {0} us").format(varray(time_spent)));

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
	output.mesh_flags = //
			(RenderingServer::ARRAY_CUSTOM_RGBA_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT) |
			(RenderingServer::ARRAY_CUSTOM_RG_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM1_SHIFT);
}

// TODO For testing at the moment
Ref<ArrayMesh> VoxelMesherTransvoxel::build_transition_mesh(Ref<gd::VoxelBuffer> voxels, int direction) {
	static thread_local transvoxel::Cache s_cache;
	static thread_local transvoxel::MeshArrays s_mesh_arrays;

	s_mesh_arrays.clear();

	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	if (voxels->is_uniform(VoxelBufferInternal::CHANNEL_SDF)) {
		// Uniform SDF won't produce any surface
		return Ref<ArrayMesh>();
	}

	// TODO We need to output transition meshes through the generic interface, they are part of the result
	// For now we can't support proper texture indices in this specific case
	transvoxel::DefaultTextureIndicesData default_texture_indices_data;
	default_texture_indices_data.use = false;
	transvoxel::build_transition_mesh(voxels->get_buffer(), VoxelBufferInternal::CHANNEL_SDF, direction, 0,
			static_cast<transvoxel::TexturingMode>(_texture_mode), s_cache, s_mesh_arrays,
			default_texture_indices_data);

	Ref<ArrayMesh> mesh;

	if (s_mesh_arrays.vertices.size() == 0) {
		return mesh;
	}

	Array arrays;
	fill_surface_arrays(arrays, s_mesh_arrays);
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
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
	_mesh_optimization_params.error_threshold = math::clamp(threshold, 0.f, 1.f);
}

float VoxelMesherTransvoxel::get_mesh_optimization_error_threshold() const {
	return _mesh_optimization_params.error_threshold;
}

void VoxelMesherTransvoxel::set_mesh_optimization_target_ratio(float ratio) {
	_mesh_optimization_params.target_ratio = math::clamp(ratio, 0.f, 1.f);
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
	ClassDB::bind_method(
			D_METHOD("is_mesh_optimization_enabled"), &VoxelMesherTransvoxel::is_mesh_optimization_enabled);

	ClassDB::bind_method(D_METHOD("set_mesh_optimization_error_threshold", "threshold"),
			&VoxelMesherTransvoxel::set_mesh_optimization_error_threshold);
	ClassDB::bind_method(D_METHOD("get_mesh_optimization_error_threshold"),
			&VoxelMesherTransvoxel::get_mesh_optimization_error_threshold);

	ClassDB::bind_method(D_METHOD("set_mesh_optimization_target_ratio", "ratio"),
			&VoxelMesherTransvoxel::set_mesh_optimization_target_ratio);
	ClassDB::bind_method(
			D_METHOD("get_mesh_optimization_target_ratio"), &VoxelMesherTransvoxel::get_mesh_optimization_target_ratio);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "texturing_mode", PROPERTY_HINT_ENUM, "None,4-blend over 16 textures (4 bits)"),
			"set_texturing_mode", "get_texturing_mode");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mesh_optimization_enabled"), "set_mesh_optimization_enabled",
			"is_mesh_optimization_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_optimization_error_threshold"),
			"set_mesh_optimization_error_threshold", "get_mesh_optimization_error_threshold");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_optimization_target_ratio"), "set_mesh_optimization_target_ratio",
			"get_mesh_optimization_target_ratio");

	BIND_ENUM_CONSTANT(TEXTURES_NONE);
	BIND_ENUM_CONSTANT(TEXTURES_BLEND_4_OVER_16);
}

} // namespace zylann::voxel
