#include "test_edition_funcs.h"
#include "../edition/funcs.h"
#include "../edition/voxel_tool_terrain.h"
#include "../meshers/blocky/voxel_blocky_library.h"
#include "../meshers/blocky/voxel_blocky_model_cube.h"
#include "../meshers/blocky/voxel_blocky_model_mesh.h"
#include "../storage/voxel_data.h"
#include "test_util.h"
#include "testing.h"

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
		VoxelBufferInternal model_buffer;
		model_buffer.create(Vector3iUtil::create(data.get_block_size()));
		for (int z = 0; z < model_buffer.get_size().z; ++z) {
			for (int x = 0; x < model_buffer.get_size().x; ++x) {
				for (int y = 0; y < model_buffer.get_size().y; ++y) {
					const int block_id = (x + y + z) % 3;
					model_buffer.set_voxel(block_id, x, y, z, VoxelBufferInternal::CHANNEL_TYPE);
				}
			}
		}

		const Box3i world_blocks_box(-4, -4, -4, 8, 8, 8);
		world_blocks_box.for_each_cell_zxy([&data, &model_buffer](Vector3i block_pos) {
			std::shared_ptr<VoxelBufferInternal> buffer = make_shared_instance<VoxelBufferInternal>();
			buffer->create(model_buffer.get_size());
			buffer->copy_from(model_buffer);
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
	VoxelBufferInternal voxels;
	voxels.create(64, 64, 64);

	Vector3i pos;
	for (pos.z = 0; pos.z < voxels.get_size().z; ++pos.z) {
		for (pos.x = 0; pos.x < voxels.get_size().x; ++pos.x) {
			for (pos.y = 0; pos.y < voxels.get_size().y; ++pos.y) {
				const float sd = Math::cos(0.53f * pos.x) + Math::sin(0.37f * pos.y) + Math::sin(0.71f * pos.z);
				voxels.set_voxel_f(sd, pos, VoxelBufferInternal::CHANNEL_SDF);
			}
		}
	}

	const int blur_radius = 3;
	const Vector3f sphere_pos = to_vec3f(voxels.get_size()) / 2.f;
	const float sphere_radius = 64 - blur_radius;

	struct L {
		static void save_image(const VoxelBufferInternal &vb, int y, const char *name) {
			Ref<Image> im = gd::VoxelBuffer::debug_print_sdf_z_slice(vb, 1.f, y);
			ZN_ASSERT(im.is_valid());
			im->resize(im->get_width() * 4, im->get_height() * 4, Image::INTERPOLATE_NEAREST);
			im->save_png(name);
		}
	};

	// L::save_image(voxels, 32, "test_box_blur_src.png");

	VoxelBufferInternal voxels_blurred_1;
	ops::box_blur_slow_ref(voxels, voxels_blurred_1, blur_radius, sphere_pos, sphere_radius);
	// L::save_image(voxels_blurred_1, 32 - blur_radius, "test_box_blur_blurred_1.png");

	VoxelBufferInternal voxels_blurred_2;
	ops::box_blur(voxels, voxels_blurred_2, blur_radius, sphere_pos, sphere_radius);
	// L::save_image(voxels_blurred_2, 32 - blur_radius, "test_box_blur_blurred_2.png");

	ZN_TEST_ASSERT(sd_equals_approx(voxels_blurred_1, voxels_blurred_2));
}

} // namespace zylann::voxel::tests