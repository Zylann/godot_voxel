#include "test_edition_funcs.h"
#include "../../edition/funcs.h"
#include "../../edition/voxel_tool_terrain.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../meshers/blocky/voxel_blocky_library.h"
#include "../../meshers/blocky/voxel_blocky_model_cube.h"
#include "../../meshers/blocky/voxel_blocky_model_mesh.h"
#include "../../storage/voxel_data.h"
#include "../testing.h"
#include "test_util.h"

namespace zylann::voxel::tests {

void test_run_blocky_random_tick() {
	const Box3i voxel_box(Vector3i(-24, -23, -22), Vector3i(64, 40, 40));

	// Create library with tickable voxels
	Ref<VoxelBlockyLibrary> library;
	library.instantiate();

	{
		Ref<VoxelBlockyModelMesh> air;
		air.instantiate();
		library->add_model(air);
	}

	{
		Ref<VoxelBlockyModelCube> non_tickable;
		non_tickable.instantiate();
		library->add_model(non_tickable);
	}

	int tickable_id = -1;
	{
		Ref<VoxelBlockyModel> tickable;
		tickable.instantiate();
		tickable->set_random_tickable(true);
		tickable_id = library->add_model(tickable);
	}

	library->bake();

	// Create test map
	VoxelData data;
	{
		// All blocks of this map will be the same,
		// an interleaving of all block types
		VoxelBuffer model_buffer;
		model_buffer.create(Vector3iUtil::create(data.get_block_size()));
		for (int z = 0; z < model_buffer.get_size().z; ++z) {
			for (int x = 0; x < model_buffer.get_size().x; ++x) {
				for (int y = 0; y < model_buffer.get_size().y; ++y) {
					const int block_id = (x + y + z) % 3;
					model_buffer.set_voxel(block_id, x, y, z, VoxelBuffer::CHANNEL_TYPE);
				}
			}
		}

		const Box3i world_blocks_box(-4, -4, -4, 8, 8, 8);
		world_blocks_box.for_each_cell_zxy([&data, &model_buffer](Vector3i block_pos) {
			std::shared_ptr<VoxelBuffer> buffer = make_shared_instance<VoxelBuffer>();
			buffer->create(model_buffer.get_size());
			buffer->copy_channels_from(model_buffer);
			VoxelDataBlock block(buffer, 0);
			block.set_edited(true);
			ZN_TEST_ASSERT(data.try_set_block(block_pos, block));
		});
	}

	struct Callback {
		Box3i voxel_box;
		Box3i pick_box;
		bool first_pick = true;
		bool ok = true;
		int tickable_id = -1;

		Callback(Box3i p_voxel_box, int p_tickable_id) : voxel_box(p_voxel_box), tickable_id(p_tickable_id) {}

		bool exec(Vector3i pos, int block_id) {
			if (ok) {
				ok = _exec(pos, block_id);
			}
			return ok;
		}

		inline bool _exec(Vector3i pos, int block_id) {
			ZN_TEST_ASSERT_V(block_id == tickable_id, false);
			ZN_TEST_ASSERT_V(voxel_box.contains(pos), false);
			if (first_pick) {
				first_pick = false;
				pick_box = Box3i(pos, Vector3i(1, 1, 1));
			} else {
				pick_box.merge_with(Box3i(pos, Vector3i(1, 1, 1)));
			}
			return true;
		}
	};

	Callback cb(voxel_box, tickable_id);

	RandomPCG random;
	random.seed(131183);
	VoxelToolTerrain::run_blocky_random_tick_static(
			data, voxel_box, **library, random, 1000, 4, &cb, [](void *self, Vector3i pos, int64_t val) {
				Callback *cb = (Callback *)self;
				return cb->exec(pos, val);
			});

	ZN_TEST_ASSERT(cb.ok);

	// Even though there is randomness, we expect to see at least one hit
	ZN_TEST_ASSERT_MSG(!cb.first_pick, "At least one hit is expected, not none");

	// Check that the points were more or less uniformly sparsed within the provided box.
	// They should, because we populated the world with a checkerboard of tickable voxels.
	// There is randomness at play, so unfortunately we may have to use a margin or pick the right seed,
	// and we only check the enclosing area.
	const int error_margin = 0;
	for (int axis_index = 0; axis_index < Vector3iUtil::AXIS_COUNT; ++axis_index) {
		const int nd = cb.pick_box.pos[axis_index] - voxel_box.pos[axis_index];
		const int pd = cb.pick_box.pos[axis_index] + cb.pick_box.size[axis_index] -
				(voxel_box.pos[axis_index] + voxel_box.size[axis_index]);
		ZN_TEST_ASSERT(Math::abs(nd) <= error_margin);
		ZN_TEST_ASSERT(Math::abs(pd) <= error_margin);
	}
}

void test_box_blur() {
	VoxelBuffer voxels;
	voxels.create(64, 64, 64);

	Vector3i pos;
	for (pos.z = 0; pos.z < voxels.get_size().z; ++pos.z) {
		for (pos.x = 0; pos.x < voxels.get_size().x; ++pos.x) {
			for (pos.y = 0; pos.y < voxels.get_size().y; ++pos.y) {
				const float sd = Math::cos(0.53f * pos.x) + Math::sin(0.37f * pos.y) + Math::sin(0.71f * pos.z);
				voxels.set_voxel_f(sd, pos, VoxelBuffer::CHANNEL_SDF);
			}
		}
	}

	const int blur_radius = 3;
	const Vector3f sphere_pos = to_vec3f(voxels.get_size()) / 2.f;
	const float sphere_radius = 64 - blur_radius;

	struct L {
		static void save_image(const VoxelBuffer &vb, int y, const char *name) {
			Ref<Image> im = godot::VoxelBuffer::debug_print_sdf_z_slice(vb, 1.f, y);
			ZN_ASSERT(im.is_valid());
			im->resize(im->get_width() * 4, im->get_height() * 4, Image::INTERPOLATE_NEAREST);
			im->save_png(name);
		}
	};

	// L::save_image(voxels, 32, "test_box_blur_src.png");

	VoxelBuffer voxels_blurred_1;
	ops::box_blur_slow_ref(voxels, voxels_blurred_1, blur_radius, sphere_pos, sphere_radius);
	// L::save_image(voxels_blurred_1, 32 - blur_radius, "test_box_blur_blurred_1.png");

	VoxelBuffer voxels_blurred_2;
	ops::box_blur(voxels, voxels_blurred_2, blur_radius, sphere_pos, sphere_radius);
	// L::save_image(voxels_blurred_2, 32 - blur_radius, "test_box_blur_blurred_2.png");

	ZN_TEST_ASSERT(sd_equals_approx(voxels_blurred_1, voxels_blurred_2));
}

void test_discord_soakil_copypaste() {
	// That was a bug reported on Discord by Soakil.
	//
	// 1) VoxelLodTerrain with data streaming enabled
	// 2) Generate flat SDF terrain with voxel materials set to 1
	// 3) Copy an area into a buffer
	// 4) Add a sphere within the area
	// 5) Paste the buffer back to "undo" the sphere
	//
	// Observed: the sphere remains present, and materials became 0 within the pasted area.
	// Expected: terrain must be in the same state as it was before step 4.
	// Notes: copy didn't work due to a defect in VoxelData ignoring blocks without cached voxels and not falling back
	// on the generator.

	// We can't test nodes like VoxelLodTerrain without an integration test project, but we can test this using the
	// underlying data structures.

	// Generator producing a floating box platform centered on origin
	Ref<VoxelGeneratorGraph> generator;
	{
		generator.instantiate();
		Ref<pg::VoxelGraphFunction> graph = generator->get_main_function();
		ZN_ASSERT(graph.is_valid());

		const uint32_t n_out_sdf = graph->create_node(pg::VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());

		const uint32_t n_out_tex = graph->create_node(pg::VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE, Vector2());
		graph->set_node_default_input(n_out_tex, 0, 1);

		const uint32_t n_x = graph->create_node(pg::VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_y = graph->create_node(pg::VoxelGraphFunction::NODE_INPUT_Y, Vector2());
		const uint32_t n_z = graph->create_node(pg::VoxelGraphFunction::NODE_INPUT_Z, Vector2());

		const uint32_t n_box = graph->create_node(pg::VoxelGraphFunction::NODE_SDF_BOX, Vector2());
		graph->set_node_param(n_box, 0, 50.0);
		graph->set_node_param(n_box, 1, 1.0);
		graph->set_node_param(n_box, 2, 50.0);

		graph->add_connection(n_x, 0, n_box, 0);
		graph->add_connection(n_y, 0, n_box, 1);
		graph->add_connection(n_z, 0, n_box, 2);
		graph->add_connection(n_box, 0, n_out_sdf, 0);

		pg::CompilationResult compilation_result = generator->compile(false);
		ZN_TEST_ASSERT_MSG(compilation_result.success,
				String("Failed to compile graph: {0}: {1}")
						.format(varray(compilation_result.node_id, compilation_result.message)));
	}

	VoxelData voxel_data;
	voxel_data.set_bounds(Box3i(Vector3iUtil::create(-5000), Vector3iUtil::create(10000)));
	voxel_data.set_streaming_enabled(true);
	voxel_data.set_generator(generator);

	const Box3i terrain_blocks_box(Vector3i(-2, -2, -2), Vector3i(4, 4, 4));

	terrain_blocks_box.for_each_cell([&voxel_data, &generator](Vector3i bpos) {
		// std::shared_ptr<VoxelBuffer> vb = make_shared_instance<VoxelBuffer>();
		// vb->create(Vector3iUtil::create(1 << constants::DEFAULT_BLOCK_SIZE_PO2));
		// VoxelGenerator::VoxelQueryData q{ *vb, bpos << constants::DEFAULT_BLOCK_SIZE_PO2, 0 };
		// generator->generate_block(q);
		VoxelDataBlock block;
		// block.set_voxels(vb);
		// We signal that this block is loaded but doesn't have voxel data, therefore the generator should be used on
		// the fly
		const bool inserted = voxel_data.try_set_block(bpos, block);
		ZN_ASSERT(inserted);
	});

	struct L {
		static void check_original(VoxelData &vd, float sdf_dequantize) {
			// Air above platform
			const float sd_above_platform =
					vd.get_voxel_f(Vector3i(0, 5, 0), VoxelBuffer::CHANNEL_SDF) * sdf_dequantize;
			ZN_TEST_ASSERT(sd_above_platform > 0.01f);

			// Matter in platform
			const float sd_in_platform = vd.get_voxel_f(Vector3i(0, 0, 0), VoxelBuffer::CHANNEL_SDF) * sdf_dequantize;
			ZN_TEST_ASSERT(sd_in_platform < -0.01f);

			// Air below platform
			const float sd_below_platform =
					vd.get_voxel_f(Vector3i(0, -5, 0), VoxelBuffer::CHANNEL_SDF) * sdf_dequantize;
			ZN_TEST_ASSERT(sd_below_platform > 0.01f);

			// Material

			VoxelSingleValue default_indices;
			default_indices.i = VoxelBuffer::get_default_value_static(VoxelBuffer::CHANNEL_INDICES);
			VoxelSingleValue default_weights;
			default_weights.i = VoxelBuffer::get_default_value_static(VoxelBuffer::CHANNEL_WEIGHTS);

			const uint16_t packed_indices_in_platform =
					vd.get_voxel(Vector3i(0, 0, 0), VoxelBuffer::CHANNEL_INDICES, default_indices).i;
			const uint16_t packed_weights_in_platform =
					vd.get_voxel(Vector3i(0, 0, 0), VoxelBuffer::CHANNEL_WEIGHTS, default_weights).i;

			check_indices_and_weights_in_platform(packed_indices_in_platform, packed_weights_in_platform);
		}

		static void check_indices_and_weights_in_platform(uint16_t packed_indices, uint16_t packed_weights) {
			const FixedArray<uint8_t, 4> indices_in_platform = decode_indices_from_packed_u16(packed_indices);
			unsigned int expected_material_index_index;
			ZN_TEST_ASSERT(find(indices_in_platform, uint8_t(1), expected_material_index_index));

			const FixedArray<uint8_t, 4> weights_in_platform = decode_weights_from_packed_u16(packed_weights);
			for (unsigned int i = 0; i < weights_in_platform.size(); ++i) {
				if (i == expected_material_index_index) {
					ZN_TEST_ASSERT(weights_in_platform[i] > 0);
				} else {
					ZN_TEST_ASSERT(weights_in_platform[i] == 0);
				}
			}
		}
	};

	// Checks terrain is as we expect
	L::check_original(voxel_data,
			// No edits yet, so SDF won't be quantized...
			1.f);

	VoxelBuffer buffer_before_edit;
	buffer_before_edit.create(Vector3i(20, 20, 20));
	const Vector3i undo_pos(-10, -10, -10);
	voxel_data.copy(undo_pos, buffer_before_edit, 0xff);

	// TODO This sucks, eventually we should have an API that does this automatically?
	const float sdf_dequantize = 1.f /
			VoxelBuffer::get_sdf_quantization_scale(buffer_before_edit.get_channel_depth(VoxelBuffer::CHANNEL_SDF));

	// Check the copy
	{
		const float sd_above_platform =
				buffer_before_edit.get_voxel_f(Vector3i(10, 19, 10), VoxelBuffer::CHANNEL_SDF) * sdf_dequantize;
		ZN_TEST_ASSERT(sd_above_platform > 0.01f);

		const Vector3i pos_in_platform(10, 10, 10);
		const float sd_in_platform =
				buffer_before_edit.get_voxel_f(pos_in_platform, VoxelBuffer::CHANNEL_SDF) * sdf_dequantize;
		ZN_TEST_ASSERT(sd_in_platform < -0.01f);

		const float sd_below_platform =
				buffer_before_edit.get_voxel_f(Vector3i(10, 0, 10), VoxelBuffer::CHANNEL_SDF) * sdf_dequantize;
		ZN_TEST_ASSERT(sd_below_platform > 0.01f);

		const uint16_t packed_indices_in_platform =
				buffer_before_edit.get_voxel(pos_in_platform, VoxelBuffer::CHANNEL_INDICES);
		const uint16_t packed_weights_in_platform =
				buffer_before_edit.get_voxel(pos_in_platform, VoxelBuffer::CHANNEL_WEIGHTS);

		L::check_indices_and_weights_in_platform(packed_indices_in_platform, packed_weights_in_platform);
	}

	{
		ops::DoSphere op;
		op.shape.center = Vector3();
		op.shape.radius = 5.f;
		op.shape.sdf_scale = 1.f;
		op.box = op.shape.get_box();
		op.mode = ops::MODE_ADD;
		// op.texture_params = _texture_params
		// op.blocky_value = _value;
		op.channel = VoxelBuffer::CHANNEL_SDF;
		op.strength = 1.f;

		ZN_ASSERT(voxel_data.is_area_loaded(op.box));

		voxel_data.pre_generate_box(op.box);
		voxel_data.get_blocks_grid(op.blocks, op.box, 0);
		op();
	}

	voxel_data.pre_generate_box(Box3i(undo_pos, buffer_before_edit.get_size()));
	voxel_data.paste(undo_pos, buffer_before_edit, 0xff, false);

	// Checks terrain is still as we expect. Not relying on copy() followed by equals(), because copy() is part of what
	// we are testing
	L::check_original(voxel_data, sdf_dequantize);
}

} // namespace zylann::voxel::tests