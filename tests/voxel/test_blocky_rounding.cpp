#include "test_blocky_rounding.h"
#include "../../meshers/blocky/types/voxel_blocky_type_library.h"
#include "../../meshers/blocky/voxel_blocky_library.h"
#include "../../meshers/blocky/voxel_blocky_model_cube.h"
#include "../../meshers/blocky/voxel_blocky_model_empty.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/time.h"
#include "../../util/io/log.h"
#include "../../util/string/format.h"
#include "../../util/testing/test_macros.h"
#include "../../util/thread/thread.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace zylann::voxel::tests {
namespace {
class BlockyRoundingTestLibrary : public VoxelBlockyLibrary {
public:
	void clear_rounding_mappings_for_test() {
		RWLockWrite lock(_baked_data_rw_lock);
		_baked_data.rounding_vertex_mappings.clear();
	}
};
using TriangleKey = std::array<int64_t, 9>;
using TriangleKeys = StdVector<TriangleKey>;
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

namespace {
uint64_t rounding_mesh_hash(const VoxelMesher::Output &output) {
	uint64_t h = 1469598103934665603ULL;
	for (const TriangleKey &t : triangle_keys(output, Vector3()))
		for (int64_t x : t) {
			h ^= uint64_t(x);
			h *= 1099511628211ULL;
		}
	// Check all emitted attributes during concurrent rebakes, not only positions.
	for (const auto &surface : output.surfaces) {
		for (int i = 0; i < surface.arrays.size(); ++i) {
			h ^= uint64_t(Variant(surface.arrays[i]).hash());
			h *= 1099511628211ULL;
		}
	}

	return h;
}
struct RoundingReaderContext {
	VoxelMesherBlocky *mesher;
	const VoxelBuffer *voxels;
	const std::array<uint64_t, 2> *expected;
	std::atomic<bool> *stop;
	std::atomic<unsigned int> *errors;
	std::array<std::atomic<unsigned int>, 2> *observed;
};
void rounding_reader(void *data) {
	auto &ctx = *static_cast<RoundingReaderContext *>(data);
	while (!ctx.stop->load()) {
		VoxelMesher::Output output;
		ctx.mesher->build(output, { *ctx.voxels, nullptr, Vector3i(), 0, true });
		const uint64_t h = rounding_mesh_hash(output);
		if (h == (*ctx.expected)[0])
			++(*ctx.observed)[0];
		else if (h == (*ctx.expected)[1])
			++(*ctx.observed)[1];
		else
			++(*ctx.errors);
		Thread::sleep_usec(100);
	}
}
bool wait_for_reads(const std::atomic<unsigned int> &counter, unsigned int minimum) {
	const uint64_t start = Time::get_singleton()->get_ticks_usec();
	while (counter < minimum) {
		if (Time::get_singleton()->get_ticks_usec() - start > 10000000)
			return false;
		Thread::sleep_usec(100);
	}
	return true;
}
} // namespace

void test_blocky_rounding_table() {
	Ref<VoxelBlockyLibrary> library;
	library.instantiate();
	Ref<VoxelBlockyModelEmpty> air;
	air.instantiate();
	library->add_model(air);
	Ref<VoxelBlockyModelCube> cube;
	cube.instantiate();
	library->add_model(cube);
	library->bake();
	ZN_TEST_ASSERT(library->get_baked_data().rounding_geometry.get_entries().empty());
	cube->set_rounding_enabled(true);
	uint64_t started = Time::get_singleton()->get_ticks_usec();
	library->bake();
	print_line(
			format("ROUND_TABLE_BAKE default_us={} bytes={} entries={}",
				   Time::get_singleton()->get_ticks_usec() - started,
				   library->get_baked_data().rounding_geometry.get_storage_bytes(),
				   library->get_baked_data().rounding_geometry.get_entries().size())
	);
	const auto *original = library->get_baked_data().rounding_geometry.get(4095);
	ZN_TEST_ASSERT(original != nullptr);
	started = Time::get_singleton()->get_ticks_usec();
	library->bake();
	print_line(format("ROUND_TABLE_REUSE us={}", Time::get_singleton()->get_ticks_usec() - started));
	ZN_TEST_ASSERT(library->get_baked_data().rounding_geometry.get(4095) == original);
	ZN_TEST_ASSERT(library->get_baked_data().rounding_geometry.get_entries().size() == 857);
	library->set_rounding_amount(.0005f);
	library->bake();
	ZN_TEST_ASSERT(library->get_baked_data().rounding_geometry.get_storage_bytes() == 0);
	library->set_rounding_amount(.15f);
	library->set_rounding_taper_distance(1.f);
	library->bake();
	const auto *rebaked = library->get_baked_data().rounding_geometry.get(4095);
	ZN_TEST_ASSERT(rebaked != nullptr && !rebaked->triangles.empty());
	cube->set_rounding_enabled(false);
	library->bake();
	ZN_TEST_ASSERT(library->get_baked_data().rounding_geometry.get_storage_bytes() == 0);
	cube->set_rounding_enabled(true);
	library->bake();

	// Type-library models use the same bake path, including turning rounding off.
	Ref<VoxelBlockyTypeLibrary> types;
	types.instantiate();
	types->load_default();
	types->get_type_from_name("air")->set_base_model(air);
	Ref<VoxelBlockyType> type = types->get_type_from_name("cube");
	ZN_TEST_ASSERT(type.is_valid());
	type->get_base_model()->set_rounding_enabled(true);
	types->bake();
	ZN_TEST_ASSERT(types->get_baked_data().rounding_geometry.get_entries().size() == 857);
	type->get_base_model()->set_rounding_enabled(false);
	types->bake();
	ZN_TEST_ASSERT(types->get_baked_data().rounding_geometry.get_storage_bytes() == 0);

	// A bake cannot replace storage while an existing reader holds its lifetime.
	struct BakeContext {
		VoxelBlockyLibrary *library;
		std::atomic<bool> entered{ false }, done{ false };
	} bake_context;
	bake_context.library = library.ptr();
	Thread baker;
	library->set_rounding_taper_profile(VoxelBlockyLibraryBase::ROUNDING_TAPER_SMOOTHSTEP);
	{
		RWLockRead lock(library->get_baked_data_rw_lock());
		const auto *saved = library->get_baked_data().rounding_geometry.get(4095);
		baker.start(
				[](void *p) {
					auto &ctx = *static_cast<BakeContext *>(p);
					ctx.entered = true;
					ctx.library->bake();
					ctx.done = true;
				},
				&bake_context
		);
		while (!bake_context.entered)
			Thread::sleep_usec(100);
		Thread::sleep_usec(1000);
		ZN_TEST_ASSERT(!bake_context.done);
		ZN_TEST_ASSERT(library->get_baked_data().rounding_geometry.get(4095) == saved);
	}
	baker.wait_to_finish();
	ZN_TEST_ASSERT(bake_context.done);

	Ref<VoxelMesherBlocky> mesher;
	mesher.instantiate();
	mesher->set_library(library);
	VoxelBuffer voxels(VoxelBuffer::ALLOCATOR_DEFAULT);
	voxels.create(4, 3, 3);
	voxels.set_voxel(1, Vector3i(1, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	voxels.set_voxel(1, Vector3i(2, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	std::array<uint64_t, 2> expected;
	for (int i = 0; i < 2; ++i) {
		library->set_rounding_amount(i == 0 ? .1f : .15f);
		library->bake();
		VoxelMesher::Output output;
		mesher->build(output, { voxels, nullptr, Vector3i(), 0, true });
		expected[i] = rounding_mesh_hash(output);
	}
	ZN_TEST_ASSERT(expected[0] != expected[1]);
	library->set_rounding_amount(.1f);
	library->bake();
	std::atomic<bool> stop{ false };
	std::atomic<unsigned int> errors{ 0 };
	std::array<std::atomic<unsigned int>, 2> observed{};
	RoundingReaderContext ctx{ mesher.ptr(), &voxels, &expected, &stop, &errors, &observed };
	std::array<Thread, 4> readers;
	for (Thread &thread : readers)
		thread.start(rounding_reader, &ctx);
	bool completed = wait_for_reads(observed[0], 20);
	for (int i = 0; i < 4 && completed; ++i) {
		const unsigned int which = (i + 1) % 2, minimum = observed[which] + 20;
		library->set_rounding_amount(which == 0 ? .1f : .15f);
		library->bake();
		completed = wait_for_reads(observed[which], minimum);
	}
	stop = true;
	for (Thread &thread : readers)
		thread.wait_to_finish();
	ZN_TEST_ASSERT(completed && errors == 0 && observed[0] >= 20 && observed[1] >= 20);
	print_line(
			format("ROUND_TABLE_CONCURRENCY readers=4 rebakes=4 old_reads={} new_reads={} errors={}",
				   observed[0].load(),
				   observed[1].load(),
				   errors.load())
	);
}

void test_blocky_rounding_mapping() {
	Ref<BlockyRoundingTestLibrary> library;
	library.instantiate();
	Ref<VoxelBlockyModelEmpty> air;
	air.instantiate();
	library->add_model(air);
	Ref<VoxelBlockyModelCube> a, b;
	a.instantiate();
	b.instantiate();
	a->set_rounding_enabled(true);
	b->set_rounding_enabled(true);
	a->set_atlas_size_in_tiles(Vector2i(4, 4));
	b->set_atlas_size_in_tiles(Vector2i(4, 4));
	b->set_color(Color(0.5, 0.75, 1.0));
	library->add_model(a);
	library->add_model(b);
	library->set_rounding_amount(.15f);
	library->set_rounding_taper_distance(1.f);
	library->bake();
	const auto &data = library->get_baked_data();
	ZN_TEST_ASSERT(data.rounding_vertex_mappings.size() == 1);
	ZN_TEST_ASSERT(data.models[1].rounding_mapping_index == data.models[2].rounding_mapping_index);
	ZN_TEST_ASSERT(data.rounding_vertex_mappings[0].size() == data.rounding_geometry.get_vertices().size());
	const auto *saved_geometry = data.rounding_geometry.get(4095);
	const auto *saved_vertices = data.rounding_geometry.get_vertices().data();
	b->set_tile(VoxelBlockyModel::SIDE_POSITIVE_Y, Vector2i(1, 1));
	library->bake();
	ZN_TEST_ASSERT(data.rounding_vertex_mappings.size() == 2);
	ZN_TEST_ASSERT(data.models[1].rounding_mapping_index != data.models[2].rounding_mapping_index);
	ZN_TEST_ASSERT(data.rounding_geometry.get(4095) == saved_geometry);
	ZN_TEST_ASSERT(data.rounding_geometry.get_vertices().data() == saved_vertices);

	Ref<VoxelMesherBlocky> mesher;
	mesher.instantiate();
	mesher->set_library(library);
	VoxelBuffer voxels(VoxelBuffer::ALLOCATOR_DEFAULT);
	voxels.create(4, 3, 3);
	voxels.set_voxel(1, Vector3i(1, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	voxels.set_voxel(2, Vector3i(2, 1, 1), VoxelBuffer::CHANNEL_TYPE);
	for (bool bake_tangents : { true, false }) {
		library->set_bake_tangents(bake_tangents);
		library->bake();
		VoxelMesher::Output cached, fallback;
		mesher->build(cached, { voxels, nullptr, Vector3i(), 0, true });
		// Fault injection exercises the slow path under an exclusive lock.
		library->clear_rounding_mappings_for_test();
		mesher->build(fallback, { voxels, nullptr, Vector3i(), 0, true });
		ZN_TEST_ASSERT(cached.surfaces.size() == fallback.surfaces.size());
		for (size_t i = 0; i < cached.surfaces.size(); ++i) {
			const auto &ca = cached.surfaces[i].arrays;
			const auto &fa = fallback.surfaces[i].arrays;
			ZN_TEST_ASSERT(ca.size() == fa.size());
			for (int j = 0; j < ca.size(); ++j)
				ZN_TEST_ASSERT(Variant(ca[j]) == Variant(fa[j]));
		}
	}
	library->bake();
	size_t mapping_bytes = data.rounding_vertex_mappings.capacity() * sizeof(StdVector<blocky::RoundedVertexMapping>);
	for (const auto &mapping : data.rounding_vertex_mappings)
		mapping_bytes += mapping.capacity() * sizeof(blocky::RoundedVertexMapping);
	print_line(format(
			"ROUND_SHARED geometry_bytes={} pooled_vertices={} mapping_arrays={} mapping_bytes={} model_bytes={}",
			data.rounding_geometry.get_storage_bytes(),
			data.rounding_geometry.get_vertices().size(),
			data.rounding_vertex_mappings.size(),
			mapping_bytes,
			sizeof(blocky::BakedModel)
	));
	a->set_rounding_enabled(false);
	b->set_rounding_enabled(false);
	library->bake();
	ZN_TEST_ASSERT(data.rounding_geometry.get_storage_bytes() == 0);
	ZN_TEST_ASSERT(data.rounding_vertex_mappings.empty() && data.rounding_vertex_mappings.capacity() == 0);
	ZN_TEST_ASSERT(data.models[1].rounding_mapping_index == UINT32_MAX);
	ZN_TEST_ASSERT(data.models[2].rounding_mapping_index == UINT32_MAX);
}

} // namespace zylann::voxel::tests
