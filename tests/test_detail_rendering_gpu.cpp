#include "../engine/mesh_block_task.h"
#include "../engine/render_detail_texture_gpu_task.h"
#include "../engine/render_detail_texture_task.h"
#include "../engine/voxel_engine.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../meshers/transvoxel/transvoxel_cell_iterator.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "testing.h"

namespace zylann::voxel::tests {

void test_normalmap_render_gpu() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	{
		pg::VoxelGraphFunction &g = **generator->get_main_function();

		// Flat plane
		// const uint32_t n_y = g.create_node(pg::VoxelGraphFunction::NODE_INPUT_Y, Vector2());
		// const uint32_t n_add = g.create_node(pg::VoxelGraphFunction::NODE_ADD, Vector2());
		// const uint32_t n_out_sd = g.create_node(pg::VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		// g.add_connection(n_y, 0, n_add, 0);
		// g.set_node_default_input(n_add, 1, -1.5f);
		// g.add_connection(n_add, 0, n_out_sd, 0);

		// Wavy plane
		// X --- Sin1 --- Add1 --- Add2 --- Add3 --- OutSDF
		//               /        /       /
		//     Z --- Sin2        Y     -3.5
		//
		const uint32_t n_x = g.create_node(pg::VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_y = g.create_node(pg::VoxelGraphFunction::NODE_INPUT_Y, Vector2());
		const uint32_t n_z = g.create_node(pg::VoxelGraphFunction::NODE_INPUT_Z, Vector2());
		const uint32_t n_add1 = g.create_node(pg::VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_add2 = g.create_node(pg::VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_add3 = g.create_node(pg::VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_sin1 = g.create_node(pg::VoxelGraphFunction::NODE_SIN, Vector2());
		const uint32_t n_sin2 = g.create_node(pg::VoxelGraphFunction::NODE_SIN, Vector2());
		const uint32_t n_out_sd = g.create_node(pg::VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_x, 0, n_sin1, 0);
		g.add_connection(n_z, 0, n_sin2, 0);
		g.add_connection(n_sin1, 0, n_add1, 0);
		g.add_connection(n_sin2, 0, n_add1, 1);
		g.add_connection(n_add1, 0, n_add2, 0);
		g.add_connection(n_y, 0, n_add2, 1);
		g.add_connection(n_add2, 0, n_add3, 0);
		g.set_node_default_input(n_add3, 1, -1.5f);
		g.add_connection(n_add3, 0, n_out_sd, 0);

		pg::CompilationResult result = generator->compile(false);
		ZN_TEST_ASSERT(result.success);
	}

	generator->compile_shaders();

	Ref<VoxelMesherTransvoxel> mesher;
	mesher.instantiate();

	const int block_size = 16;
	VoxelBufferInternal voxels;
	voxels.create(Vector3iUtil::create(block_size + mesher->get_minimum_padding() + mesher->get_maximum_padding()));

	const Vector3i origin_in_voxels;
	const uint8_t lod_index = 0;
	generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels, origin_in_voxels, 0 });

	const VoxelMesher::Input mesher_input = { voxels, generator.ptr(), nullptr, origin_in_voxels, lod_index, false,
		false, true };
	VoxelMesher::Output mesher_output;
	mesher->build(mesher_output, mesher_input);

	const bool mesh_is_empty = VoxelMesher::is_mesh_empty(mesher_output.surfaces);
	ZN_TEST_ASSERT(!mesh_is_empty);

	const transvoxel::MeshArrays &mesh_arrays = VoxelMesherTransvoxel::get_mesh_cache_from_current_thread();
	Span<const transvoxel::CellInfo> cell_infos = VoxelMesherTransvoxel::get_cell_info_from_current_thread();
	ZN_ASSERT(cell_infos.size() > 0 && mesh_arrays.vertices.size() > 0);

	UniquePtr<TransvoxelCellIterator> cell_iterator = make_unique_instance<TransvoxelCellIterator>(cell_infos);

	std::shared_ptr<DetailTextureOutput> detail_textures = make_shared_instance<DetailTextureOutput>();
	detail_textures->valid = false;

	DetailRenderingSettings detail_texture_settings;
	detail_texture_settings.enabled = true;
	detail_texture_settings.begin_lod_index = 0;
	detail_texture_settings.max_deviation_degrees = 60;
	detail_texture_settings.tile_resolution_min = 16;
	detail_texture_settings.tile_resolution_max = 16;
	detail_texture_settings.octahedral_encoding_enabled = false;

	RenderDetailTextureTask nm_task;
	nm_task.cell_iterator = std::move(cell_iterator);
	nm_task.mesh_vertices = mesh_arrays.vertices;
	nm_task.mesh_normals = mesh_arrays.normals;
	nm_task.mesh_indices = mesh_arrays.indices;
	nm_task.generator = generator;
	nm_task.voxel_data = nullptr;
	nm_task.mesh_block_size = Vector3iUtil::create(block_size);
	nm_task.lod_index = lod_index;
	nm_task.mesh_block_position = Vector3i();
	nm_task.output_textures = detail_textures;
	nm_task.detail_texture_settings = detail_texture_settings;
	nm_task.priority_dependency;
	nm_task.use_gpu = true;
	RenderDetailTextureGPUTask *gpu_task = nm_task.make_gpu_task();

	RenderingDevice &rd = VoxelEngine::get_singleton().get_rendering_device();
	GPUStorageBufferPool storage_buffer_pool;
	storage_buffer_pool.set_rendering_device(&rd);

	GPUTaskContext gpu_task_context{ rd, storage_buffer_pool };
	gpu_task->prepare(gpu_task_context);

	rd.submit();
	rd.sync();

	const PackedByteArray atlas_texture_data = gpu_task->collect_texture_and_cleanup(rd, storage_buffer_pool);

	Ref<Image> gpu_atlas_image = Image::create_from_data(
			gpu_task->texture_width, gpu_task->texture_height, false, Image::FORMAT_RGBA8, atlas_texture_data);
	ERR_FAIL_COND(gpu_atlas_image.is_null());
	gpu_atlas_image->convert(Image::FORMAT_RGB8);

	ZN_DELETE(gpu_task);

	// Make a comparison with the CPU version

	nm_task.cell_iterator->rewind();
	DetailTextureData detail_textures_data;

	compute_detail_texture_data(*nm_task.cell_iterator, to_span(nm_task.mesh_vertices), to_span(nm_task.mesh_normals),
			to_span(nm_task.mesh_indices), detail_textures_data, detail_texture_settings.tile_resolution_min,
			**generator, nm_task.voxel_data.get(), origin_in_voxels, mesher_input.voxels.get_size(), lod_index,
			detail_texture_settings.octahedral_encoding_enabled,
			math::deg_to_rad(float(detail_texture_settings.max_deviation_degrees)), false);

	DetailImages images =
			store_normalmap_data_to_images(detail_textures_data, detail_texture_settings.tile_resolution_min,
					nm_task.mesh_block_size, detail_texture_settings.octahedral_encoding_enabled);
	ZN_ASSERT(images.atlas.is_valid());
	Ref<Image> cpu_atlas_image = images.atlas;

	// Analyze

	struct L {
		static float compare(const Image &im1, const Image &im2) {
			ZN_ASSERT(im1.get_size() == im2.get_size());
			const Vector2i size = im1.get_size();
			float dsum = 0.f;
			int counted_pixels = 0;
			for (int y = 0; y < size.y; ++y) {
				for (int x = 0; x < size.x; ++x) {
					const Color c1 = im1.get_pixel(x, y);
					const Color c2 = im2.get_pixel(x, y);
					const float d = Math::sqrt(math::squared(c1.r - c2.r) + math::squared(c1.g - c2.g) +
							math::squared(c1.b - c2.b) + math::squared(c1.a - c2.a));
					dsum += d;
					++counted_pixels;
				}
			}
			return dsum / float(counted_pixels);
		}
	};

	storage_buffer_pool.clear();

	// Debug dumps
	// gpu_atlas_image->save_png("test_gpu_normalmap.png");
	// cpu_atlas_image->save_png("test_cpu_normalmap.png");

	// Simple compare for now. Ideally we should analyze more things. A challenge is that the two approaches produce
	// slightly different images, even though they are functionally equivalent.
	const float diff = L::compare(**cpu_atlas_image, **gpu_atlas_image);
	ZN_TEST_ASSERT(diff < 0.1);
}

} // namespace zylann::voxel::tests
