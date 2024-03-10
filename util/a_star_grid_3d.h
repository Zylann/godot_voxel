#ifndef ZN_ASTAR_GRID_3D_H
#define ZN_ASTAR_GRID_3D_H

#include "../util/containers/std_unordered_map.h"
#include "../util/containers/std_vector.h"
#include "../util/godot/core/sort_array.h"
#include "../util/math/box3i.h"
#include "../util/math/vector3f.h"
#include <limits>
#include <unordered_map>

namespace zylann {

// Variant of AStar specialized in 3D grids.
// This implementation can be executed step by step, for debugging or spreading cost over time.
class AStarGrid3D {
public:
	AStarGrid3D();
	virtual ~AStarGrid3D() {}

	void set_agent_size(Vector3f size);
	Vector3f get_agent_size() const {
		return _agent_size;
	}

	void set_max_fall_height(int h);
	int get_max_fall_height() const;

	void set_max_path_cost(float cost);
	float get_max_path_cost() const;

	void set_region(Box3i region);
	const Box3i &get_region() const {
		return _region;
	}

	void start(Vector3i from_position, Vector3i target_position);
	void step();
	bool is_running() const;
	Span<const Vector3i> get_path() const;
	void clear();

	// Debug

	void debug_get_visited_points(StdVector<Vector3i> &out_positions) const;
	bool debug_get_next_step_point(Vector3i &out_pos) const;

protected:
	virtual bool is_solid(Vector3i pos);

private:
	float evaluate_heuristic(Vector3i pos, Vector3i target_pos) const;
	void reconstruct_path(uint32_t end_point_index);
	void get_neighbor_positions(Vector3i pos, StdVector<Vector3i> &out_positions);
	bool is_ground_close_enough(Vector3i pos);
	bool fits(Vector3f pos, Vector3f agent_extents);

	struct Point {
		static const uint32_t NO_CAME_FROM = std::numeric_limits<uint32_t>::max();

		Vector3i position;

		// Sum of all costs travelling every point until reaching this point, along the best path so far
		float gscore;

		// Estimated cost from this point to the destination
		float fscore;

		// Preceding point along the best path so far
		uint32_t came_from_point_index;

		bool in_open_set;
	};

	struct ComparePoints {
		StdVector<Point> *pool;

		inline bool operator()(uint32_t ai, uint32_t bi) const {
			const Point &a = (*pool)[ai];
			const Point &b = (*pool)[bi];
			return a.fscore > b.fscore;

			// The following spams "bad comparison function", no clue why, despite being the way AStarGrid2D works.
			// It goes away when replacing >= with >, but then it's no longer the same logic.
			// From my profilings so far, it also makes no difference
			// if (a.fscore < b.fscore) {
			// 	return true;
			// }
			// if (a.fscore > b.fscore) {
			// 	return false;
			// }
			// // If the fscores are the same then prioritize the points that are further away from the start.
			// return a.gscore >= b.gscore;
		}
	};

	struct PriorityQueue {
		SortArray<uint32_t, ComparePoints> sorter;
		StdVector<uint32_t> items;

		inline uint32_t peek() const {
			return items[0];
		}

		inline void pop() {
			// Remove the current point from the open list.
			sorter.pop_heap(0, items.size(), items.data());
			items.pop_back();
		}

		inline void push(uint32_t v) {
			items.push_back(v);
			sorter.push_heap(0, items.size() - 1, 0, v, items.data());
		}

		inline unsigned int size() const {
			return items.size();
		}

		inline void clear() {
			items.clear();
		}

		void update_priority(uint32_t v) {
			for (unsigned int i = 0; i < items.size(); ++i) {
				// This would normally need a way to define a custom equality comparison, but in our use case it works
				if (items[i] == v) {
					sorter.push_heap(0, i, 0, v, items.data());
					break;
				}
			}
		}
	};

	uint32_t _start_point_index;
	Vector3i _target_position;
	bool _is_running = false;
	// Agent size, defaulting to a player 2 voxels tall and 1 voxel wide. Should be slightly lower than one voxel to
	// give some wiggle room.
	// TODO Specify agent origin manually, because it's tricky to check if it fits a cell when it is larger than one
	// cell. The algorithm only travels through cell centers, so depending on the agent's origin, some paths will never
	// be taken, even though it could if it could travel between cells. Usually a good choice is to divide the agent
	// into grid cells body parts can fit in, and put its origin at the center of its lower-left corner. This needs to
	// be taken into account when reading the final path.
	Vector3f _agent_size = Vector3f(0.8f, 1.8f, 0.8f);
	Vector3f _fitting_offset;
	int _max_fall_height = 3;

	// Nodes with a cumulated edge cost greater than this will be ignored.
	// This limits the effective cost a path can have, in addition to the region check.
	// By default, edge cost is distance, so it would be maximum path length.
	float _max_path_cost = 1000.f;

	Box3i _region;
	StdVector<Point> _points_pool;
	PriorityQueue _open_list;

	// Only visited points will be in this map. Should use less memory than if we made a big 3D grid of points, because
	// in practice we may only visit a fraction of them.
	// Eventually we could try a chunked grid if that's faster?
	StdUnorderedMap<Vector3i, uint32_t> _points_map;

	StdVector<Vector3i> _path;
	StdVector<Vector3i> _neighbor_positions;
};

} // namespace zylann

#endif // ZN_ASTAR_GRID_3D_H
