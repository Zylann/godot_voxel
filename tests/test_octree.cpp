#include "test_octree.h"
#include "../constants/cube_tables.h"
#include "../terrain/lod_octree.h"
#include "../util/profiling_clock.h"
#include <core/map.h>
#include <core/print_string.h>
#include <unordered_map>
#include <unordered_set>

void test_octree_update() {
	static const float lod_distance = 80;
	static const float view_distance = 1024;
	static const int lod_count = 6;
	static const int block_size = 16;
	const Vector3 block_size_v(block_size, block_size, block_size);
	Vector3 viewer_pos = Vector3(100, 50, 200);
	const int octree_size = block_size << (lod_count - 1);

	// Testing as an octree forest, as it is the way they are used in VoxelLodTerrain
	Map<Vector3i, LodOctree> octrees;
	const Box3i viewer_box_voxels =
			Box3i::from_center_extents(Vector3i::from_floored(viewer_pos), Vector3i(view_distance));
	const Box3i viewer_box_octrees = viewer_box_voxels.downscaled(octree_size);
	viewer_box_octrees.for_each_cell([&octrees](Vector3i pos) {
		Map<Vector3i, LodOctree>::Element *e = octrees.insert(pos, LodOctree());
		LodOctree &octree = e->value();
		LodOctree::NoDestroyAction nda;
		octree.create_from_lod_count(block_size, lod_count, nda);
		octree.set_lod_distance(lod_distance);
	});

	struct OctreeActions {
		int created_count = 0;
		int destroyed_count = 0;

		void create_child(Vector3i node_pos, int lod_index, LodOctree::NodeData &data) {
			++created_count;
		}

		void destroy_child(Vector3i node_pos, int lod_index) {
			++destroyed_count;
		}

		void show_parent(Vector3i node_pos, int lod_index) {
		}

		void hide_parent(Vector3i node_pos, int lod_index) {
		}

		bool can_create_root(int lod_index) {
			return true;
		}

		bool can_split(Vector3i node_pos, int child_lod_index, LodOctree::NodeData &data) {
			return true;
		}

		bool can_join(Vector3i node_pos, int parent_lod_index) {
			return true;
		}
	};

	int initial_block_count = 0;
	ProfilingClock profiling_clock;

	// Initial
	// Needs multiple passes because the current version is not recursive...
	for (int i = 0; i < 10; ++i) {
		for (Map<Vector3i, LodOctree>::Element *e = octrees.front(); e; e = e->next()) {
			LodOctree &octree = e->value();

			const Vector3i block_pos_maxlod = e->key();
			const Vector3i block_offset_lod0 = block_pos_maxlod << (lod_count - 1);
			const Vector3 relative_viewer_pos = viewer_pos - block_size_v * block_offset_lod0.to_vec3();

			OctreeActions actions;
			octree.update(relative_viewer_pos, actions);
			initial_block_count += actions.created_count;
			ERR_FAIL_COND(actions.destroyed_count != 0);
		}
	}

	const int time_init = profiling_clock.restart();

	int initial_block_count_rec = 0;
	for (Map<Vector3i, LodOctree>::Element *e = octrees.front(); e; e = e->next()) {
		const LodOctree &octree = e->value();
		initial_block_count_rec += octree.get_node_count();
	}

	print_line(String("Initial block count: {0}, time: {1} us").format(varray(initial_block_count, time_init)));
	ERR_FAIL_COND(initial_block_count <= 0);
	ERR_FAIL_COND(initial_block_count != initial_block_count_rec);

	// Updates without moving
	int created_block_count = 0;
	int destroyed_block_count = 0;
	for (int i = 0; i < 10; ++i) {
		profiling_clock.restart();

		created_block_count = 0;
		for (Map<Vector3i, LodOctree>::Element *e = octrees.front(); e; e = e->next()) {
			LodOctree &octree = e->value();

			const Vector3i block_pos_maxlod = e->key();
			const Vector3i block_offset_lod0 = block_pos_maxlod << (lod_count - 1);
			const Vector3 relative_viewer_pos = viewer_pos - block_size_v * block_offset_lod0.to_vec3();

			OctreeActions actions;
			octree.update(relative_viewer_pos, actions);
			created_block_count += actions.created_count;
			destroyed_block_count += actions.destroyed_count;
		}

		const int time_stay = profiling_clock.restart();

		// Block count should not change
		ERR_FAIL_COND(created_block_count != 0);
		ERR_FAIL_COND(destroyed_block_count != 0);
		print_line(String("Stay time: {0} us").format(varray(time_stay)));
	}

	// Clearing
	int block_count = initial_block_count;
	for (Map<Vector3i, LodOctree>::Element *e = octrees.front(); e; e = e->next()) {
		LodOctree &octree = e->value();

		struct DestroyAction {
			int destroyed_blocks = 0;
			inline void operator()(Vector3i node_pos, int lod) {
				++destroyed_blocks;
			}
		};

		DestroyAction da;
		octree.clear(da);

		block_count -= da.destroyed_blocks;
	}

	ERR_FAIL_COND(block_count != 0);
}

void test_octree_find_in_box() {
	const int blocks_across = 32;
	const int block_size = 16;
	int lods = 0;
	{
		int diameter = blocks_across;
		while (diameter > 0) {
			diameter = diameter >> 1;
			lods += 1;
		}
		//print_line(String("Lod count: {0}").format(varray(lods)));
	}

	// Build a fully populated octree with all its leaves at LOD0
	LodOctree octree;
	LodOctree::NoDestroyAction nda;
	octree.create_from_lod_count(block_size, lods, nda);
	struct SubdivideActions {
		bool can_split(Vector3i node_pos, int lod_index, const LodOctree::NodeData &node_data) {
			return true;
		}
		void create_child(Vector3i pos, int lod_index, LodOctree::NodeData &node_data) {
			node_data.state = pos.x + pos.y + pos.z;
		}
	};
	SubdivideActions sa;
	octree.subdivide(sa);

	std::unordered_map<Vector3i, std::unordered_set<Vector3i>> expected_positions;

	const Box3i full_box(Vector3i(), Vector3i(blocks_across, blocks_across, blocks_across));

	// Build expected result
	full_box.for_each_cell([full_box, &expected_positions](Vector3i pos) {
		Box3i area_box(pos - Vector3i(1, 1, 1), Vector3i(3, 3, 3));
		area_box.clip(full_box);
		auto insert_result = expected_positions.insert(std::make_pair(pos, std::unordered_set<Vector3i>()));
		ERR_FAIL_COND(insert_result.second == false);
		std::unordered_set<Vector3i> &area_positions = insert_result.first->second;
		area_box.for_each_cell([&area_positions](Vector3i npos) {
			auto it = area_positions.insert(npos);
			ERR_FAIL_COND(it.second == false);
		});
	});

	// Get octree results
	int checksum = 0;
	full_box.for_each_cell([&octree, &expected_positions, &checksum](Vector3i pos) {
		const Box3i area_box(pos - Vector3i(1, 1, 1), Vector3i(3, 3, 3));
		auto it = expected_positions.find(pos);
		ERR_FAIL_COND(it == expected_positions.end());
		const std::unordered_set<Vector3i> &expected_area_positions = it->second;
		std::unordered_set<Vector3i> found_positions;
		octree.for_leaves_in_box(area_box, [&found_positions, &expected_area_positions, &checksum](Vector3i node_pos, int lod, const LodOctree::NodeData &node_data) {
			auto insert_result = found_positions.insert(node_pos);
			// Must be one of the expected positions
			ERR_FAIL_COND(expected_area_positions.find(node_pos) == expected_area_positions.end());
			// Must not be a duplicate
			ERR_FAIL_COND(insert_result.second == false);
			checksum += node_data.state;
		});
	});

	// Doing it again just to measure time
	{
		ProfilingClock profiling_clock;
		int checksum2 = 0;
		full_box.for_each_cell([&octree, &checksum2](Vector3i pos) {
			const Box3i area_box(pos - Vector3i(1, 1, 1), Vector3i(3, 3, 3));
			octree.for_leaves_in_box(area_box, [&checksum2](Vector3i node_pos, int lod, const LodOctree::NodeData &node_data) {
				checksum2 += node_data.state;
			});
		});
		ERR_FAIL_COND(checksum2 != checksum);
		const int for_each_cell_time = profiling_clock.restart();
		const float single_query_time = float(for_each_cell_time) / full_box.size.volume();
		print_line(String("for_each_cell time with {0} lods: total {1} us, single query {2} us, checksum: {3}")
						   .format(varray(lods, for_each_cell_time, single_query_time, checksum2)));
	}
}
