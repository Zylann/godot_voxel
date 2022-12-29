#include "voxel_mesher_transvoxel.h"
#include "../../engine/voxel_engine.h"
#include "../../generators/voxel_generator.h"
#include "../../shaders/transvoxel_minimal_shader.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../storage/voxel_data.h"
#include "../../thirdparty/meshoptimizer/meshoptimizer.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/rendering_server.h"
#include "../../util/godot/classes/shader.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/funcs.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "transvoxel_tables.cpp"

namespace zylann::voxel {

namespace {
Ref<ShaderMaterial> g_minimal_shader_material;
} // namespace

namespace transvoxel {
// Wrapping thread-locals in functions so they initialize the first time they are needed, instead of when the
// application starts. This works around a bug in the static debug MSVC runtime (/MTd). At time of writing, Godot is
// using /MT even in debug builds, which prevents from getting safety checks from the standard library.
// See https://github.com/baldurk/renderdoc/issues/1743
// and https://developercommunity.visualstudio.com/t/race-condition-on-g-tss-mutex-with-static-crt/672664
MeshArrays &get_tls_mesh_arrays() {
	thread_local MeshArrays tls_mesh_arrays;
	return tls_mesh_arrays;
}
std::vector<CellInfo> &get_tls_cell_infos() {
	thread_local std::vector<CellInfo> tls_cell_infos;
	return tls_cell_infos;
}
} // namespace transvoxel

const transvoxel::MeshArrays &VoxelMesherTransvoxel::get_mesh_cache_from_current_thread() {
	return transvoxel::get_tls_mesh_arrays();
}

Span<const transvoxel::CellInfo> VoxelMesherTransvoxel::get_cell_info_from_current_thread() {
	return to_span(transvoxel::get_tls_cell_infos());
}

void VoxelMesherTransvoxel::load_static_resources() {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(g_transvoxel_minimal_shader);
	g_minimal_shader_material.instantiate();
	g_minimal_shader_material->set_shader(shader);
}

void VoxelMesherTransvoxel::free_static_resources() {
	g_minimal_shader_material.unref();
}

VoxelMesherTransvoxel::VoxelMesherTransvoxel() {
	set_padding(transvoxel::MIN_PADDING, transvoxel::MAX_PADDING);
}

VoxelMesherTransvoxel::~VoxelMesherTransvoxel() {}

int VoxelMesherTransvoxel::get_used_channels_mask() const {
	if (_texture_mode == TEXTURES_BLEND_4_OVER_16) {
		return (1 << VoxelBufferInternal::CHANNEL_SDF) | (1 << VoxelBufferInternal::CHANNEL_INDICES) |
				(1 << VoxelBufferInternal::CHANNEL_WEIGHTS);
	}
	return (1 << VoxelBufferInternal::CHANNEL_SDF);
}

bool VoxelMesherTransvoxel::is_generating_collision_surface() const {
	// Via submesh indices
	return true;
}

static void fill_surface_arrays(Array &arrays, const transvoxel::MeshArrays &src) {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedFloat32Array lod_data; // 4*float32
	PackedFloat32Array texturing_data; // 2*4*uint8 as 2*float32
	PackedInt32Array indices;

	copy_to(vertices, src.vertices);

	// raw_copy_to(lod_data, src.lod_data);
	lod_data.resize(src.lod_data.size() * 4);
	// Based on the layout, position is first 3 floats, and 4th float is actually a bitmask
	static_assert(sizeof(transvoxel::LodAttrib) == 16);
	memcpy(lod_data.ptrw(), src.lod_data.data(), lod_data.size() * sizeof(float));

	copy_to(indices, src.indices);

	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (src.normals.size() != 0) {
		copy_to(normals, src.normals);
		arrays[Mesh::ARRAY_NORMAL] = normals;
	}
	if (src.texturing_data.size() != 0) {
		// raw_copy_to(texturing_data, src.texturing_data);
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
	ZN_PROFILE_SCOPE();

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
		ZN_PROFILE_SCOPE_NAMED("meshopt_simplify");

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

struct DeepSampler : transvoxel::IDeepSDFSampler {
	VoxelGenerator &generator;
	const VoxelData &data;
	const VoxelBufferInternal::ChannelId sdf_channel;
	const Vector3i origin;

	DeepSampler(VoxelGenerator &p_generator, const VoxelData &p_data, VoxelBufferInternal::ChannelId p_sdf_channel,
			Vector3i p_origin) :
			generator(p_generator), data(p_data), sdf_channel(p_sdf_channel), origin(p_origin) {}

	float get_single(Vector3i position_in_voxels, uint32_t lod_index) const override {
		position_in_voxels += origin;
		return data.get_voxel_f(position_in_voxels, sdf_channel);
		/*const Vector3i lod_pos = position_in_voxels >> lod_index;
		const VoxelDataLodMap::Lod &lod = data.lods[lod_index];
		unsigned int bsm = 0;
		std::shared_ptr<VoxelBufferInternal> voxels;
		{
			RWLockRead rlock(lod.map_lock);
			const Vector3i lod_bpos = lod_pos >> lod.map.get_block_size_pow2();
			const VoxelDataBlock *block = lod.map.get_block(lod_bpos);
			// TODO Thread-safety: this checking presence of voxels is not safe.
			// It can change while meshing takes place if a modifier is moved in the same area,
			// because it invalidates cached data.
			if (block != nullptr && block->has_voxels()) {
				voxels = block->get_voxels_shared();
				bsm = lod.map.get_block_size_mask();
			}
		}
		if (voxels != nullptr) {
			RWLockRead rlock(voxels->get_lock());
			return voxels->get_voxel_f(lod_pos & bsm, sdf_channel);
		} else {
			return generator.generate_single(position_in_voxels, sdf_channel).f;
		}*/
	}
};

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	ZN_PROFILE_SCOPE();

	static thread_local transvoxel::Cache tls_cache;
	// static thread_local FixedArray<transvoxel::MeshArrays, Cube::SIDE_COUNT> tls_transition_mesh_arrays;
	static thread_local transvoxel::MeshArrays tls_simplified_mesh_arrays;

	const VoxelBufferInternal::ChannelId sdf_channel = VoxelBufferInternal::CHANNEL_SDF;

	// Initialize dynamic memory:
	// These vectors are re-used.
	// We don't know in advance how much geometry we are going to produce.
	// Once capacity is big enough, no more memory should be allocated
	transvoxel::MeshArrays &mesh_arrays = transvoxel::get_tls_mesh_arrays();
	mesh_arrays.clear();

	const VoxelBufferInternal &voxels = input.voxels;
	if (voxels.is_uniform(sdf_channel)) {
		// There won't be anything to polygonize since the SDF has no variations, so it can't cross the isolevel
		return;
	}

	// const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	transvoxel::DefaultTextureIndicesData default_texture_indices_data;
	std::vector<transvoxel::CellInfo> *cell_infos = nullptr;
	if (input.virtual_texture_hint) {
		transvoxel::get_tls_cell_infos().clear();
		cell_infos = &transvoxel::get_tls_cell_infos();
	}

	if (_deep_sampling_enabled && input.generator != nullptr && input.data != nullptr && input.lod_index > 0) {
		const DeepSampler ds(*input.generator, *input.data, sdf_channel, input.origin_in_voxels);
		// TODO Optimization: "area scope" feature on generators to optimize certain uses of `generate_single`.
		// The idea is to call `begin_area(box)` and `end_area()`, so the generator can optimize random calls to
		// `generate_single` in between, knowing they will all be done within the specified area.

		default_texture_indices_data = transvoxel::build_regular_mesh(voxels, sdf_channel, input.lod_index,
				static_cast<transvoxel::TexturingMode>(_texture_mode), tls_cache, mesh_arrays, &ds, cell_infos);
	} else {
		default_texture_indices_data = transvoxel::build_regular_mesh(voxels, sdf_channel, input.lod_index,
				static_cast<transvoxel::TexturingMode>(_texture_mode), tls_cache, mesh_arrays, nullptr, cell_infos);
	}

	if (mesh_arrays.vertices.size() == 0) {
		// The mesh can be empty
		return;
	}

	transvoxel::MeshArrays *combined_mesh_arrays = &mesh_arrays;
	if (_mesh_optimization_params.enabled) {
		// TODO When voxel texturing is enabled, this will decrease quality a lot.
		// There is no support yet for taking textures into account when simplifying.
		// See https://github.com/zeux/meshoptimizer/issues/158
		simplify(mesh_arrays, tls_simplified_mesh_arrays, _mesh_optimization_params.target_ratio,
				_mesh_optimization_params.error_threshold);

		combined_mesh_arrays = &tls_simplified_mesh_arrays;
	}

	output.collision_surface.submesh_vertex_end = combined_mesh_arrays->vertices.size();
	output.collision_surface.submesh_index_end = combined_mesh_arrays->indices.size();

	if (_transitions_enabled && input.lod_hint) {
		// We combine transition meshes with the regular mesh, because it results in less draw calls than if they were
		// separate. This only requires a vertex shader trick to discard them when neighbors change.
		ZN_ASSERT(combined_mesh_arrays != nullptr);

		for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			ZN_PROFILE_SCOPE();

			transvoxel::build_transition_mesh(voxels, sdf_channel, dir, input.lod_index,
					static_cast<transvoxel::TexturingMode>(_texture_mode), tls_cache, *combined_mesh_arrays,
					default_texture_indices_data);
		}
	}

	Array gd_arrays;
	fill_surface_arrays(gd_arrays, *combined_mesh_arrays);
	output.surfaces.push_back({ gd_arrays, 0 });

	// const uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	// print_line(String("VoxelMesherTransvoxel spent {0} us").format(varray(time_spent)));

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
	output.mesh_flags = //
			(RenderingServer::ARRAY_CUSTOM_RGBA_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT) |
			(RenderingServer::ARRAY_CUSTOM_RG_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM1_SHIFT);
}

// Only exists for testing
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

void VoxelMesherTransvoxel::set_deep_sampling_enabled(bool enable) {
	_deep_sampling_enabled = enable;
}

bool VoxelMesherTransvoxel::is_deep_sampling_enabled() const {
	return _deep_sampling_enabled;
}

void VoxelMesherTransvoxel::set_transitions_enabled(bool enable) {
	_transitions_enabled = enable;
}

bool VoxelMesherTransvoxel::get_transitions_enabled() const {
	return _transitions_enabled;
}

Ref<ShaderMaterial> VoxelMesherTransvoxel::get_default_lod_material() const {
	return g_minimal_shader_material;
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

	ClassDB::bind_method(
			D_METHOD("set_deep_sampling_enabled", "enabled"), &VoxelMesherTransvoxel::set_deep_sampling_enabled);
	ClassDB::bind_method(D_METHOD("is_deep_sampling_enabled"), &VoxelMesherTransvoxel::is_deep_sampling_enabled);

	ClassDB::bind_method(
			D_METHOD("set_transitions_enabled", "enabled"), &VoxelMesherTransvoxel::set_transitions_enabled);
	ClassDB::bind_method(D_METHOD("get_transitions_enabled"), &VoxelMesherTransvoxel::get_transitions_enabled);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "texturing_mode", PROPERTY_HINT_ENUM, "None,4-blend over 16 textures (4 bits)"),
			"set_texturing_mode", "get_texturing_mode");

	ADD_GROUP("Mesh optimization", "mesh_optimization_");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mesh_optimization_enabled"), "set_mesh_optimization_enabled",
			"is_mesh_optimization_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_optimization_error_threshold"),
			"set_mesh_optimization_error_threshold", "get_mesh_optimization_error_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_optimization_target_ratio"), "set_mesh_optimization_target_ratio",
			"get_mesh_optimization_target_ratio");

	ADD_GROUP("Advanced", "");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deep_sampling_enabled"), "set_deep_sampling_enabled",
			"is_deep_sampling_enabled");
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "transitions_enabled"), "set_transitions_enabled", "get_transitions_enabled");

	BIND_ENUM_CONSTANT(TEXTURES_NONE);
	// TODO Rename MIXEL
	BIND_ENUM_CONSTANT(TEXTURES_BLEND_4_OVER_16);
}

} // namespace zylann::voxel
