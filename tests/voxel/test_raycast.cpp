#include "test_raycast.h"
#include "../../edition/raycast.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../meshers/blocky/voxel_blocky_library.h"
#include "../../meshers/blocky/voxel_blocky_model_cube.h"
#include "../../meshers/blocky/voxel_blocky_model_empty.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../storage/voxel_data.h"
#include "../../util/testing/test_macros.h"

namespace zylann::voxel::tests {

void test_raycast_sdf() {
	const unsigned int approx_steps = 5;

	// Very basic test with a flat plane
	{
		const float plane_height = 5.f;

		VoxelData data;
		data.set_bounds(Box3i::from_min_max(Vector3iUtil::create(-100), Vector3iUtil::create(100)));

		{
			std::shared_ptr<VoxelBuffer> vb_p = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_DEFAULT);
			VoxelBuffer &vb = *vb_p;

			vb.create(Vector3iUtil::create(1 << constants::DEFAULT_BLOCK_SIZE_PO2));
			for (int rz = 0; rz < vb.get_size().z; ++rz) {
				for (int rx = 0; rx < vb.get_size().x; ++rx) {
					for (int ry = 0; ry < vb.get_size().y; ++ry) {
						const float gy = ry;
						const float sd = gy - plane_height;
						vb.set_voxel_f(sd, Vector3i(rx, ry, rz), VoxelBuffer::CHANNEL_SDF);
					}
				}
			}

			VoxelDataBlock block(vb_p, 0);
			block.set_edited(true);

			data.try_set_block(Vector3i(0, 0, 0), block);
		}

		{
			// Raycast from integer coordinates
			const Vector3 ray_origin(5, plane_height + 2, 5);
			Ref<VoxelRaycastResult> hit = raycast_sdf(data, ray_origin, Vector3i(0, -1, 0), 10.0, approx_steps, true);
			ZN_TEST_ASSERT(hit.is_valid());
			ZN_TEST_ASSERT(Math::is_equal_approx(hit->distance_along_ray, 2.f));
			ZN_TEST_ASSERT(hit->normal.is_equal_approx(Vector3(0, 1, 0)));
		}
		{
			// Raycast from decimal coordinates
			const Vector3 ray_origin(5.2, plane_height + 2.2, 5.2);
			Ref<VoxelRaycastResult> hit = raycast_sdf(data, ray_origin, Vector3i(0, -1, 0), 10.0, approx_steps, true);
			ZN_TEST_ASSERT(hit.is_valid());
			ZN_TEST_ASSERT(Math::is_equal_approx(hit->distance_along_ray, 2.2f));
			ZN_TEST_ASSERT(hit->normal.is_equal_approx(Vector3(0, 1, 0)));
		}
	}

	// Very basic test with a sphere
	{
		const int sphere_radius = 5;

		VoxelData data;
		data.set_bounds(Box3i::from_min_max(Vector3iUtil::create(-100), Vector3iUtil::create(100)));

		{
			std::shared_ptr<VoxelBuffer> vb_p = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_DEFAULT);
			VoxelBuffer &vb = *vb_p;

			vb.create(Vector3iUtil::create(1 << constants::DEFAULT_BLOCK_SIZE_PO2));
			for (int rz = 0; rz < vb.get_size().z; ++rz) {
				for (int rx = 0; rx < vb.get_size().x; ++rx) {
					for (int ry = 0; ry < vb.get_size().y; ++ry) {
						const float sd = math::length(Vector3f(rx, ry, rz)) - static_cast<float>(sphere_radius);
						vb.set_voxel_f(sd, Vector3i(rx, ry, rz), VoxelBuffer::CHANNEL_SDF);
					}
				}
			}

			VoxelDataBlock block(vb_p, 0);
			block.set_edited(true);

			data.try_set_block(Vector3i(0, 0, 0), block);
		}

		{
			// Raycast in diagonal across all axes

			const Vector3 ray_origin(sphere_radius, sphere_radius, sphere_radius);
			const Vector3 ray_dir = Vector3(-1, -1, -1).normalized();
			Ref<VoxelRaycastResult> hit = raycast_sdf(data, ray_origin, ray_dir, 10.0, approx_steps, true);
			ZN_TEST_ASSERT(hit.is_valid());

			const float expected_distance = ray_origin.distance_to(Vector3(1, 1, 1).normalized() * sphere_radius);
			const float found_distance = hit->distance_along_ray;
			ZN_TEST_ASSERT(math::abs(expected_distance - found_distance) < 0.05f);

			const Vector3 expected_normal = Vector3(1, 1, 1).normalized();
			ZN_TEST_ASSERT(hit->normal.is_equal_approx(expected_normal));
		}
	}
}

void test_raycast_blocky() {
	VoxelData data;
	data.set_bounds(Box3i::from_min_max(Vector3iUtil::create(-100), Vector3iUtil::create(100)));

	const float slab_height = 0.3f;

	// const int air_id = 0;
	const int cube_id = 1;
	const int slab_id = 2;

	const int floor_height = 2;

	const uint32_t collision_mask = 0xffffffff;

	const Vector3i slab_position(5, floor_height, 5);

	Ref<VoxelMesherBlocky> mesher;
	{
		Ref<VoxelBlockyLibrary> library;
		library.instantiate();

		{
			Ref<VoxelBlockyModelEmpty> air;
			air.instantiate();
			library->add_model(air);
		}
		{
			Ref<VoxelBlockyModelCube> cube;
			cube.instantiate();
			library->add_model(cube);
		}
		{
			Ref<VoxelBlockyModelCube> slab;
			slab.instantiate();
			slab->set_height(slab_height);
			library->add_model(slab);
		}

		library->bake();

		mesher.instantiate();
		mesher->set_library(library);
	}

	{
		std::shared_ptr<VoxelBuffer> vb_p = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_DEFAULT);
		VoxelBuffer &vb = *vb_p;

		vb.create(Vector3iUtil::create(1 << constants::DEFAULT_BLOCK_SIZE_PO2));
		vb.fill_area(
				cube_id,
				Vector3i(0, 0, 0),
				Vector3i(vb.get_size().x, floor_height, vb.get_size().z),
				VoxelBuffer::CHANNEL_TYPE
		);
		vb.set_voxel(slab_id, slab_position, VoxelBuffer::CHANNEL_TYPE);

		VoxelDataBlock block(vb_p, 0);
		block.set_edited(true);

		data.try_set_block(Vector3i(0, 0, 0), block);
	}

	{
		// Raycast the ground away from the slab

		const Vector3 ray_origin(slab_position.x + 1.5, slab_position.y + 2, slab_position.z);
		const Vector3 ray_dir(0, -1, 0);

		Ref<VoxelRaycastResult> hit = raycast_blocky(data, **mesher, ray_origin, ray_dir, 10, collision_mask);

		ZN_TEST_ASSERT(hit.is_valid());

		const Vector3i expected_hit_position(math::floor(ray_origin.x), floor_height - 1, math::floor(ray_origin.z));
		ZN_TEST_ASSERT(hit->position == expected_hit_position);

		ZN_TEST_ASSERT(hit->previous_position == expected_hit_position + Vector3i(0, 1, 0));

		ZN_TEST_ASSERT(hit->normal == Vector3i(0, 1, 0));
	}

	{
		// Raycast the slab

		const Vector3 ray_origin(slab_position.x, slab_position.y + 2, slab_position.z);
		const Vector3 ray_dir(0, -1, 0);

		Ref<VoxelRaycastResult> hit = raycast_blocky(data, **mesher, ray_origin, ray_dir, 10, collision_mask);

		ZN_TEST_ASSERT(hit.is_valid());

		ZN_TEST_ASSERT(hit->position == slab_position);

		ZN_TEST_ASSERT(hit->previous_position == slab_position + Vector3i(0, 1, 0));

		ZN_TEST_ASSERT(hit->normal == Vector3i(0, 1, 0));

		const float expected_distance = ray_origin.y - (static_cast<float>(slab_position.y) + slab_height);
		ZN_TEST_ASSERT(Math::is_equal_approx(hit->distance_along_ray, expected_distance));
	}
}

void test_raycast_blocky_no_cache_graph() {
	using namespace pg;

	Ref<VoxelGeneratorGraph> graph;
	graph.instantiate();

	const int floor_height = 5;

	const int air_id = 0;
	const int cube_id = 1;

	// Make flat world with ground below Y=5
	{
		Ref<VoxelGraphFunction> main = graph->get_main_function();

		const uint32_t n_in_y = main->create_node(VoxelGraphFunction::NODE_INPUT_Y);
		const uint32_t n_select = main->create_node(VoxelGraphFunction::NODE_SELECT);
		const uint32_t n_out_type = main->create_node(VoxelGraphFunction::NODE_OUTPUT_TYPE);

		const int threshold_param_index = 0;
		main->set_node_param(n_select, threshold_param_index, floor_height);
		main->set_node_default_input(n_select, 0, cube_id);
		main->set_node_default_input(n_select, 1, air_id);

		main->add_connection(n_in_y, 0, n_select, 2);
		main->add_connection(n_select, 0, n_out_type, 0);

		const CompilationResult result = graph->compile(false);
		ZN_TEST_ASSERT(result.success);
	}

	VoxelData data;
	data.set_bounds(Box3i::from_min_max(Vector3iUtil::create(-100), Vector3iUtil::create(100)));
	data.set_generator(graph);
	data.set_streaming_enabled(false);
	data.set_full_load_completed(true);

	Ref<VoxelMesherBlocky> mesher;
	{
		Ref<VoxelBlockyLibrary> library;
		library.instantiate();

		{
			Ref<VoxelBlockyModelEmpty> air;
			air.instantiate();
			library->add_model(air);
		}
		{
			Ref<VoxelBlockyModelCube> cube;
			cube.instantiate();
			library->add_model(cube);
		}

		library->bake();

		mesher.instantiate();
		mesher->set_library(library);
	}

	const Vector3 ray_origin(10.0, 20.0, 15.0);
	const Vector3 ray_dir(0, -1, 0);

	const uint32_t collision_mask = 0xffffffff;

	const int v1 = graph->generate_single(Vector3i(0, floor_height, 0), VoxelBuffer::CHANNEL_TYPE).i;
	const int v0 = graph->generate_single(Vector3i(0, floor_height - 1, 0), VoxelBuffer::CHANNEL_TYPE).i;

	ZN_TEST_ASSERT(v1 == air_id);
	ZN_TEST_ASSERT(v0 == cube_id);

	Ref<VoxelRaycastResult> hit = raycast_blocky(data, **mesher, ray_origin, ray_dir, 20, collision_mask);

	ZN_TEST_ASSERT(hit.is_valid());
	ZN_TEST_ASSERT(hit->position == Vector3i(10, floor_height - 1, 15));
}

} // namespace zylann::voxel::tests
