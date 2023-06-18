#include "tests.h"
#include "../edition/funcs.h"
#include "../edition/voxel_mesh_sdf_gd.h"
#include "../edition/voxel_tool_terrain.h"
#include "../generators/graph/range_utility.h"
#include "../meshers/blocky/voxel_blocky_library.h"
#include "../meshers/blocky/voxel_blocky_model_cube.h"
#include "../meshers/blocky/voxel_blocky_model_mesh.h"
#include "../meshers/cubes/voxel_mesher_cubes.h"
#include "../storage/voxel_buffer_gd.h"
#include "../storage/voxel_data.h"
#include "../storage/voxel_data_map.h"
#include "../streams/instance_data.h"
#include "../streams/region/region_file.h"
#include "../streams/region/voxel_stream_region_files.h"
#include "../streams/voxel_block_serializer.h"
#include "../streams/voxel_block_serializer_gd.h"
#include "../util/container_funcs.h"
#include "../util/flat_map.h"
#include "../util/godot/classes/box_shape_3d.h"
#include "../util/godot/classes/time.h"
#include "../util/godot/funcs.h"
#include "../util/island_finder.h"
#include "../util/math/box3i.h"
#include "../util/profiling.h"
#include "../util/slot_map.h"
#include "../util/string_funcs.h"
#include "test_block_serializer.h"
#include "test_box3i.h"
#include "test_detail_rendering_gpu.h"
#include "test_expression_parser.h"
#include "test_octree.h"
#include "test_region_file.h"
#include "test_spatial_lock.h"
#include "test_threaded_task_runner.h"
#include "test_util.h"
#include "test_voxel_buffer.h"
#include "test_voxel_data_map.h"
#include "test_voxel_graph.h"
#include "testing.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "../util/noise/fast_noise_2.h"
#endif

#include <core/io/dir_access.h>
#include <core/io/stream_peer.h>
#include <core/string/print_string.h>
#include <core/templates/hash_map.h>

namespace zylann::voxel::tests {

void test_encode_weights_packed_u16() {
	FixedArray<uint8_t, 4> weights;
	// There is data loss of the 4 smaller bits in this encoding,
	// so to test this we may use values greater than 16.
	// There is a compromise in decoding, where we choose that only values multiple of 16 are bijective.
	weights[0] = 1 << 4;
	weights[1] = 5 << 4;
	weights[2] = 10 << 4;
	weights[3] = 15 << 4;
	const uint16_t encoded_weights = encode_weights_to_packed_u16_lossy(weights[0], weights[1], weights[2], weights[3]);
	FixedArray<uint8_t, 4> decoded_weights = decode_weights_from_packed_u16(encoded_weights);
	ZN_TEST_ASSERT(weights == decoded_weights);
}

void test_copy_3d_region_zxy() {
	struct L {
		static void compare(Span<const uint16_t> srcs, Vector3i src_size, Vector3i src_min, Vector3i src_max,
				Span<const uint16_t> dsts, Vector3i dst_size, Vector3i dst_min) {
			Vector3i pos;
			for (pos.z = src_min.z; pos.z < src_max.z; ++pos.z) {
				for (pos.x = src_min.x; pos.x < src_max.x; ++pos.x) {
					for (pos.y = src_min.y; pos.y < src_max.y; ++pos.y) {
						const uint16_t srcv = srcs[Vector3iUtil::get_zxy_index(pos, src_size)];
						const uint16_t dstv = dsts[Vector3iUtil::get_zxy_index(pos - src_min + dst_min, dst_size)];
						ZN_TEST_ASSERT(srcv == dstv);
					}
				}
			}
		}
	};
	// Sub-region
	{
		std::vector<uint16_t> src;
		std::vector<uint16_t> dst;
		const Vector3i src_size(8, 8, 8);
		const Vector3i dst_size(3, 4, 5);
		src.resize(Vector3iUtil::get_volume(src_size), 0);
		dst.resize(Vector3iUtil::get_volume(dst_size), 0);
		for (unsigned int i = 0; i < src.size(); ++i) {
			src[i] = i;
		}

		Span<const uint16_t> srcs = to_span_const(src);
		Span<uint16_t> dsts = to_span(dst);
		const Vector3i dst_min(0, 0, 0);
		const Vector3i src_min(2, 1, 0);
		const Vector3i src_max(5, 4, 3);
		copy_3d_region_zxy(dsts, dst_size, dst_min, srcs, src_size, src_min, src_max);

		/*for (pos.y = src_min.y; pos.y < src_max.y; ++pos.y) {
		String s;
		for (pos.x = src_min.x; pos.x < src_max.x; ++pos.x) {
			const uint16_t v = srcs[pos.get_zxy_index(src_size)];
			if (v < 10) {
				s += String("{0}   ").format(varray(v));
			} else if (v < 100) {
				s += String("{0}  ").format(varray(v));
			} else {
				s += String("{0} ").format(varray(v));
			}
		}
		print_line(s);
	}
	print_line("----");
	const Vector3i dst_max = dst_min + (src_max - src_min);
	pos = Vector3i();
	for (pos.y = dst_min.y; pos.y < dst_max.y; ++pos.y) {
		String s;
		for (pos.x = dst_min.x; pos.x < dst_max.x; ++pos.x) {
			const uint16_t v = dsts[pos.get_zxy_index(dst_size)];
			if (v < 10) {
				s += String("{0}   ").format(varray(v));
			} else if (v < 100) {
				s += String("{0}  ").format(varray(v));
			} else {
				s += String("{0} ").format(varray(v));
			}
		}
		print_line(s);
	}*/

		L::compare(srcs, src_size, src_min, src_max, to_span_const(dsts), dst_size, dst_min);
	}
	// Same size, full region
	{
		std::vector<uint16_t> src;
		std::vector<uint16_t> dst;
		const Vector3i src_size(3, 4, 5);
		const Vector3i dst_size(3, 4, 5);
		src.resize(Vector3iUtil::get_volume(src_size), 0);
		dst.resize(Vector3iUtil::get_volume(dst_size), 0);
		for (unsigned int i = 0; i < src.size(); ++i) {
			src[i] = i;
		}

		Span<const uint16_t> srcs = to_span_const(src);
		Span<uint16_t> dsts = to_span(dst);
		const Vector3i dst_min(0, 0, 0);
		const Vector3i src_min(0, 0, 0);
		const Vector3i src_max = src_size;
		copy_3d_region_zxy(dsts, dst_size, dst_min, srcs, src_size, src_min, src_max);

		L::compare(srcs, src_size, src_min, src_max, to_span_const(dsts), dst_size, dst_min);
	}
}

void test_island_finder() {
	const char *cdata = "X X X - X "
						"X X X - - "
						"X X X - - "
						"X X X - - "
						"X X X - - "
						//
						"- - - - - "
						"X X - - - "
						"X X - - - "
						"X X X X X "
						"X X - - X "
						//
						"- - - - - "
						"- - - - - "
						"- - - - - "
						"- - - - - "
						"- - - - - "
						//
						"- - - - - "
						"- - - - - "
						"- - X - - "
						"- - X X - "
						"- - - - - "
						//
						"- - - - - "
						"- - - - - "
						"- - - - - "
						"- - - X - "
						"- - - - - "
			//
			;

	const Vector3i grid_size(5, 5, 5);
	ZN_TEST_ASSERT(Vector3iUtil::get_volume(grid_size) == strlen(cdata) / 2);

	std::vector<int> grid;
	grid.resize(Vector3iUtil::get_volume(grid_size));
	for (unsigned int i = 0; i < grid.size(); ++i) {
		const char c = cdata[i * 2];
		if (c == 'X') {
			grid[i] = 1;
		} else if (c == '-') {
			grid[i] = 0;
		} else {
			ERR_FAIL();
		}
	}

	std::vector<uint8_t> output;
	output.resize(Vector3iUtil::get_volume(grid_size));
	unsigned int label_count;

	IslandFinder island_finder;
	island_finder.scan_3d(
			Box3i(Vector3i(), grid_size),
			[&grid, grid_size](Vector3i pos) {
				const unsigned int i = Vector3iUtil::get_zxy_index(pos, grid_size);
				CRASH_COND(i >= grid.size());
				return grid[i] == 1;
			},
			to_span(output), &label_count);

	// unsigned int i = 0;
	// for (int z = 0; z < grid_size.z; ++z) {
	// 	for (int x = 0; x < grid_size.x; ++x) {
	// 		String s;
	// 		for (int y = 0; y < grid_size.y; ++y) {
	// 			s += String::num_int64(output[i++]);
	// 			s += " ";
	// 		}
	// 		print_line(s);
	// 	}
	// 	print_line("//");
	// }

	ZN_TEST_ASSERT(label_count == 3);
}

void test_unordered_remove_if() {
	struct L {
		static unsigned int count(const std::vector<int> &vec, int v) {
			unsigned int n = 0;
			for (size_t i = 0; i < vec.size(); ++i) {
				if (vec[i] == v) {
					++n;
				}
			}
			return n;
		}
	};
	// Remove one at beginning
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 0; });

		ZN_TEST_ASSERT(vec.size() == 3);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 0 && L::count(vec, 1) == 1 && L::count(vec, 2) == 1 && L::count(vec, 3) == 1);
	}
	// Remove one in middle
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 2; });

		ZN_TEST_ASSERT(vec.size() == 3);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 1 && L::count(vec, 2) == 0 && L::count(vec, 3) == 1);
	}
	// Remove one at end
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 3; });

		ZN_TEST_ASSERT(vec.size() == 3);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 1 && L::count(vec, 2) == 1 && L::count(vec, 3) == 0);
	}
	// Remove multiple
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 1 || v == 2; });

		ZN_TEST_ASSERT(vec.size() == 2);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 0 && L::count(vec, 2) == 0 && L::count(vec, 3) == 1);
	}
	// Remove last
	{
		std::vector<int> vec;
		vec.push_back(0);

		unordered_remove_if(vec, [](int v) { return v == 0; });

		ZN_TEST_ASSERT(vec.size() == 0);
	}
}

void test_instance_data_serialization() {
	struct L {
		static InstanceBlockData::InstanceData create_instance(
				float x, float y, float z, float rotx, float roty, float rotz, float scale) {
			InstanceBlockData::InstanceData d;
			d.transform = to_transform3f(Transform3D(
					Basis().rotated(Vector3(rotx, roty, rotz)).scaled(Vector3(scale, scale, scale)), Vector3(x, y, z)));
			return d;
		}
	};

	// Create some example data
	InstanceBlockData src_data;
	{
		src_data.position_range = 30;
		{
			InstanceBlockData::LayerData layer;
			layer.id = 1;
			layer.scale_min = 1.f;
			layer.scale_max = 1.f;
			layer.instances.push_back(L::create_instance(0, 0, 0, 0, 0, 0, 1));
			layer.instances.push_back(L::create_instance(10, 0, 0, 3.14, 0, 0, 1));
			layer.instances.push_back(L::create_instance(0, 20, 0, 0, 3.14, 0, 1));
			layer.instances.push_back(L::create_instance(0, 0, 30, 0, 0, 3.14, 1));
			src_data.layers.push_back(layer);
		}
		{
			InstanceBlockData::LayerData layer;
			layer.id = 2;
			layer.scale_min = 1.f;
			layer.scale_max = 4.f;
			layer.instances.push_back(L::create_instance(0, 1, 0, 0, 0, 0, 1));
			layer.instances.push_back(L::create_instance(20, 1, 0, -2.14, 0, 0, 2));
			layer.instances.push_back(L::create_instance(0, 20, 0, 0, -2.14, 0, 3));
			layer.instances.push_back(L::create_instance(0, 1, 20, -1, 0, 2.14, 4));
			src_data.layers.push_back(layer);
		}
	}

	std::vector<uint8_t> serialized_data;

	ZN_TEST_ASSERT(serialize_instance_block_data(src_data, serialized_data));

	InstanceBlockData dst_data;
	ZN_TEST_ASSERT(deserialize_instance_block_data(dst_data, to_span_const(serialized_data)));

	// Compare blocks
	ZN_TEST_ASSERT(src_data.layers.size() == dst_data.layers.size());
	ZN_TEST_ASSERT(dst_data.position_range >= 0.f);
	ZN_TEST_ASSERT(dst_data.position_range == src_data.position_range);

	const float distance_error = math::max(src_data.position_range, InstanceBlockData::POSITION_RANGE_MINIMUM) /
			float(InstanceBlockData::POSITION_RESOLUTION);

	// Compare layers
	for (unsigned int layer_index = 0; layer_index < dst_data.layers.size(); ++layer_index) {
		const InstanceBlockData::LayerData &src_layer = src_data.layers[layer_index];
		const InstanceBlockData::LayerData &dst_layer = dst_data.layers[layer_index];

		ZN_TEST_ASSERT(src_layer.id == dst_layer.id);
		if (src_layer.scale_max - src_layer.scale_min < InstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) {
			ZN_TEST_ASSERT(src_layer.scale_min == dst_layer.scale_min);
		} else {
			ZN_TEST_ASSERT(src_layer.scale_min == dst_layer.scale_min);
			ZN_TEST_ASSERT(src_layer.scale_max == dst_layer.scale_max);
		}
		ZN_TEST_ASSERT(src_layer.instances.size() == dst_layer.instances.size());

		const float scale_error = math::max(src_layer.scale_max - src_layer.scale_min,
										  InstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) /
				float(InstanceBlockData::SIMPLE_11B_V1_SCALE_RESOLUTION);

		const float rotation_error = 2.f / float(InstanceBlockData::SIMPLE_11B_V1_QUAT_RESOLUTION);

		// Compare instances
		for (unsigned int instance_index = 0; instance_index < src_layer.instances.size(); ++instance_index) {
			const InstanceBlockData::InstanceData &src_instance = src_layer.instances[instance_index];
			const InstanceBlockData::InstanceData &dst_instance = dst_layer.instances[instance_index];

			ZN_TEST_ASSERT(
					math::distance(src_instance.transform.origin, dst_instance.transform.origin) <= distance_error);

			const Basis src_basis = to_basis3(src_instance.transform.basis);
			const Basis dst_basis = to_basis3(dst_instance.transform.basis);

			const Vector3 src_scale = src_basis.get_scale();
			const Vector3 dst_scale = src_basis.get_scale();
			ZN_TEST_ASSERT(src_scale.distance_to(dst_scale) <= scale_error);

			// Had to normalize here because Godot doesn't want to give you a Quat if the basis is scaled (even
			// uniformly)
			const Quaternion src_rot = src_basis.orthonormalized().get_quaternion();
			const Quaternion dst_rot = dst_basis.orthonormalized().get_quaternion();
			const float rot_dx = Math::abs(src_rot.x - dst_rot.x);
			const float rot_dy = Math::abs(src_rot.y - dst_rot.y);
			const float rot_dz = Math::abs(src_rot.z - dst_rot.z);
			const float rot_dw = Math::abs(src_rot.w - dst_rot.w);
			ZN_TEST_ASSERT(rot_dx <= rotation_error);
			ZN_TEST_ASSERT(rot_dy <= rotation_error);
			ZN_TEST_ASSERT(rot_dz <= rotation_error);
			ZN_TEST_ASSERT(rot_dw <= rotation_error);
		}
	}
}

void test_transform_3d_array_zxy() {
	// YXZ
	int src_grid[] = {
		0, 1, 2, 3, //
		4, 5, 6, 7, //
		8, 9, 10, 11, //

		12, 13, 14, 15, //
		16, 17, 18, 19, //
		20, 21, 22, 23 //
	};
	const Vector3i src_size(3, 4, 2);
	const unsigned int volume = Vector3iUtil::get_volume(src_size);

	FixedArray<int, 24> dst_grid;
	ZN_TEST_ASSERT(dst_grid.size() == volume);

	{
		int expected_dst_grid[] = {
			0, 4, 8, //
			1, 5, 9, //
			2, 6, 10, //
			3, 7, 11, //

			12, 16, 20, //
			13, 17, 21, //
			14, 18, 22, //
			15, 19, 23 //
		};
		const Vector3i expected_dst_size(4, 3, 2);
		IntBasis basis;
		basis.x = Vector3i(0, 1, 0);
		basis.y = Vector3i(1, 0, 0);
		basis.z = Vector3i(0, 0, 1);

		const Vector3i dst_size =
				transform_3d_array_zxy(Span<const int>(src_grid, 0, volume), to_span(dst_grid), src_size, basis);

		ZN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
		}
	}
	{
		int expected_dst_grid[] = {
			3, 2, 1, 0, //
			7, 6, 5, 4, //
			11, 10, 9, 8, //

			15, 14, 13, 12, //
			19, 18, 17, 16, //
			23, 22, 21, 20 //
		};
		const Vector3i expected_dst_size(3, 4, 2);
		IntBasis basis;
		basis.x = Vector3i(1, 0, 0);
		basis.y = Vector3i(0, -1, 0);
		basis.z = Vector3i(0, 0, 1);

		const Vector3i dst_size =
				transform_3d_array_zxy(Span<const int>(src_grid, 0, volume), to_span(dst_grid), src_size, basis);

		ZN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
		}
	}
	{
		int expected_dst_grid[] = {
			15, 14, 13, 12, //
			19, 18, 17, 16, //
			23, 22, 21, 20, //

			3, 2, 1, 0, //
			7, 6, 5, 4, //
			11, 10, 9, 8 //
		};
		const Vector3i expected_dst_size(3, 4, 2);
		IntBasis basis;
		basis.x = Vector3i(1, 0, 0);
		basis.y = Vector3i(0, -1, 0);
		basis.z = Vector3i(0, 0, -1);

		const Vector3i dst_size =
				transform_3d_array_zxy(Span<const int>(src_grid, 0, volume), to_span(dst_grid), src_size, basis);

		ZN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
		}
	}
}

void test_get_curve_monotonic_sections() {
	// This one is a bit annoying to test because Curve has float precision issues stemming from the bake() function
	struct L {
		static bool is_equal_approx(float a, float b) {
			return Math::is_equal_approx(a, b, 2.f * CURVE_RANGE_MARGIN);
		}
	};
	{
		// One segment going up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[0].x_max == 1.f);
		ZN_TEST_ASSERT(sections[0].y_min == 0.f);
		ZN_TEST_ASSERT(sections[0].y_max == 1.f);
		{
			math::Interval yi = get_curve_range(**curve, sections, math::Interval(0.f, 1.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.min, 0.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.max, 1.f));
		}
		{
			math::Interval yi = get_curve_range(**curve, sections, math::Interval(-2.f, 2.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.min, 0.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.max, 1.f));
		}
		{
			math::Interval xi(0.2f, 0.8f);
			math::Interval yi = get_curve_range(**curve, sections, xi);
			math::Interval yi_expected(curve->sample_baked(xi.min), curve->sample_baked(xi.max));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.min, yi_expected.min));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.max, yi_expected.max));
		}
	}
	{
		// One flat segment
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[0].x_max == 1.f);
		ZN_TEST_ASSERT(sections[0].y_min == 0.f);
		ZN_TEST_ASSERT(sections[0].y_max == 0.f);
	}
	{
		// Two segments: going up, then flat
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Two segments: flat, then up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Three segments: flat, then up, then flat
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 0));
		curve->add_point(Vector2(0.6, 1));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Three segments: up, down, up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 1));
		curve->add_point(Vector2(0.6, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 3);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[2].x_max == 1.f);
	}
	{
		// Two segments: going up, then down
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 2);
	}
	{
		// One segment, curved as a parabola going up then down
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0), 0.f, 1.f);
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 2);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[0].y_max >= 0.1f);
		ZN_TEST_ASSERT(sections[1].x_max == 1.f);
	}
}

#ifdef VOXEL_ENABLE_FAST_NOISE_2

void test_fast_noise_2_basic() {
	// Very basic test. The point is to make sure it doesn't crash, so there is no special condition to check.
	Ref<FastNoise2> noise;
	noise.instantiate();
	float nv = noise->get_noise_2d_single(Vector2(42, 666));
	print_line(String("SIMD level: {0}").format(varray(FastNoise2::get_simd_level_name(noise->get_simd_level()))));
	print_line(String("Noise: {0}").format(varray(nv)));
	Ref<Image> im = create_empty_image(256, 256, false, Image::FORMAT_RGB8);
	noise->generate_image(im, false);
	// im->save_png("zylann_test_fastnoise2.png");
}

void test_fast_noise_2_empty_encoded_node_tree() {
	Ref<FastNoise2> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoise2::TYPE_ENCODED_NODE_TREE);
	// This can print an error, but should not crash
	noise->update_generator();
}

#endif

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

void test_flat_map() {
	struct Value {
		int i;
		bool operator==(const Value &other) const {
			return i == other.i;
		}
	};
	typedef FlatMap<int, Value>::Pair Pair;

	std::vector<Pair> sorted_pairs;
	for (int i = 0; i < 100; ++i) {
		sorted_pairs.push_back(Pair{ i, Value{ 1000 * i } });
	}
	const int inexistent_key1 = 101;
	const int inexistent_key2 = -1;

	struct L {
		static bool validate_map(const FlatMap<int, Value> &map, const std::vector<Pair> &sorted_pairs) {
			ZN_TEST_ASSERT_V(sorted_pairs.size() == map.size(), false);
			for (size_t i = 0; i < sorted_pairs.size(); ++i) {
				const Pair expected_pair = sorted_pairs[i];
				ZN_TEST_ASSERT_V(map.has(expected_pair.key), false);
				ZN_TEST_ASSERT_V(map.find(expected_pair.key) != nullptr, false);
				const Value *value = map.find(expected_pair.key);
				ZN_TEST_ASSERT_V(value != nullptr, false);
				ZN_TEST_ASSERT_V(*value == expected_pair.value, false);
			}
			return true;
		}
	};

	std::vector<Pair> shuffled_pairs = sorted_pairs;
	RandomPCG rng;
	rng.seed(131183);
	for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
		size_t dst_i = rng.rand() % shuffled_pairs.size();
		const Pair temp = shuffled_pairs[dst_i];
		shuffled_pairs[dst_i] = shuffled_pairs[i];
		shuffled_pairs[i] = temp;
	}

	{
		// Insert pre-sorted pairs
		FlatMap<int, Value> map;
		for (size_t i = 0; i < sorted_pairs.size(); ++i) {
			const Pair pair = sorted_pairs[i];
			ZN_TEST_ASSERT(map.insert(pair.key, pair.value));
		}
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Insert random pairs
		FlatMap<int, Value> map;
		for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
			const Pair pair = shuffled_pairs[i];
			ZN_TEST_ASSERT(map.insert(pair.key, pair.value));
		}
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Insert random pairs with duplicates
		FlatMap<int, Value> map;
		for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
			const Pair pair = shuffled_pairs[i];
			ZN_TEST_ASSERT(map.insert(pair.key, pair.value));
			ZN_TEST_ASSERT_MSG(!map.insert(pair.key, pair.value), "Inserting the key a second time should fail");
		}
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Init from collection
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Inexistent items
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		ZN_TEST_ASSERT(!map.has(inexistent_key1));
		ZN_TEST_ASSERT(!map.has(inexistent_key2));
	}
	{
		// Iteration
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		size_t i = 0;
		for (FlatMap<int, Value>::ConstIterator it = map.begin(); it != map.end(); ++it) {
			ZN_TEST_ASSERT(i < sorted_pairs.size());
			const Pair expected_pair = sorted_pairs[i];
			ZN_TEST_ASSERT(expected_pair.key == it->key);
			ZN_TEST_ASSERT(expected_pair.value == it->value);
			++i;
		}
	}
}

void test_voxel_mesher_cubes() {
	VoxelBufferInternal vb;
	vb.create(8, 8, 8);
	vb.set_channel_depth(VoxelBufferInternal::CHANNEL_COLOR, VoxelBufferInternal::DEPTH_16_BIT);
	vb.set_voxel(Color8(0, 255, 0, 255).to_u16(), Vector3i(3, 4, 4), VoxelBufferInternal::CHANNEL_COLOR);
	vb.set_voxel(Color8(0, 255, 0, 255).to_u16(), Vector3i(4, 4, 4), VoxelBufferInternal::CHANNEL_COLOR);
	vb.set_voxel(Color8(0, 0, 255, 128).to_u16(), Vector3i(5, 4, 4), VoxelBufferInternal::CHANNEL_COLOR);

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

void test_issue463() {
	Ref<VoxelMeshSDF> msdf;
	msdf.instantiate();

	Dictionary d;
	d["roman"] = 22;
	d[22] = 25;
	// TODO The original report was creating a BoxShape3D, but for reasons beyond my understanding, Godot's
	// PhysicsServer3D is still not created after `MODULE_INITIALIZATION_LEVEL_SERVERS`. And not even SCENE or EDITOR
	// levels. It's impossible to use a level to do anything with physics.... Go figure.
	//
	// Ref<BoxShape3D> shape1;
	// shape1.instantiate();
	// Ref<BoxShape3D> shape2;
	// shape2.instantiate();
	// d[shape1] = shape2;
	Ref<Resource> res1;
	res1.instantiate();
	Ref<Resource> res2;
	res2.instantiate();
	d[res1] = res2;

	ZN_ASSERT(msdf->has_method("_set_data"));
	// Setting invalid data should cause an error but not crash or leak
	msdf->call("_set_data", d);
}

void test_slot_map() {
	SlotMap<int> map;

	const SlotMap<int>::Key key1 = map.add(1);
	const SlotMap<int>::Key key2 = map.add(2);
	const SlotMap<int>::Key key3 = map.add(3);

	ZN_TEST_ASSERT(key1 != key2 && key2 != key3);
	ZN_TEST_ASSERT(map.exists(key1));
	ZN_TEST_ASSERT(map.exists(key2));
	ZN_TEST_ASSERT(map.exists(key3));
	ZN_TEST_ASSERT(map.count() == 3);

	map.remove(key2);
	ZN_TEST_ASSERT(map.exists(key1));
	ZN_TEST_ASSERT(!map.exists(key2));
	ZN_TEST_ASSERT(map.exists(key3));
	ZN_TEST_ASSERT(map.count() == 2);

	const SlotMap<int>::Key key4 = map.add(4);
	ZN_TEST_ASSERT(key4 != key2);
	ZN_TEST_ASSERT(map.count() == 3);

	const int v1 = map.get(key1);
	const int v4 = map.get(key4);
	const int v3 = map.get(key3);
	ZN_TEST_ASSERT(v1 == 1);
	ZN_TEST_ASSERT(v4 == 4);
	ZN_TEST_ASSERT(v3 == 3);

	map.clear();
	ZN_TEST_ASSERT(!map.exists(key1));
	ZN_TEST_ASSERT(!map.exists(key4));
	ZN_TEST_ASSERT(!map.exists(key3));
	ZN_TEST_ASSERT(map.count() == 0);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VOXEL_TEST(fname)                                                                                              \
	{                                                                                                                  \
		print_line(String("Running {0}").format(varray(#fname)));                                                      \
		ZN_PROFILE_SCOPE_NAMED(#fname);                                                                                \
		fname();                                                                                                       \
	}

void run_voxel_tests() {
	print_line("------------ Voxel tests begin -------------");

	using namespace zylann::tests;

	VOXEL_TEST(test_box3i_intersects);
	VOXEL_TEST(test_box3i_for_inner_outline);
	VOXEL_TEST(test_voxel_data_map_paste_fill);
	VOXEL_TEST(test_voxel_data_map_paste_mask);
	VOXEL_TEST(test_voxel_data_map_copy);
	VOXEL_TEST(test_encode_weights_packed_u16);
	VOXEL_TEST(test_copy_3d_region_zxy);
	VOXEL_TEST(test_voxel_graph_invalid_connection);
	VOXEL_TEST(test_voxel_graph_generator_default_graph_compilation);
	VOXEL_TEST(test_voxel_graph_sphere_on_plane);
	VOXEL_TEST(test_voxel_graph_clamp_simplification);
	VOXEL_TEST(test_voxel_graph_generator_expressions);
	VOXEL_TEST(test_voxel_graph_generator_expressions_2);
	VOXEL_TEST(test_voxel_graph_generator_texturing);
	VOXEL_TEST(test_voxel_graph_equivalence_merging);
	VOXEL_TEST(test_voxel_graph_generate_block_with_input_sdf);
	VOXEL_TEST(test_voxel_graph_functions_pass_through);
	VOXEL_TEST(test_voxel_graph_functions_nested_pass_through);
	VOXEL_TEST(test_voxel_graph_functions_autoconnect);
	VOXEL_TEST(test_voxel_graph_functions_io_mismatch);
	VOXEL_TEST(test_voxel_graph_functions_misc);
	VOXEL_TEST(test_voxel_graph_issue461);
	VOXEL_TEST(test_voxel_graph_fuzzing);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	VOXEL_TEST(test_voxel_graph_issue427);
#ifdef TOOLS_ENABLED
	VOXEL_TEST(test_voxel_graph_hash);
#endif
#endif
	VOXEL_TEST(test_voxel_graph_issue471);
	VOXEL_TEST(test_voxel_graph_unused_single_texture_output);
	VOXEL_TEST(test_voxel_graph_spots2d_optimized_execution_map);
	VOXEL_TEST(test_voxel_graph_unused_inner_output);
	VOXEL_TEST(test_island_finder);
	VOXEL_TEST(test_unordered_remove_if);
	VOXEL_TEST(test_instance_data_serialization);
	VOXEL_TEST(test_transform_3d_array_zxy);
	VOXEL_TEST(test_octree_update);
	VOXEL_TEST(test_octree_find_in_box);
	VOXEL_TEST(test_get_curve_monotonic_sections);
	VOXEL_TEST(test_voxel_buffer_create);
	VOXEL_TEST(test_block_serializer);
	VOXEL_TEST(test_block_serializer_stream_peer);
	VOXEL_TEST(test_region_file);
	VOXEL_TEST(test_voxel_stream_region_files);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	VOXEL_TEST(test_fast_noise_2_basic);
	VOXEL_TEST(test_fast_noise_2_empty_encoded_node_tree);
#endif
	VOXEL_TEST(test_run_blocky_random_tick);
	VOXEL_TEST(test_flat_map);
	VOXEL_TEST(test_expression_parser);
	VOXEL_TEST(test_voxel_buffer_metadata);
	VOXEL_TEST(test_voxel_buffer_metadata_gd);
	VOXEL_TEST(test_voxel_mesher_cubes);
	VOXEL_TEST(test_threaded_task_runner_misc);
	VOXEL_TEST(test_threaded_task_runner_debug_names);
	VOXEL_TEST(test_task_priority_values);
	VOXEL_TEST(test_issue463);
	VOXEL_TEST(test_normalmap_render_gpu);
	VOXEL_TEST(test_slot_map);
	VOXEL_TEST(test_box_blur);
	VOXEL_TEST(test_threaded_task_postponing);
	VOXEL_TEST(test_spatial_lock_misc);
	VOXEL_TEST(test_spatial_lock_spam);
	VOXEL_TEST(test_spatial_lock_dependent_map_chunks);

	print_line("------------ Voxel tests end -------------");
}

} // namespace zylann::voxel::tests
