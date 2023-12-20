#include "mesh_block_task.h"
#include "../engine/detail_rendering/render_detail_texture_task.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../storage/voxel_data.h"
#include "../terrain/voxel_mesh_block.h"
#include "../util/dstack.h"
#include "../util/godot/classes/mesh.h"
#include "../util/io/log.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
//#include "../util/string_funcs.h" // Debug
#include "../engine/voxel_engine.h"
#include "../generators/generate_block_gpu_task.h"
#include "../meshers/transvoxel/transvoxel_cell_iterator.h"

namespace zylann::voxel {

struct CubicAreaInfo {
	int edge_size; // In data blocks
	int mesh_block_size_factor;
	unsigned int anchor_buffer_index;

	inline bool is_valid() const {
		return edge_size != 0;
	}
};

CubicAreaInfo get_cubic_area_info_from_size(unsigned int size) {
	// Determine size of the cube of blocks
	int edge_size;
	int mesh_block_size_factor;
	switch (size) {
		case 3 * 3 * 3:
			edge_size = 3;
			mesh_block_size_factor = 1;
			break;
		case 4 * 4 * 4:
			edge_size = 4;
			mesh_block_size_factor = 2;
			break;
		default:
			ZN_PRINT_ERROR("Unsupported block count");
			return CubicAreaInfo{ 0, 0, 0 };
	}

	// Pick anchor block, usually within the central part of the cube (that block must be valid)
	const unsigned int anchor_buffer_index = edge_size * edge_size + edge_size + 1;

	return { edge_size, mesh_block_size_factor, anchor_buffer_index };
}

// Takes a list of blocks and interprets it as a cube of blocks centered around the area we want to create a mesh from.
// Voxels from central blocks are copied, and part of side blocks are also copied so we get a temporary buffer
// which includes enough neighbors for the mesher to avoid doing bound checks.
static void copy_block_and_neighbors(Span<std::shared_ptr<VoxelBufferInternal>> blocks, VoxelBufferInternal &dst,
		int min_padding, int max_padding, int channels_mask, Ref<VoxelGenerator> generator, const VoxelData &voxel_data,
		uint8_t lod_index, Vector3i mesh_block_pos, std::vector<Box3i> *out_boxes_to_generate,
		Vector3i *out_origin_in_voxels) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	// Extract wanted channels in a list
	unsigned int channels_count = 0;
	FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channels =
			VoxelBufferInternal::mask_to_channels_list(channels_mask, channels_count);

	// Determine size of the cube of blocks
	const CubicAreaInfo area_info = get_cubic_area_info_from_size(blocks.size());
	ERR_FAIL_COND(!area_info.is_valid());

	std::shared_ptr<VoxelBufferInternal> &central_buffer = blocks[area_info.anchor_buffer_index];
	ERR_FAIL_COND_MSG(central_buffer == nullptr && generator.is_null(), "Central buffer must be valid");
	if (central_buffer != nullptr) {
		ERR_FAIL_COND_MSG(
				Vector3iUtil::all_members_equal(central_buffer->get_size()) == false, "Central buffer must be cubic");
	}
	const int data_block_size = voxel_data.get_block_size();
	const int mesh_block_size = data_block_size * area_info.mesh_block_size_factor;
	const int padded_mesh_block_size = mesh_block_size + min_padding + max_padding;

	dst.create(padded_mesh_block_size, padded_mesh_block_size, padded_mesh_block_size);

	// TODO Need to provide format differently, this won't work in full load mode where areas are generated on the fly
	// for (unsigned int ci = 0; ci < channels.size(); ++ci) {
	// 	dst.set_channel_depth(ci, central_buffer->get_channel_depth(ci));
	// }
	// This is a hack
	for (unsigned int i = 0; i < blocks.size(); ++i) {
		const std::shared_ptr<VoxelBufferInternal> &buffer = blocks[i];
		if (buffer != nullptr) {
			// Initialize channel depths from the first non-null block found
			dst.copy_format(*buffer);
			break;
		}
	}

	const Vector3i min_pos = -Vector3iUtil::create(min_padding);
	const Vector3i max_pos = Vector3iUtil::create(mesh_block_size + max_padding);

	// TODO In terrains that only work with caches, we should never consider generating voxels from here.
	// This is the case of VoxelTerrain, which is now doing unnecessary box subtraction calculations...

	// These boxes are in buffer coordinates (not world voxel coordinates)
	std::vector<Box3i> boxes_to_generate;
	const Box3i mesh_data_box = Box3i::from_min_max(min_pos, max_pos);
	if (contains(blocks.to_const(), std::shared_ptr<VoxelBufferInternal>())) {
		boxes_to_generate.push_back(mesh_data_box);
	}

	{
		// TODO The following logic might as well be simplified and moved to VoxelData.
		// We are just sampling or generating data in a given area.

		const Vector3i data_block_pos0 = mesh_block_pos * area_info.mesh_block_size_factor;
		SpatialLock3D::Read srlock(voxel_data.get_spatial_lock(lod_index),
				BoxBounds3i(data_block_pos0 - Vector3i(1, 1, 1),
						data_block_pos0 + Vector3iUtil::create(area_info.edge_size)));

		// Using ZXY as convention to reconstruct positions with thread locking consistency
		unsigned int block_index = 0;
		for (int z = -1; z < area_info.edge_size - 1; ++z) {
			for (int x = -1; x < area_info.edge_size - 1; ++x) {
				for (int y = -1; y < area_info.edge_size - 1; ++y) {
					const Vector3i offset = data_block_size * Vector3i(x, y, z);
					const std::shared_ptr<VoxelBufferInternal> &src = blocks[block_index];
					++block_index;

					if (src == nullptr) {
						continue;
					}

					const Vector3i src_min = min_pos - offset;
					const Vector3i src_max = max_pos - offset;

					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						dst.copy_from(*src, src_min, src_max, Vector3i(), channels[ci]);
					}

					if (boxes_to_generate.size() > 0) {
						// Subtract edited box from the area to generate
						// TODO This approach allows to batch boxes if necessary,
						// but is it just better to do it anyways for every clipped box?
						ZN_PROFILE_SCOPE_NAMED("Box subtract");
						const unsigned int input_count = boxes_to_generate.size();
						const Box3i block_box =
								Box3i(offset, Vector3iUtil::create(data_block_size)).clipped(mesh_data_box);

						for (unsigned int box_index = 0; box_index < input_count; ++box_index) {
							Box3i box = boxes_to_generate[box_index];
							// Remainder boxes are added to the end of the list
							box.difference_to_vec(block_box, boxes_to_generate);
#ifdef DEBUG_ENABLED
							// Difference should add boxes to the vector, not remove any
							CRASH_COND(box_index >= boxes_to_generate.size());
#endif
						}

						// Remove input boxes
						boxes_to_generate.erase(boxes_to_generate.begin(), boxes_to_generate.begin() + input_count);
					}
				}
			}
		}
	}

	const Vector3i origin_in_voxels =
			mesh_block_pos * (area_info.mesh_block_size_factor * data_block_size << lod_index) -
			Vector3iUtil::create(min_padding << lod_index);

	for (Box3i &box : boxes_to_generate) {
		box.pos += Vector3iUtil::create(min_padding);
	}

	if (out_origin_in_voxels != nullptr) {
		*out_origin_in_voxels = origin_in_voxels;
	}

	if (out_boxes_to_generate != nullptr) {
		// Delegate generation to the caller
		append_array(*out_boxes_to_generate, boxes_to_generate);

	} else {
		// Complete data with generated voxels on the CPU
		ZN_PROFILE_SCOPE_NAMED("Generate");
		VoxelBufferInternal generated_voxels;

		const VoxelModifierStack &modifiers = voxel_data.get_modifiers();

		for (const Box3i &box : boxes_to_generate) {
			ZN_PROFILE_SCOPE_NAMED("Box");
			// print_line(String("size={0}").format(varray(box.size.to_vec3())));
			generated_voxels.create(box.size);
			// generated_voxels.set_voxel_f(2.0f, box.size.x / 2, box.size.y / 2, box.size.z / 2,
			// VoxelBufferInternal::CHANNEL_SDF);
			VoxelGenerator::VoxelQueryData q{ generated_voxels, (box.pos << lod_index) + origin_in_voxels, lod_index };

			if (generator.is_valid()) {
				generator->generate_block(q);
			}
			modifiers.apply(q.voxel_buffer, AABB(q.origin_in_voxels, q.voxel_buffer.get_size() << lod_index));

			for (unsigned int ci = 0; ci < channels_count; ++ci) {
				dst.copy_from(generated_voxels, Vector3i(), generated_voxels.get_size(), box.pos, channels[ci]);
			}
		}
	}
}

Ref<ArrayMesh> build_mesh(Span<const VoxelMesher::Output::Surface> surfaces, Mesh::PrimitiveType primitive, int flags,
		// This vector indexes surfaces to the material they use (if a surface uses a material but is empty, it
		// won't be added to the mesh)
		std::vector<uint16_t> &mesh_material_indices) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(mesh_material_indices.size() == 0);

	Ref<ArrayMesh> mesh;

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

		mesh_material_indices.push_back(surface.material_index);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
std::atomic_int g_debug_mesh_tasks_count = { 0 };
} // namespace

MeshBlockTask::MeshBlockTask() {
	++g_debug_mesh_tasks_count;
}

MeshBlockTask::~MeshBlockTask() {
	--g_debug_mesh_tasks_count;
}

int MeshBlockTask::debug_get_running_count() {
	return g_debug_mesh_tasks_count;
}

void MeshBlockTask::run(zylann::ThreadedTaskContext &ctx) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(meshing_dependency != nullptr);
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_MSG(meshing_dependency->mesher.is_valid(),
			"Meshing task started without a mesher. Maybe missing on the terrain node?");
#endif

	if (block_generation_use_gpu) {
		if (_stage == 0) {
			gather_voxels_gpu(ctx);
		}
		if (_stage == 1) {
			GenerateBlockGPUTaskResult::convert_to_voxel_buffer(to_span(_gpu_generation_results), _voxels);
			_stage = 2;
		}
		if (_stage == 2) {
			build_mesh();
		}
	} else {
		gather_voxels_cpu();
		build_mesh();
	}
}

void MeshBlockTask::gather_voxels_gpu(zylann::ThreadedTaskContext &ctx) {
	ZN_ASSERT(meshing_dependency != nullptr);
	ZN_ASSERT(data != nullptr);

	Ref<VoxelMesher> mesher = meshing_dependency->mesher;
	const unsigned int min_padding = mesher->get_minimum_padding();
	const unsigned int max_padding = mesher->get_maximum_padding();

	std::vector<Box3i> boxes_to_generate;
	Vector3i origin_in_voxels;

	copy_block_and_neighbors(to_span(blocks, blocks_count), _voxels, min_padding, max_padding,
			mesher->get_used_channels_mask(), meshing_dependency->generator, *data, lod_index, mesh_block_position,
			&boxes_to_generate, &origin_in_voxels);

	if (boxes_to_generate.size() == 0) {
		_stage = 2;
		return;
	}

	Ref<VoxelGenerator> generator = meshing_dependency->generator;
	ERR_FAIL_COND(generator.is_null());

	VoxelGenerator::VoxelQueryData generator_query{ _voxels, origin_in_voxels, lod_index };
	if (generator->generate_broad_block(generator_query)) {
		_stage = 2;
		return;
	}

	std::shared_ptr<ComputeShader> generator_shader = generator->get_block_rendering_shader();
	ERR_FAIL_COND(generator_shader == nullptr);

	GenerateBlockGPUTask *gpu_task = memnew(GenerateBlockGPUTask);
	gpu_task->boxes_to_generate = std::move(boxes_to_generate);
	gpu_task->generator_shader = generator_shader;
	gpu_task->generator_shader_params = generator->get_block_rendering_shader_parameters();
	gpu_task->generator_shader_outputs = generator->get_block_rendering_shader_outputs();
	gpu_task->lod_index = lod_index;
	gpu_task->origin_in_voxels = origin_in_voxels;
	gpu_task->consumer_task = this;

	const AABB aabb_voxels(to_vec3(origin_in_voxels), to_vec3(_voxels.get_size() << lod_index));
	std::vector<VoxelModifier::ShaderData> modifiers_shader_data;
	const VoxelModifierStack &modifiers = data->get_modifiers();
	modifiers.apply_for_gpu_rendering(modifiers_shader_data, aabb_voxels, VoxelModifier::ShaderData::TYPE_BLOCK);
	for (const VoxelModifier::ShaderData &d : modifiers_shader_data) {
		gpu_task->modifiers.push_back(
				GenerateBlockGPUTask::ModifierData{ d.shader_rids[VoxelModifier::ShaderData::TYPE_BLOCK], d.params });
	}

	ctx.status = ThreadedTaskContext::STATUS_TAKEN_OUT;

	// Start GPU task, we'll continue meshing after it
	VoxelEngine::get_singleton().push_gpu_task(gpu_task);
}

void MeshBlockTask::set_gpu_results(std::vector<GenerateBlockGPUTaskResult> &&results) {
	_gpu_generation_results = std::move(results);
	_stage = 1;
}

void MeshBlockTask::gather_voxels_cpu() {
	ZN_ASSERT(meshing_dependency != nullptr);
	ZN_ASSERT(data != nullptr);

	Ref<VoxelMesher> mesher = meshing_dependency->mesher;
	const unsigned int min_padding = mesher->get_minimum_padding();
	const unsigned int max_padding = mesher->get_maximum_padding();

	copy_block_and_neighbors(to_span(blocks, blocks_count), _voxels, min_padding, max_padding,
			mesher->get_used_channels_mask(), meshing_dependency->generator, *data, lod_index, mesh_block_position,
			nullptr, nullptr);

	// Could cache generator data from here if it was safe to write into the map
	/*if (data != nullptr && cache_generated_blocks) {
		const CubicAreaInfo area_info = get_cubic_area_info_from_size(blocks.size());
		ERR_FAIL_COND(!area_info.is_valid());

		VoxelDataLodMap::Lod &lod = data->lods[lod_index];

		// Note, this box does not include neighbors!
		const Vector3i min_bpos = position * area_info.mesh_block_size_factor;
		const Vector3i max_bpos = min_bpos + Vector3iUtil::create(area_info.edge_size - 2);

		Vector3i bpos;
		for (bpos.z = min_bpos.z; bpos.z < max_bpos.z; ++bpos.z) {
			for (bpos.x = min_bpos.x; bpos.x < max_bpos.x; ++bpos.x) {
				for (bpos.y = min_bpos.y; bpos.y < max_bpos.y; ++bpos.y) {
					// {
					// 	RWLockRead rlock(lod.map_lock);
					// 	VoxelDataBlock *block = lod.map.get_block(bpos);
					// 	if (block != nullptr && (block->is_edited() || block->is_modified())) {
					// 		continue;
					// 	}
					// }
					std::shared_ptr<VoxelBufferInternal> &cache_buffer = make_shared_instance<VoxelBufferInternal>();
					cache_buffer->copy_format(voxels);
					const Vector3i min_src_pos =
							(bpos - min_bpos) * data_block_size + Vector3iUtil::create(min_padding);
					cache_buffer->copy_from(voxels, min_src_pos, min_src_pos + cache_buffer->get_size(), Vector3i());
					// TODO Where to put voxels? Can't safely write to data at the moment.
				}
			}
		}
	}*/
}

void MeshBlockTask::build_mesh() {
	Ref<VoxelMesher> mesher = meshing_dependency->mesher;
	const Vector3i mesh_block_size =
			_voxels.get_size() - Vector3iUtil::create(mesher->get_minimum_padding() + mesher->get_maximum_padding());

	const Vector3i origin_in_voxels = mesh_block_position * (mesh_block_size << lod_index);

	const VoxelMesher::Input input = { _voxels, meshing_dependency->generator.ptr(), data.get(), origin_in_voxels,
		lod_index, collision_hint, lod_hint, true };
	mesher->build(_surfaces_output, input);

	const bool mesh_is_empty = VoxelMesher::is_mesh_empty(_surfaces_output.surfaces);

	// Currently, Transvoxel only is supported in combination with detail normalmap texturing, because the algorithm
	// provides a cheap source for cells subdividing the mesh. It should be possible to obtain cells from any mesh,
	// but it is more expensive to find them from scratch, and for now Transvoxel is the most viable algorithm for
	// smooth terrain.
	Ref<VoxelMesherTransvoxel> transvoxel_mesher = mesher;

	if (transvoxel_mesher.is_valid() && detail_texture_settings.enabled && !mesh_is_empty &&
			lod_index >= detail_texture_settings.begin_lod_index && require_detail_texture) {
		ZN_PROFILE_SCOPE_NAMED("Schedule virtual render");

		const transvoxel::MeshArrays &mesh_arrays = VoxelMesherTransvoxel::get_mesh_cache_from_current_thread();
		Span<const transvoxel::CellInfo> cell_infos = VoxelMesherTransvoxel::get_cell_info_from_current_thread();
		ZN_ASSERT(cell_infos.size() > 0 && mesh_arrays.vertices.size() > 0);

		UniquePtr<TransvoxelCellIterator> cell_iterator = make_unique_instance<TransvoxelCellIterator>(cell_infos);

		std::shared_ptr<DetailTextureOutput> detail_textures = make_shared_instance<DetailTextureOutput>();
		detail_textures->valid = false;
		// This is stored here in case detail texture rendering completes before the output of the current task gets
		// dequeued in the main thread, since it runs in a separate asynchronous task
		_detail_textures = detail_textures;

		RenderDetailTextureTask *nm_task = ZN_NEW(RenderDetailTextureTask);
		nm_task->cell_iterator = std::move(cell_iterator);
		nm_task->mesh_vertices = mesh_arrays.vertices;
		nm_task->mesh_normals = mesh_arrays.normals;
		nm_task->mesh_indices = mesh_arrays.indices;
		if (detail_texture_generator_override.is_valid()) {
			nm_task->generator = lod_index >= detail_texture_generator_override_begin_lod_index
					? detail_texture_generator_override
					: meshing_dependency->generator;
		} else {
			nm_task->generator = meshing_dependency->generator;
		}
		nm_task->voxel_data = data;
		nm_task->mesh_block_size = mesh_block_size;
		nm_task->lod_index = lod_index;
		nm_task->mesh_block_position = mesh_block_position;
		nm_task->volume_id = volume_id;
		nm_task->output_textures = detail_textures;
		nm_task->detail_texture_settings = detail_texture_settings;
		nm_task->priority_dependency = priority_dependency;
		nm_task->use_gpu = detail_texture_use_gpu;

		VoxelEngine::get_singleton().push_async_task(nm_task);
	}

	if (VoxelEngine::get_singleton().is_threaded_graphics_resource_building_enabled()) {
		// This shall only run if Godot supports building meshes from multiple threads
		_mesh = zylann::voxel::build_mesh(to_span(_surfaces_output.surfaces), _surfaces_output.primitive_type,
				_surfaces_output.mesh_flags, _mesh_material_indices);
		_has_mesh_resource = true;

	} else {
		_has_mesh_resource = false;
	}

	_has_run = true;
}

TaskPriority MeshBlockTask::get_priority() {
	float closest_viewer_distance_sq;
	const TaskPriority p =
			priority_dependency.evaluate(lod_index, constants::TASK_PRIORITY_MESH_BAND2, &closest_viewer_distance_sq);
	_too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool MeshBlockTask::is_cancelled() {
	return !meshing_dependency->valid || _too_far;
}

void MeshBlockTask::apply_result() {
	if (VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		// It is assumed that if a dependency is changed, a new copy of it is made and the old one is marked
		// invalid.
		if (meshing_dependency->valid) {
			VoxelEngine::BlockMeshOutput o;
			// TODO Check for invalidation due to property changes

			if (_has_run) {
				o.type = VoxelEngine::BlockMeshOutput::TYPE_MESHED;
			} else {
				o.type = VoxelEngine::BlockMeshOutput::TYPE_DROPPED;
			}

			o.position = mesh_block_position;
			o.lod = lod_index;
			o.surfaces = std::move(_surfaces_output);
			o.mesh = _mesh;
			o.mesh_material_indices = std::move(_mesh_material_indices);
			o.has_mesh_resource = _has_mesh_resource;
			o.detail_textures = _detail_textures;

			VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(volume_id);
			ERR_FAIL_COND(callbacks.mesh_output_callback == nullptr);
			ERR_FAIL_COND(callbacks.data == nullptr);
			callbacks.mesh_output_callback(callbacks.data, o);
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Mesh request response came back but volume wasn't found");
	}
}

} // namespace zylann::voxel
