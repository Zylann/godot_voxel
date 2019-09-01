#ifndef LOD_OCTREE_H
#define LOD_OCTREE_H

#include "../math/vector3i.h"
#include "../octree_tables.h"

// Octree designed to handle level of detail.
template <class T>
class LodOctree {
public:
	static const unsigned int NO_CHILDREN = -1;
	static const unsigned int ROOT_INDEX = -1; // Root node isn't stored in pool
	static const unsigned int MAX_LOD = 32;

	struct Node {
		// Index to first child node within the node pool.
		// The 7 next indexes are the other children.
		// If the node isn't subdivided, it is set to NO_CHILDREN.
		// Could have used a pointer but an index is enough, occupies half memory and is immune to realloc
		unsigned int first_child;

		// Whatever data to associate to the node, when it's a leaf.
		// It needs to be booleanizable, where `true` means presence of data, and `false` means no data.
		T block;

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
			block = T();
			first_child = NO_CHILDREN;
		}
	};

	// This pool treats nodes as packs of 8 so they can be addressed by only knowing the first child
	class NodePool {
	public:
		// Warning: the returned pointer may be invalidated later by `allocate_children`. Use with care.
		inline Node *get_node(unsigned int i) {
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

	struct NoDestroyAction {
		inline void operator()(Node *node, Vector3i node_pos, int lod) {}
	};

	template <typename A>
	void clear(A &destroy_action) {
		join_all_recursively(&_root, Vector3i(), _max_depth, destroy_action);
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

	template <typename A>
	void create_from_lod_count(int base_size, unsigned int lod_count, A &destroy_action) {
		ERR_FAIL_COND(lod_count > MAX_LOD);
		clear(destroy_action);
		_base_size = base_size;
		_max_depth = lod_count - 1;
	}

	unsigned int get_lod_count() const {
		return _max_depth + 1;
	}

	// The higher, the longer LODs will spread and higher the quality.
	// The lower, the shorter LODs will spread and lower the quality.
	void set_split_scale(float p_split_scale) {

		const float minv = 2.0;
		const float maxv = 5.0;

		// Split scale must be greater than a threshold,
		// otherwise lods will decimate too fast and it will look messy
		if (p_split_scale < minv) {
			p_split_scale = minv;
		} else if (p_split_scale > maxv) {
			p_split_scale = maxv;
		}

		_split_scale = p_split_scale;
	}

	float get_split_scale() const {
		return _split_scale;
	}

	static inline int get_lod_factor(int lod) {
		return 1 << lod;
	}

	template <typename A, typename B>
	void update(Vector3 view_pos, A &create_action, B &destroy_action) {

		if (_root.block || _root.has_children()) {
			update(ROOT_INDEX, Vector3i(), _max_depth, view_pos, create_action, destroy_action);

		} else {
			// Treat the root in a slightly different way the first time.
			if (create_action.can_do_root(_max_depth)) {
				_root.block = create_action(&_root, Vector3i(), _max_depth);
			}
		}
	}

	static inline Vector3i get_child_position(Vector3i parent_position, int i) {
		return Vector3i(
				parent_position.x * 2 + OctreeTables::g_octant_position[i][0],
				parent_position.y * 2 + OctreeTables::g_octant_position[i][1],
				parent_position.z * 2 + OctreeTables::g_octant_position[i][2]);
	}

private:
	inline Node *get_node(unsigned int index) {
		if (index == ROOT_INDEX) {
			return &_root;
		} else {
			return _pool.get_node(index);
		}
	}

	template <typename A, typename B>
	void update(unsigned int node_index, Vector3i node_pos, int lod, Vector3 view_pos, A &create_action, B &destroy_action) {
		// This function should be called regularly over frames.

		int lod_factor = get_lod_factor(lod);
		int chunk_size = _base_size * lod_factor;
		Vector3 world_center = static_cast<real_t>(chunk_size) * (node_pos.to_vec3() + Vector3(0.5, 0.5, 0.5));
		float split_distance = chunk_size * _split_scale;
		Node *node = get_node(node_index);

		if (!node->has_children()) {

			// If it's not the last LOD, if close enough and custom conditions get fulfilled
			if (lod > 0 && world_center.distance_to(view_pos) < split_distance && create_action.can_do_children(node, node_pos, lod - 1)) {
				// Split

				unsigned int first_child = _pool.allocate_children();
				// Get node again because `allocate_children` may invalidate the pointer
				node = get_node(node_index);
				node->first_child = first_child;

				for (unsigned int i = 0; i < 8; ++i) {

					Node *child = _pool.get_node(first_child + i);

					child->block = create_action(child, get_child_position(node_pos, i), lod - 1);

					// If the node needs to split more, we'll ask more recycling at the next frame...
					// That means the initialization of the game should do some warm up and fetch all leaves,
					// otherwise it's gonna be rough
				}

				if (node->block) {
					destroy_action(node, node_pos, lod);
					node->block = T();
				}
			}

		} else {

			bool has_split_child = false;
			unsigned int first_child = node->first_child;

			for (unsigned int i = 0; i < 8; ++i) {
				unsigned int child_index = first_child + i;
				update(child_index, get_child_position(node_pos, i), lod - 1, view_pos, create_action, destroy_action);
				has_split_child |= _pool.get_node(child_index)->has_children();
			}

			// Get node again because `update` may invalidate the pointer
			node = get_node(node_index);

			if (!has_split_child && world_center.distance_to(view_pos) > split_distance && destroy_action.can_do(node, node_pos, lod)) {
				// Join
				if (node->has_children()) {

					for (unsigned int i = 0; i < 8; ++i) {
						Node *child = _pool.get_node(first_child + i);
						destroy_action(child, get_child_position(node_pos, i), lod - 1);
						child->block = T();
					}

					_pool.recycle_children(first_child);
					node->first_child = NO_CHILDREN;

					// If this is true, means the parent wasn't properly split.
					// When subdividing a node, that node's block must be destroyed as it is replaced by its children.
					CRASH_COND(node->block);

					node->block = create_action(node, node_pos, lod);
				}
			}
		}
	}

	template <typename A>
	void join_all_recursively(Node *node, Vector3i node_pos, int lod, A &destroy_action) {
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

		} else if (node->block) {
			destroy_action(node, node_pos, lod);
			node->block = T();
		}
	}

	Node _root;
	int _max_depth = 0;
	float _base_size = 16;
	float _split_scale = 2.0;
	// TODO May be worth making this pool external for sharing purpose
	NodePool _pool;
};

// Notes:
// Population of an octree given its depth, thanks to Sage:
// ((1 << 3 * (depth + 1)) - 1 ) / 7

#endif // LOD_OCTREE_H
