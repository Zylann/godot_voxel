#include "test_voxel_mesher_cubes.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../testing.h"

namespace zylann::voxel::tests {

void test_voxel_mesher_cubes() {
	VoxelBuffer vb(VoxelBuffer::ALLOCATOR_DEFAULT);
	vb.create(8, 8, 8);
	vb.set_channel_depth(VoxelBuffer::CHANNEL_COLOR, VoxelBuffer::DEPTH_16_BIT);
	vb.set_voxel(Color8(0, 255, 0, 255).to_u16(), Vector3i(3, 4, 4), VoxelBuffer::CHANNEL_COLOR);
	vb.set_voxel(Color8(0, 255, 0, 255).to_u16(), Vector3i(4, 4, 4), VoxelBuffer::CHANNEL_COLOR);
	vb.set_voxel(Color8(0, 0, 255, 128).to_u16(), Vector3i(5, 4, 4), VoxelBuffer::CHANNEL_COLOR);

	Ref<VoxelMesherCubes> mesher;
	mesher.instantiate();
	mesher->set_color_mode(VoxelMesherCubes::COLOR_RAW);

	VoxelMesher::Input input{ vb, nullptr, nullptr, Vector3i(), 0, false };
	VoxelMesher::Output output;
	mesher->build(output, input);

	const unsigned int opaque_surface_index = VoxelMesherCubes::MATERIAL_OPAQUE;
	const unsigned int transparent_surface_index = VoxelMesherCubes::MATERIAL_TRANSPARENT;

	ZN_TEST_ASSERT(output.surfaces.size() == 2);
	ZN_TEST_ASSERT(output.surfaces[0].arrays.size() > 0);
	ZN_TEST_ASSERT(output.surfaces[1].arrays.size() > 0);

	const PackedVector3Array surface0_vertices = output.surfaces[opaque_surface_index].arrays[Mesh::ARRAY_VERTEX];
	const unsigned int surface0_vertices_count = surface0_vertices.size();

	const PackedVector3Array surface1_vertices = output.surfaces[transparent_surface_index].arrays[Mesh::ARRAY_VERTEX];
	const unsigned int surface1_vertices_count = surface1_vertices.size();

	// println("Surface0:");
	// for (int i = 0; i < surface0_vertices.size(); ++i) {
	// 	println(format("v[{}]: {}", i, surface0_vertices[i]));
	// }
	// println("Surface1:");
	// for (int i = 0; i < surface1_vertices.size(); ++i) {
	// 	println(format("v[{}]: {}", i, surface1_vertices[i]));
	// }

	// Greedy meshing with two cubes of the same color next to each other means it will be a single box.
	// Each side has different normals, so vertices have to be repeated. 6 sides * 4 vertices = 24.
	ZN_TEST_ASSERT(surface0_vertices_count == 24);
	// The transparent cube has less vertices because one of its faces overlaps with a neighbor solid face,
	// so it is culled
	ZN_TEST_ASSERT(surface1_vertices_count == 20);
}

} // namespace zylann::voxel::tests
