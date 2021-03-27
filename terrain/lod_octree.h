#ifndef LOD_OCTREE_H
#define LOD_OCTREE_H

#include "../constants/octree_tables.h"
#include "../constants/voxel_constants.h"
#include "../util/math/vector3i.h"

// Octree designed to handle level of detail.
class LodOctree {
public:
	static const unsigned int NO_CHILDREN = -1;
	static const unsigned int ROOT_INDEX = -1; // Root node isn't stored in pool

	struct Node {
		// Index to first child node within the node pool.
		// The 7 next indexes are the other children.
		// If the node isn't subdivided, it is set to NO_CHILDREN.
		// Could have used a pointer but an index is enough, occupies half memory and is immune to realloc
		unsigned int first_child;

		// No userdata... I removed it because it was never actually used.
		// May add it back if the need comes again.

		// Node positions are calculated on the fly to save memory,
		// and divided by chunk size at the current LOD,
		// so it is sequential within each LOD, which makes it usable for grid storage

		Node() {
			init();
		}

		inline bool has_children() const {
			return first_child != NO_CHILDREN;
		}

		inline void init() {
			first_child = NO_CHILDREN;
		}
	};

	struct NoDestroyAction {
		inline void operator()(Vector3i node_pos, int lod) {}
	};

	template <typename DestroyAction_T>
	void clear(DestroyAction_T &destroy_action) {
		join_all_recursively(&_root, Vector3i(), _max_depth, destroy_action);
		_is_root_created = false;
		_max_depth = 0;
		_base_size = 0;
	}

	static int compute_lod_count(int base_size, int full_size) {
		int po = 0;
		while (full_size > base_size) {
			full_size = full_size >> 1;
			po += 1;
		}
		return po;
	}

	template <typename DestroyAction_T>
	void create_from_lod_count(int base_size, unsigned int lod_count, DestroyAction_T &destroy_action) {
		ERR_FAIL_COND(lod_count > VoxelConstants::MAX_LOD);
		clear(destroy_action);
		_base_size = base_size;
		_max_depth = lod_count - 1;
	}

	unsigned int get_lod_count() const {
		return _max_depth + 1;
	}

	// The higher, the longer LODs will spread and higher the quality.
	// The lower, the shorter LODs will spread and lower the quality.
	void set_lod_distance(float p_lod_distance) {
		// Distance must be greater than a threshold,
		// otherwise lods will decimate too fast and it will look messy
		_lod_distance =
				clamp(p_lod_distance, VoxelConstants::MINIMUM_LOD_DISTANCE, VoxelConstants::MAXIMUM_LOD_DISTANCE);
	}

	float get_lod_distance() const {
		return _lod_distance;
	}

	static inline int get_lod_factor(int lod) {
		return 1 << lod;
	}

	// Signature examples
	struct DefaultUpdateActions {
		void create_child(Vector3i node_pos, int lod) {} // Occurs on split
		void destroy_child(Vector3i node_pos, int lod) {} // Occurs on merge
		void show_parent(Vector3i node_pos, int lod) {} // Occurs on merge
		void hide_parent(Vector3i node_pos, int lod) {} // Occurs on split
		bool can_create_root(int lod) { return true; }
		bool can_split(Vector3i node_pos, int child_lod_index) { return true; }
		bool can_join(Vector3i node_pos, int lod) { return true; }
	};

	template <typename UpdateActions_T>
	void update(Vector3 view_pos, UpdateActions_T &actions) {
		if (_is_root_created || _root.has_children()) {
			update(ROOT_INDEX, Vector3i(), _max_depth, view_pos, actions);
		} else {
			// TODO I don't like this much
			// Treat the root in a slightly different way the first time.
			if (actions.can_create_root(_max_depth)) {
				actions.create_child(Vector3i(), _max_depth);
				_is_root_created = true;
			}
		}
	}

	static inline Vector3i get_child_position(Vector3i parent_position, int i) {
		return Vector3i(
				parent_position.x * 2 + OctreeTables::g_octant_position[i][0],
				parent_position.y * 2 + OctreeTables::g_octant_position[i][1],
				parent_position.z * 2 + OctreeTables::g_octant_position[i][2]);
	}

	const Node *get_root() const {
		return &_root;
	}

	const Node *get_child(const Node *node, unsigned int i) const {
		ERR_FAIL_COND_V(node == nullptr, nullptr);
		ERR_FAIL_INDEX_V(i, 8, nullptr);
		return get_node(node->first_child + i);
	}

private:
	// This pool treats nodes as packs of 8 so they can be addressed by only knowing the first child
	class NodePool {
	public:
		// Warning: the returned pointer may be invalidated later by `allocate_children`. Use with care.
		inline Node *get_node(unsigned int i) {
			CRASH_COND(i >= _nodes.size());
			CRASH_COND(i == ROOT_INDEX);
			return &_nodes[i];
		}

		inline const Node *get_node(unsigned int i) const {
			CRASH_COND(i >= _nodes.size());
			CRASH_COND(i == ROOT_INDEX);
			return &_nodes[i];
		}

		unsigned int allocate_children() {
			if (_free_indexes.size() == 0) {
				unsigned int i0 = _nodes.size();
				_nodes.resize(i0 + 8);
				return i0;
			} else {
				unsigned int i0 = _free_indexes[_free_indexes.size() - 1];
				_free_indexes.pop_back();
				return i0;
			}
		}

		// Warning: this is not recursive. Use it properly.
		void recycle_children(unsigned int i0) {

			// Debug check, there is no use case in recycling a node which is not a first child
			CRASH_COND(i0 % 8 != 0);

			for (unsigned int i = 0; i < 8; ++i) {
				_nodes[i0 + i].init();
			}

			_free_indexes.push_back(i0);
		}

	private:
		// TODO If this grows too much, mayyybe could implement a paged vector to fight fragmentation.
		// If we do so, that may also solve pointer invalidation since pages would remain stable
		std::vector<Node> _nodes;
		std::vector<unsigned int> _free_indexes;
	};

	inline Node *get_node(unsigned int index) {
		if (index == ROOT_INDEX) {
			return &_root;
		} else {
			return _pool.get_node(index);
		}
	}

	inline const Node *get_node(unsigned int index) const {
		if (index == ROOT_INDEX) {
			return &_root;
		} else {
			return _pool.get_node(index);
		}
	}

	template <typename UpdateActions_T>
	void update(unsigned int node_index, Vector3i node_pos, int lod, Vector3 view_pos, UpdateActions_T &actions) {
		// This function should be called regularly over frames.

		const int lod_factor = get_lod_factor(lod);
		const int chunk_size = _base_size * lod_factor;
		const Vector3 world_center = static_cast<real_t>(chunk_size) * (node_pos.to_vec3() + Vector3(0.5, 0.5, 0.5));
		const float split_distance = _lod_distance * lod_factor;
		Node *node = get_node(node_index);

		if (!node->has_children()) {
			// If it's not the last LOD, if close enough and custom conditions get fulfilled
			if (lod > 0 && world_center.distance_to(view_pos) < split_distance && actions.can_split(node_pos, lod - 1)) {
				// Split
				const unsigned int first_child = _pool.allocate_children();
				// Get node again because `allocate_children` may invalidate the pointer
				node = get_node(node_index);
				node->first_child = first_child;

				for (unsigned int i = 0; i < 8; ++i) {
					actions.create_child(get_child_position(node_pos, i), lod - 1);
					// If the node needs to split more, we'll ask more recycling at the next frame...
					// That means the initialization of the game should do some warm up and fetch all leaves,
					// otherwise it's gonna be rough
				}

				actions.hide_parent(node_pos, lod);
			}

		} else {
			bool has_split_child = false;
			const unsigned int first_child = node->first_child;

			for (unsigned int i = 0; i < 8; ++i) {
				const unsigned int child_index = first_child + i;
				update(child_index, get_child_position(node_pos, i), lod - 1, view_pos, actions);
				has_split_child |= _pool.get_node(child_index)->has_children();
			}

			// Get node again because `update` may invalidate the pointer
			node = get_node(node_index);

			if (!has_split_child && world_center.distance_to(view_pos) > split_distance && actions.can_join(node_pos, lod)) {
				// Join
				if (node->has_children()) {

					for (unsigned int i = 0; i < 8; ++i) {
						actions.destroy_child(get_child_position(node_pos, i), lod - 1);
					}

					_pool.recycle_children(first_child);
					node->first_child = NO_CHILDREN;

					actions.show_parent(node_pos, lod);
				}
			}
		}
	}

	template <typename DestroyAction_T>
	void join_all_recursively(Node *node, Vector3i node_pos, int lod, DestroyAction_T &destroy_action) {
		// We can use pointers here because we won't allocate new nodes,
		// and won't shrink the node pool either

		if (node->has_children()) {
			unsigned int first_child = node->first_child;

			for (unsigned int i = 0; i < 8; ++i) {
				Node *child = _pool.get_node(first_child + i);
				join_all_recursively(child, get_child_position(node_pos, i), lod - 1, destroy_action);
			}

			_pool.recycle_children(first_child);
			node->first_child = NO_CHILDREN;

		} else {
			destroy_action(node_pos, lod);
		}
	}

	Node _root;
	bool _is_root_created = false;
	int _max_depth = 0;
	float _base_size = 16;
	float _lod_distance = 32.0;
	// TODO May be worth making this pool external for sharing purpose
	NodePool _pool;
};

// Notes:
// Population of an octree given its depth, thanks to Sage:
// ((1 << 3 * (depth + 1)) - 1 ) / 7

#endif // LOD_OCTREE_H
