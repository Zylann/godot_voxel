#include "test_blocky_rounding.h"
#include "../../meshers/blocky/voxel_blocky_library.h"
#include "../../meshers/blocky/voxel_blocky_model_cube.h"
#include "../../meshers/blocky/voxel_blocky_model_empty.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/testing/test_macros.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace zylann::voxel::tests {
namespace {
using TriangleKey = std::array<int64_t, 9>;
using TriangleKeys = std::vector<TriangleKey>;
TriangleKeys triangle_keys(const VoxelMesher::Output &output, Vector3 offset) {
	TriangleKeys keys;
	for (const VoxelMesher::Output::Surface &surface : output.surfaces) {
		const PackedVector3Array vertices = surface.arrays[Mesh::ARRAY_VERTEX];
		const PackedInt32Array indices = surface.arrays[Mesh::ARRAY_INDEX];
		for (int i = 0; i < indices.size(); i += 3) {
			std::array<std::array<int64_t, 3>, 3> points;
			for (int j = 0; j < 3; ++j) {
				const Vector3 p = vertices[indices[i + j]] + offset;
				points[j] = { std::llround(p.x * 1e6), std::llround(p.y * 1e6), std::llround(p.z * 1e6) };
			}
			std::sort(points.begin(), points.end());
			TriangleKey key;
			for (int j = 0; j < 3; ++j) {
				for (int k = 0; k < 3; ++k) {
					key[3 * j + k] = points[j][k];
				}
			}
			keys.push_back(key);
		}
	}
	std::sort(keys.begin(), keys.end());
	return keys;
}
} // namespace

void test_blocky_rounding() {
	Ref<VoxelBlockyLibrary> library;
	library.instantiate();
	Ref<VoxelBlockyModelEmpty> air;
	air.instantiate();
	library->add_model(air);
	Ref<VoxelBlockyModelCube> cube;
	cube.instantiate();
	cube->set_mesh_collision_enabled(0, true);
	library->add_model(cube);
	Ref<VoxelBlockyModelCube> square;
	square.instantiate();
	square->set_mesh_collision_enabled(0, true);
	library->add_model(square);
	library->bake();
	Ref<VoxelMesherBlocky> mesher;
	mesher.instantiate();
	mesher->set_library(library);
	VoxelBuffer voxels(VoxelBuffer::ALLOCATOR_DEFAULT);
	voxels.create(4, 3, 3);
	voxels.set_voxel(1, Vector3i(1, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	voxels.set_voxel(2, Vector3i(2, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	VoxelMesher::Input input{ voxels, nullptr, Vector3i(), 0, true };
	VoxelMesher::Output plain;
	mesher->build(plain, input);
	ZN_TEST_ASSERT(triangle_keys(plain, Vector3()).size() == 20);
	cube->set_rounding_enabled(true);
	library->bake();
	VoxelMesher::Output rounded;
	mesher->build(rounded, input);
	ZN_TEST_ASSERT(triangle_keys(rounded, Vector3()).size() > 20);
	ZN_TEST_ASSERT(rounded.surfaces.size() == 1);
	const PackedVector3Array vertices = rounded.surfaces[0].arrays[Mesh::ARRAY_VERTEX];
	const PackedInt32Array indices = rounded.surfaces[0].arrays[Mesh::ARRAY_INDEX];
	ZN_TEST_ASSERT(size_t(vertices.size()) == rounded.collision_surface.positions.size());
	ZN_TEST_ASSERT(size_t(indices.size()) == rounded.collision_surface.indices.size());
	for (int i = 0; i < vertices.size(); ++i) {
		const Vector3f p = rounded.collision_surface.positions[i];
		ZN_TEST_ASSERT(vertices[i].is_equal_approx(Vector3(p.x, p.y, p.z)));
	}
	for (int i = 0; i < indices.size(); ++i) {
		ZN_TEST_ASSERT(indices[i] == rounded.collision_surface.indices[i]);
	}
	ZN_TEST_ASSERT(voxels.get_voxel(Vector3i(1, 1, 1), VoxelBuffer::CHANNEL_TYPE) == 1);
	ZN_TEST_ASSERT(voxels.get_voxel(Vector3i(2, 1, 1), VoxelBuffer::CHANNEL_TYPE) == 2);
	// The same pair split across two mesh chunks must emit identical triangles.
	VoxelBuffer left(VoxelBuffer::ALLOCATOR_DEFAULT), right(VoxelBuffer::ALLOCATOR_DEFAULT);
	left.create(3, 3, 3);
	right.create(3, 3, 3);
	left.set_voxel(1, Vector3i(1, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	left.set_voxel(2, Vector3i(2, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	right.set_voxel(1, Vector3i(0, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	right.set_voxel(2, Vector3i(1, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	VoxelMesher::Output left_mesh, right_mesh;
	mesher->build(left_mesh, { left, nullptr, Vector3i(), 0, true });
	mesher->build(right_mesh, { right, nullptr, Vector3i(1, 0, 0), 0, true });
	TriangleKeys joined = triangle_keys(left_mesh, Vector3());
	const TriangleKeys right_keys = triangle_keys(right_mesh, Vector3(1, 0, 0));
	joined.insert(joined.end(), right_keys.begin(), right_keys.end());
	std::sort(joined.begin(), joined.end());
	ZN_TEST_ASSERT(joined == triangle_keys(rounded, Vector3()));
	library->set_rounding_amount(0);
	library->bake();
	VoxelMesher::Output zero;
	mesher->build(zero, input);
	ZN_TEST_ASSERT(triangle_keys(zero, Vector3()) == triangle_keys(plain, Vector3()));
}
} // namespace zylann::voxel::tests
