#include "render_detail_texture_task.h"
#include "../util/profiling.h"
#include "voxel_engine.h"
//#include "../util/string_funcs.h" // Debug
#include "../constants/voxel_constants.h"
#include "../modifiers/voxel_modifier_stack.h"
#include "../storage/voxel_data.h"
#include "../util/math/conv.h"
#include "render_detail_texture_gpu_task.h"

namespace zylann::voxel {

namespace {
DetailTextureData &get_tls_normalmap_data() {
	static thread_local DetailTextureData tls_normalmap_data;
	return tls_normalmap_data;
}
} // namespace

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

void RenderDetailTextureTask::run(ThreadedTaskContext ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(generator.is_valid());
	ZN_ASSERT_RETURN(output_textures != nullptr);
	ZN_ASSERT_RETURN(output_textures->valid == false);
	ZN_ASSERT_RETURN(cell_iterator != nullptr);

	if (use_gpu) {
		run_on_gpu();
	} else {
		run_on_cpu();
	}
}

void RenderDetailTextureTask::run_on_cpu() {
	DetailTextureData &normalmap_data = get_tls_normalmap_data();
	normalmap_data.clear();

	const unsigned int tile_resolution = get_detail_texture_tile_resolution_for_lod(detail_texture_settings, lod_index);

	// LOD0 coordinates
	const Vector3i size_in_voxels = mesh_block_size << lod_index;
	const Vector3i origin_in_voxels = mesh_block_position * size_in_voxels;

	compute_detail_texture_data(*cell_iterator, to_span(mesh_vertices), to_span(mesh_normals), to_span(mesh_indices),
			normalmap_data, tile_resolution, **generator, voxel_data.get(), origin_in_voxels, size_in_voxels, lod_index,
			detail_texture_settings.octahedral_encoding_enabled,
			math::deg_to_rad(float(detail_texture_settings.max_deviation_degrees)), false);

	DetailImages images = store_normalmap_data_to_images(
			normalmap_data, tile_resolution, mesh_block_size, detail_texture_settings.octahedral_encoding_enabled);

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
		DetailTextures textures = store_normalmap_data_to_textures(images);
		output_textures->textures = textures;
	} else {
		output_textures->images = images;
	}

	output_textures->valid = true;
}

void RenderDetailTextureTask::apply_result() {
	if (use_gpu) {
		return;
	}

	if (!VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Normalmap task completed but volume wasn't found");
		return;
	}

	VoxelEngine::BlockDetailTextureOutput o;
	// TODO Check for invalidation due to property changes

	o.position = mesh_block_position;
	o.lod_index = lod_index;
	o.detail_textures = output_textures;

	VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(volume_id);
	ZN_ASSERT_RETURN(callbacks.mesh_output_callback != nullptr);
	ZN_ASSERT_RETURN(callbacks.data != nullptr);
	callbacks.virtual_texture_output_callback(callbacks.data, o);
}

TaskPriority RenderDetailTextureTask::get_priority() {
	// Priority by distance, but after meshes
	TaskPriority p = priority_dependency.evaluate(lod_index, constants::TASK_PRIORITY_VIRTUAL_TEXTURES_BAND2, nullptr);
	return p;
}

bool RenderDetailTextureTask::is_cancelled() {
	// TODO Cancel if too far?
	return false;
}

static void build_gpu_tiles_data(ICellIterator &cell_iterator, unsigned int tile_count,
		std::vector<int32_t> &cell_triangles, std::vector<RenderDetailTextureGPUTask::TileData> &tile_data,
		const std::vector<int> &mesh_indices, const std::vector<Vector3f> &mesh_normals) {
	tile_data.reserve(tile_count);

	CurrentCellInfo cell_info;
	while (cell_iterator.next(cell_info)) {
		const unsigned int chunk_index = cell_triangles.size();

		Vector3f normal_sum;

		for (unsigned int i = 0; i < cell_info.triangle_count; ++i) {
			const unsigned int ii0 = cell_info.triangle_begin_indices[i];
			cell_triangles.push_back(ii0 / 3);

			const unsigned int i0 = mesh_indices[ii0];
			const unsigned int i1 = mesh_indices[ii0 + 1];
			const unsigned int i2 = mesh_indices[ii0 + 2];

			const Vector3f n0 = mesh_normals[i0];
			const Vector3f n1 = mesh_normals[i1];
			const Vector3f n2 = mesh_normals[i2];

			normal_sum += n0;
			normal_sum += n1;
			normal_sum += n2;
		}

		const unsigned int projection = math::get_longest_axis(normal_sum);

		RenderDetailTextureGPUTask::TileData td;
		td.cell_x = cell_info.position.x;
		td.cell_y = cell_info.position.y;
		td.cell_z = cell_info.position.z;
#ifdef DEBUG_ENABLED
		ZN_ASSERT(chunk_index < ((1 << 24) - 1));
		ZN_ASSERT(cell_info.triangle_count <= 5);
		ZN_ASSERT(projection <= 2);
#endif
		td.data = (chunk_index << 8) | (cell_info.triangle_count << 4) | projection;

		tile_data.push_back(td);
	}
}

void RenderDetailTextureTask::run_on_gpu() {
	ZN_PROFILE_SCOPE();
	RenderDetailTextureGPUTask *gpu_task = make_gpu_task();
	VoxelEngine::get_singleton().push_gpu_task(gpu_task);
}

RenderDetailTextureGPUTask *RenderDetailTextureTask::make_gpu_task() {
	const unsigned int tile_resolution = get_detail_texture_tile_resolution_for_lod(detail_texture_settings, lod_index);

	// Fallback on CPU for tiles containing edited voxels.
	// TODO Figure out an efficient way to have sparse voxel data available on the GPU
	const Vector3i size_in_voxels = mesh_block_size << lod_index;
	const Vector3i origin_in_voxels = mesh_block_position * size_in_voxels;
	DetailTextureData edited_tiles_normalmap_data;
	compute_detail_texture_data(*cell_iterator, to_span(mesh_vertices), to_span(mesh_normals), to_span(mesh_indices),
			edited_tiles_normalmap_data, tile_resolution, **generator, voxel_data.get(), origin_in_voxels,
			size_in_voxels, lod_index, false, /*virtual_texture_settings.octahedral_encoding_enabled*/
			math::deg_to_rad(float(detail_texture_settings.max_deviation_degrees)), true);

	const unsigned int tile_count = cell_iterator->get_count();
	const unsigned int tiles_across = get_square_grid_size_from_item_count(tile_count);
	const unsigned int pixels_across = tiles_across * tile_resolution;

	std::vector<RenderDetailTextureGPUTask::TileData> tile_data;
	std::vector<int32_t> cell_triangles;
	cell_iterator->rewind();
	build_gpu_tiles_data(*cell_iterator, tile_count, cell_triangles, tile_data, mesh_indices, mesh_normals);
	ZN_ASSERT(cell_triangles.size() > 0);

	RenderDetailTextureGPUTask::Params params;
	params.block_origin_world = to_vec3f(origin_in_voxels);
	params.pixel_world_step = float(1 << lod_index) / float(tile_resolution);
	params.max_deviation_cosine = Math::cos(math::deg_to_rad(float(detail_texture_settings.max_deviation_degrees)));
	params.max_deviation_sine = Math::sin(math::deg_to_rad(float(detail_texture_settings.max_deviation_degrees)));
	params.tiles_x = tiles_across;
	params.tiles_y = tiles_across;
	params.tile_size_pixels = tile_resolution;

	// Create GPU task

	RenderDetailTextureGPUTask *gpu_task = ZN_NEW(RenderDetailTextureGPUTask);
	gpu_task->texture_width = pixels_across;
	gpu_task->texture_height = pixels_across;
	// TODO Mesh data need std::move or std::shared_ptr, we only read it
	gpu_task->mesh_indices = mesh_indices;

	std::vector<Vector4f> &dst_vertices = gpu_task->mesh_vertices;
	dst_vertices.reserve(mesh_vertices.size());
	for (const Vector3f v : mesh_vertices) {
		dst_vertices.push_back(Vector4f(v.x, v.y, v.z, 0.f));
	}

	if (voxel_data != nullptr) {
		const AABB aabb_voxels(to_vec3(origin_in_voxels), to_vec3(mesh_block_size << lod_index));
		std::vector<VoxelModifier::ShaderData> modifiers_shader_data;
		const VoxelModifierStack &modifiers = voxel_data->get_modifiers();
		modifiers.apply_for_detail_gpu_rendering(modifiers_shader_data, aabb_voxels);
		for (const VoxelModifier::ShaderData &d : modifiers_shader_data) {
			gpu_task->modifiers.push_back(
					RenderDetailTextureGPUTask::ModifierData{ d.detail_rendering_shader_rid, d.params });
		}
	}

	gpu_task->cell_triangles = std::move(cell_triangles);
	gpu_task->tile_data = std::move(tile_data);
	gpu_task->params = params;
	gpu_task->shader = generator->get_virtual_rendering_shader();
	gpu_task->shader_params = generator->get_virtual_rendering_shader_parameters();
	gpu_task->output = output_textures;
	gpu_task->edited_tiles_texture_data = std::move(edited_tiles_normalmap_data);
	gpu_task->block_position = mesh_block_position;
	gpu_task->block_size = mesh_block_size;
	gpu_task->lod_index = lod_index;
	gpu_task->volume_id = volume_id;

	return gpu_task;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// PackedByteArray convert_pixels_from_rgba8_to_rgb8(const PackedByteArray &src) {
// 	ZN_ASSERT((src.size() % 4) == 0);
// 	const unsigned int pixel_count = src.size() / 4;
// 	PackedByteArray dst;
// 	uint8_t *dst_w = dst.ptrw();
// 	const uint8_t *src_r = src.ptr();
// 	for (unsigned int pi = 0; pi < pixel_count; ++pi) {
// 		const unsigned int src_i = pi * 4;
// 		const unsigned int dst_i = pi * 3;
// 		dst_w[dst_i] = src_r[src_i];
// 		dst_w[dst_i + 1] = src_r[src_i + 1];
// 		dst_w[dst_i + 2] = src_r[src_i + 2];
// 	}
// 	return dst;
// }

void convert_pixels_from_rgba8_to_rgb8_in_place(PackedByteArray &pba) {
	ZN_ASSERT((pba.size() % 4) == 0);
	const unsigned int pixel_count = pba.size() / 4;
	uint8_t *pba_w = pba.ptrw();
	for (unsigned int pi = 0; pi < pixel_count; ++pi) {
		const unsigned int src_i = pi * 4;
		const unsigned int dst_i = pi * 3;
		pba_w[dst_i] = pba_w[src_i];
		pba_w[dst_i + 1] = pba_w[src_i + 1];
		pba_w[dst_i + 2] = pba_w[src_i + 2];
	}
	pba.resize(pixel_count * 3);
}

void combine_edited_tiles(PackedByteArray &atlas_data, unsigned int tile_size_pixels, Vector2i atlas_size_pixels,
		const DetailTextureData &edited_tiles_normalmap_data) {
	ZN_PROFILE_SCOPE();

	uint8_t *dst_w = atlas_data.ptrw();
	Span<uint8_t> dst(dst_w, atlas_data.size());

	const unsigned int tile_size_in_bytes = math::squared(tile_size_pixels) * 3;
	const unsigned int tiles_x = atlas_size_pixels.x / tile_size_pixels;

	for (unsigned int tile_index = 0; tile_index < edited_tiles_normalmap_data.tiles.size(); ++tile_index) {
		Span<const uint8_t> src_pixels = to_span_from_position_and_size(
				edited_tiles_normalmap_data.normals, tile_index * tile_size_in_bytes, tile_size_in_bytes);

		const unsigned int dst_tile_index = edited_tiles_normalmap_data.tile_indices[tile_index];
		const Vector2i dst_tile_pos_pixels =
				Vector2i(dst_tile_index % tiles_x, dst_tile_index / tiles_x) * tile_size_pixels;

		copy_2d_region_from_packed_to_atlased(dst, atlas_size_pixels, src_pixels,
				Vector2i(tile_size_pixels, tile_size_pixels), dst_tile_pos_pixels, 3);
	}
}

void RenderDetailTexturePass2Task::run(ThreadedTaskContext ctx) {
	ZN_PROFILE_SCOPE();

	// TODO Suggestion: given how fast GPU normalmaps are computed, maybe we could output them first,
	// and get the edits later, even if that means computing tiles redundantly, because at least we get a
	// result quicker rather than a "hole" of lack of detail

	convert_pixels_from_rgba8_to_rgb8_in_place(atlas_data);

	// TODO Optimization: currently, the GPU task still generates tiles that would otherwise be replaced with edited
	// tiles. Maybe we should find a way to tell the GPU task to exclude these tiles efficiently?
	if (edited_tiles_texture_data.tiles.size() > 0) {
		combine_edited_tiles(
				atlas_data, tile_size_pixels, Vector2i(atlas_width, atlas_height), edited_tiles_texture_data);
	}

	DetailImages images;

	// TODO Octahedral compression
	images.atlas = Image::create_from_data(atlas_width, atlas_height, false, Image::FORMAT_RGB8, atlas_data);
	ERR_FAIL_COND(images.atlas.is_null());
	// images.atlas->convert(Image::FORMAT_RGB8);

	images.lookup = store_lookup_to_image(tile_data, mesh_block_size);

	if (VoxelEngine::get_singleton().is_threaded_graphics_resource_building_enabled()) {
		DetailTextures textures = store_normalmap_data_to_textures(images);
		output_textures->textures = textures;
	} else {
		output_textures->images = images;
	}

	output_textures->valid = true;
}

void RenderDetailTexturePass2Task::apply_result() {
	if (!VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Normalmap task completed but volume wasn't found");
		return;
	}

	VoxelEngine::BlockDetailTextureOutput o;
	// TODO Check for invalidation due to property changes

	o.position = mesh_block_position;
	o.lod_index = lod_index;
	o.detail_textures = output_textures;

	VoxelEngine::VolumeCallbacks callbacks = VoxelEngine::get_singleton().get_volume_callbacks(volume_id);
	ZN_ASSERT_RETURN(callbacks.mesh_output_callback != nullptr);
	ZN_ASSERT_RETURN(callbacks.data != nullptr);
	callbacks.virtual_texture_output_callback(callbacks.data, o);
}

} // namespace zylann::voxel
