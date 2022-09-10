#include "generate_distance_normalmap_task.h"
#include "../util/profiling.h"
#include "voxel_engine.h"
//#include "../util/string_funcs.h" // Debug
#include "../constants/voxel_constants.h"

namespace zylann::voxel {

/*static void debug_dump_atlas(Vector<Ref<Image>> images, String fpath) {
	if (images.size() == 0) {
		return;
	}
	const int tile_width = images[0]->get_width();
	const int tile_height = images[0]->get_height();
	const int sqri = int(Math::ceil(Math::sqrt(float(images.size()))));
	const int atlas_width = sqri * tile_width;
	const int atlas_height = sqri * tile_height;
	Ref<Image> atlas;
	atlas.instantiate();
	atlas->create(atlas_width, atlas_height, false, images[0]->get_format());
	for (int layer_index = 0; layer_index < images.size(); ++layer_index) {
		Ref<Image> im = images[layer_index];
		const int xi = layer_index % sqri;
		const int yi = layer_index / sqri;
		atlas->blit_rect(
				im, Rect2(0, 0, im->get_width(), im->get_height()), Vector2(xi * tile_width, yi * tile_height));
	}
	const int err = atlas->save_png(fpath);
	if (err != OK) {
		print_line(String("Could not save {0}, err {1}").format(varray(fpath, err)));
	}
}*/

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

	const Vector3i origin_in_voxels = mesh_block_position * (mesh_block_size << lod_index);

	compute_normalmap(*cell_iterator, to_span(mesh_vertices), to_span(mesh_normals), to_span(mesh_indices),
			tls_normalmap_data, tile_resolution, **generator, voxel_data.get(), origin_in_voxels, lod_index,
			virtual_texture_settings.octahedral_encoding_enabled);

	NormalMapImages images = store_normalmap_data_to_images(
			tls_normalmap_data, tile_resolution, mesh_block_size, virtual_texture_settings.octahedral_encoding_enabled);

	// Debug
	// debug_dump_atlas(images.atlas,
	// 		String("debug_data/debug_normalmap_atlas_{0}_{1}_{2}_lod{3}_n{4}.png")
	// 				.format(varray(mesh_block_position.x, mesh_block_position.y, mesh_block_position.z, lod_index,
	// 						images.atlas.size())));
	// if (images.atlas.is_valid()) {
	// 	images.atlas->save_png(String("debug_data/debug_normalmap_atlas_{0}_{1}_{2}_lod{3}_n{4}.png")
	// 								   .format(varray(mesh_block_position.x, mesh_block_position.y,
	// 										   mesh_block_position.z, lod_index, tls_normalmap_data.tiles.size())));
	// }
	// if (images.lookup.is_valid()) {
	// 	images.lookup->save_png(String("debug_data/debug_lookup_{0}_{1}_{2}_lod{3}.png")
	// 									.format(varray(mesh_block_position.x, mesh_block_position.y,
	// 											mesh_block_position.z, lod_index)));
	// }

	if (VoxelEngine::get_singleton().is_threaded_graphics_resource_building_enabled()) {
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

	o.position = mesh_block_position;
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
