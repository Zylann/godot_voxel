#include "generate_distance_normalmap_task.h"
#include "../util/profiling.h"
#include "voxel_engine.h"
//#include "../util/string_funcs.h" // Debug
#include "../constants/voxel_constants.h"

namespace zylann::voxel {

void GenerateDistanceNormalmapTask::run(ThreadedTaskContext ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(generator.is_valid());
	ZN_ASSERT_RETURN(virtual_textures != nullptr);
	ZN_ASSERT_RETURN(virtual_textures->valid == false);
	ZN_ASSERT_RETURN(cell_iterator != nullptr);

	static thread_local NormalMapData tls_normalmap_data;
	tls_normalmap_data.clear();

	const unsigned int tile_resolution =
			get_virtual_texture_tile_resolution_for_lod(virtual_texture_settings, lod_index);

	compute_normalmap(*cell_iterator, to_span(mesh_vertices), to_span(mesh_normals), to_span(mesh_indices),
			tls_normalmap_data, tile_resolution, **generator, voxel_data.get(), origin_in_voxels, lod_index,
			virtual_texture_settings.octahedral_encoding_enabled);

	NormalMapImages images = store_normalmap_data_to_images(
			tls_normalmap_data, tile_resolution, block_size, virtual_texture_settings.octahedral_encoding_enabled);

	if (VoxelEngine::get_singleton().is_threaded_mesh_resource_building_enabled()) {
		NormalMapTextures textures = store_normalmap_data_to_textures(images);
		virtual_textures->normalmap_textures = textures;
	} else {
		virtual_textures->normalmap_images = images;
	}

	virtual_textures->valid = true;
}

void GenerateDistanceNormalmapTask::apply_result() {
	if (!VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Normalmap task completed but volume wasn't found");
		return;
	}

	VoxelEngine::BlockVirtualTextureOutput o;
	// TODO Check for invalidation due to property changes

	o.position = block_position;
	o.lod_index = lod_index;
	o.virtual_textures = virtual_textures;

	VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(volume_id);
	ZN_ASSERT_RETURN(callbacks.mesh_output_callback != nullptr);
	ZN_ASSERT_RETURN(callbacks.data != nullptr);
	callbacks.virtual_texture_output_callback(callbacks.data, o);
}

TaskPriority GenerateDistanceNormalmapTask::get_priority() {
	// Priority by distance, but after meshes
	TaskPriority p = priority_dependency.evaluate(lod_index, constants::TASK_PRIORITY_VIRTUAL_TEXTURES_BAND2, nullptr);
	return p;
}

bool GenerateDistanceNormalmapTask::is_cancelled() {
	// TODO Cancel if too far?
	return false;
}

} // namespace zylann::voxel
