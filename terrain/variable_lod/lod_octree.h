#ifndef VOXEL_LOD_OCTREE_H
#define VOXEL_LOD_OCTREE_H

#include "../../util/math/box3i.h"

namespace zylann::voxel {

// Octree designed to handle level of detail.
class LodOctree {
public:
	static const unsigned int NO_CHILDREN = -1;
	static const unsigned int ROOT_INDEX = -1; // Root node isn't stored in pool

	struct NodeData {
		uint32_t state = 0;
	};

	struct Node {
		// Index to first child node within the node pool.
		// The 7 next indexes are the other children.
		// If the node isn't subdivided, it is set to NO_CHILDREN.
		// Could have used a pointer but an index is enough, occupies half memory and is immune to realloc
		unsigned int first_child;

		NodeData data;

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
		inline void operator()(Vector3i node_pos, unsigned int lod) {}
	};

	template <typename DestroyAction_T>
	void clear(DestroyAction_T &destroy_action) {
		join_all_recursively(&_root, Vector3i(), _max_depth, destroy_action);
		_is_root_created = false;
		_max_depth = 0;
	}

	void clear() {
		_pool.clear();
		_root.init();
		_is_root_created = false;
		_max_depth = 0;
	}

	template <typename DestroyAction_T>
	void create(unsigned int lod_count, DestroyAction_T &destroy_action) {
		clear(destroy_action);
		CRASH_COND(lod_count == 0);
		_max_depth = lod_count - 1;
	}

	void create(unsigned int lod_count) {
		clear();
		CRASH_COND(lod_count == 0);
		_max_depth = lod_count - 1;
	}

	unsigned int get_lod_count() const {
		return _max_depth + 1;
	}

	// Signature examples
	struct DefaultUpdateActions {
		void create_child(Vector3i node_pos, unsigned int lod, NodeData &data) {} // Occurs on split
		void destroy_child(Vector3i node_pos, unsigned int lod) {} // Occurs on merge
		void show_parent(Vector3i node_pos, unsigned int lod) {} // Occurs on merge
		void hide_parent(Vector3i node_pos, unsigned int lod) {} // Occurs on split
		bool can_create_root(int lod) {
			return true;
		}
		bool can_split(Vector3i node_pos, unsigned int lod_index, NodeData &data) {
			return true;
		}
		bool can_join(Vector3i node_pos, unsigned int lod) {
			return true;
		}
	};

	// TODO Have a version of `update` that works fully recursively.
	// i.e one call should be enough to obtain the target shape

	// Fits the octree by splitting nodes if they satisfy the `can_split` predicate,
	// and joining nodes if they satisfy the `can_join` predicate.
	// This is not fully recursive. It is expected to be called over several frames,
	// so the shape is obtained progressively.
	template <typename UpdateActions_T>
	void update(UpdateActions_T &actions) {
		if (_is_root_created || _root.has_children()) {
			update(ROOT_INDEX, Vector3i(), _max_depth, actions);
		} else {
			// TODO I don't like this much
			// Treat the root in a slightly different way the first time.
			if (actions.can_create_root(_max_depth)) {
				actions.create_child(Vector3i(), _max_depth, _root.data);
				_is_root_created = true;

				update(ROOT_INDEX, Vector3i(), _max_depth, actions);
			}
		}
	}

	static inline Vector3i get_child_position(Vector3i parent_position, unsigned int i) {
		return Vector3i( //
				parent_position.x * 2 + (i & 1), //
				parent_position.y * 2 + ((i >> 1) & 1), //
				parent_position.z * 2 + ((i >> 2) & 1));
	}

	const Node *get_root() const {
		return &_root;
	}

	const Node *get_child(const Node *node, unsigned int i) const {
		ERR_FAIL_COND_V(node == nullptr, nullptr);
		ERR_FAIL_INDEX_V(i, 8, nullptr);
		return get_node(node->first_child + i);
	}

	// Runs a predicate on all leaf nodes intersecting the box, and stops as soon as it is true.
	// The box is given in unit coordinates of the octree (1 unit is the size of a leaf node at maximum depth).
	// Returns true if the predicate matches any node, false otherwise.
	// predicate: `bool is_match(Vector3i node_pos, int lod_index, const NodeData &data)`
	template <typename Predicate_T>
	bool find_in_box(Box3i box, Predicate_T predicate) const {
		Box3i root_box(Vector3i(), Vector3iUtil::create(1 << _max_depth));
		box.clip(root_box);
		return find_in_box_recursive(box, Vector3i(), ROOT_INDEX, _max_depth, predicate);
	}

	// Executes a function on all leaf nodes intersecting the box.
	// f: `void f(Vector3i node_pos, int lod_index, NodeData &data)`
	template <typename F>
	void for_leaves_in_box(Box3i box, F f) {
		Box3i root_box(Vector3i(), Vector3iUtil::create(1 << _max_depth));
		box.clip(root_box);
		return for_leaves_in_box_recursive(box, Vector3i(), ROOT_INDEX, _max_depth, f);
	}

	// Executes a function on all leaf nodes of the octree.
	// f: `void f(Vector3i node_pos, int lod_index, const NodeData &data)`
	template <typename F>
	void for_each_leaf(F f) const {
		return for_each_leaf_recursive(Vector3i(), ROOT_INDEX, _max_depth, f);
	}

	unsigned int get_node_count() const {
		return get_node_count_recursive(ROOT_INDEX);
	}

	struct SubdivideActionsDefault {
		bool can_split(Vector3i node_pos, unsigned int lod_index, const NodeData &node_data) {
			return true;
		}
		void create_child(Vector3i node_pos, unsigned int lod_index, NodeData &node_data) {}
	};

	// Subdivides the octree recursively, solely based on `can_split`.
	// Does not unsubdivide existing nodes.
	template <typename Actions_T>
	void subdivide(Actions_T &actions) {
		if (!_is_root_created && actions.can_split(Vector3i(), _max_depth, _root.data)) {
			actions.create_child(Vector3i(), _max_depth, _root.data);
			_is_root_created = true;
		} else {
			return;
		}
		subdivide_recursively(ROOT_INDEX, Vector3i(), _max_depth, actions);
	}

	// Gets the bounding box of a node within the LOD0 coordinate system
	// (i.e a leaf node will always be 1x1x1, a LOD1 node will be 2x2x2 etc)
	static inline Box3i get_node_box(Vector3i pos_within_lod, unsigned int lod_index) {
		return Box3i(pos_within_lod << lod_index, Vector3iUtil::create(1 << lod_index));
	}

	// Convenience for use in UpdateActions::can_split.
	// Coordinates are in octree space (where 1 unit = size of a leaf node)
	static bool is_below_split_distance(Vector3i node_pos, unsigned int lod, Vector3 view_pos, float lod_distance) {
		const unsigned int lod_factor = 1 << lod;
		const Vector3 world_center = static_cast<real_t>(lod_factor) * (Vector3(node_pos) + Vector3(0.5, 0.5, 0.5));
		const float split_distance_sq = math::squared(lod_distance * lod_factor);
		return world_center.distance_squared_to(view_pos) < split_distance_sq;
	}

	// Helper for creating an octree with the right depth
	static int compute_lod_count(unsigned int base_size, unsigned int full_size) {
		unsigned int po = 0;
		while (full_size > base_size) {
			full_size = full_size >> 1;
			po += 1;
		}
		return po;
	}

	static inline unsigned int get_octree_size_po2(unsigned int block_size_po2, unsigned int lod_count) {
		return block_size_po2 + lod_count - 1;
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

		void clear() {
			_nodes.clear();
			_free_indexes.clear();
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
	void update(unsigned int node_index, Vector3i node_pos, unsigned int lod, UpdateActions_T &actions) {
		// This function should be called regularly over frames.
		Node *node = get_node(node_index);

		if (!node->has_children()) {
			// If it's not the last LOD, if close enough and custom conditions get fulfilled
			if (lod > 0 && actions.can_split(node_pos, lod, node->data)) {
				// Split
				const unsigned int first_child = _pool.allocate_children();
				// Get node again because `allocate_children` may invalidate the pointer
				node = get_node(node_index);
				node->first_child = first_child;

				for (unsigned int i = 0; i < 8; ++i) {
					const Vector3i child_pos = get_child_position(node_pos, i);
					const unsigned int child_lod = lod - 1;
					const unsigned int child_index = first_child + i;

					Node *child = get_node(child_index);
					actions.create_child(child_pos, child_lod, child->data);

					update(child_index, child_pos, child_lod, actions);
				}

				actions.hide_parent(node_pos, lod);
			}

		} else {
			// `node` has children

			bool has_split_child = false;
			const unsigned int first_child = node->first_child;

			for (unsigned int i = 0; i < 8; ++i) {
				const unsigned int child_index = first_child + i;
				update(child_index, get_child_position(node_pos, i), lod - 1, actions);
				has_split_child |= _pool.get_node(child_index)->has_children();
			}

			if (!has_split_child && actions.can_join(node_pos, lod)) {
				// Get node again because `update` may invalidate the pointer
				node = get_node(node_index);

				// Join
				for (unsigned int i = 0; i < 8; ++i) {
					actions.destroy_child(get_child_position(node_pos, i), lod - 1);
				}

				_pool.recycle_children(first_child);
				node->first_child = NO_CHILDREN;

				actions.show_parent(node_pos, lod);
			}
		}
	}

	template <typename DestroyAction_T>
	void join_all_recursively(Node *node, Vector3i node_pos, unsigned int lod, DestroyAction_T &destroy_action) {
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
		}

		// Destroy self
		destroy_action(node_pos, lod);
	}

	template <typename Predicate_T>
	bool find_in_box_recursive(
			Box3i box, Vector3i node_pos, unsigned int node_index, unsigned int depth, Predicate_T predicate) const {
		const Node *node = get_node(node_index);
		const Box3i node_box = get_node_box(node_pos, depth);
		if (!node_box.intersects(box)) {
			return false;
		}
		if (node->has_children()) {
			const unsigned int first_child_index = node->first_child;
			const unsigned int lower_depth = depth - 1;
			// TODO Optimization: we could do breadth-first search instead of depth-first,
			// because packs of children are contiguous in memory and would help the pre-fetcher
			for (unsigned int ri = 0; ri < 8; ++ri) {
				const bool found = find_in_box_recursive(
						box, get_child_position(node_pos, ri), first_child_index + ri, lower_depth, predicate);
				if (found) {
					return true;
				}
			}
		} else if (predicate(node_pos, depth, node->data)) {
			return true;
		}
		return false;
	}

	template <typename F>
	void for_leaves_in_box_recursive(Box3i box, Vector3i node_pos, unsigned int node_index, unsigned int depth, F f) {
		Node *node = get_node(node_index);
		const Box3i node_box = get_node_box(node_pos, depth);
		if (!node_box.intersects(box)) {
			return;
		}
		if (node->has_children()) {
			const unsigned int first_child_index = node->first_child;
			const unsigned int lower_depth = depth - 1;
			for (int ri = 0; ri < 8; ++ri) {
				for_leaves_in_box_recursive(
						box, get_child_position(node_pos, ri), first_child_index + ri, lower_depth, f);
			}
		} else {
			f(node_pos, depth, node->data);
		}
	}

	unsigned int get_node_count_recursive(unsigned int node_index) const {
		const Node *node = get_node(node_index);
		unsigned int count = 1;
		if (node->has_children()) {
			for (unsigned int i = 0; i < 8; ++i) {
				count += get_node_count_recursive(node->first_child + i);
			}
		}
		return count;
	}

	template <typename F>
	void for_each_leaf_recursive(Vector3i node_pos, unsigned int node_index, unsigned int depth, F f) const {
		const Node *node = get_node(node_index);
		if (node->has_children()) {
			const unsigned int first_child_index = node->first_child;
			const unsigned int lower_depth = depth - 1;
			for (unsigned int ri = 0; ri < 8; ++ri) {
				for_each_leaf_recursive(get_child_position(node_pos, ri), first_child_index + ri, lower_depth, f);
			}
		} else {
			f(node_pos, depth, node->data);
		}
	}

	template <typename Actions_T>
	void subdivide_recursively(unsigned int node_index, Vector3i node_pos, unsigned int lod, Actions_T &actions) {
		Node *node = get_node(node_index);
		if (node->has_children()) {
			if (lod == 1) {
				// Children can't split
				return;
			}
			// `node` might be invalidated during the loop
			const unsigned int first_child_index = node->first_child;
			for (unsigned int i = 0; i < 8; ++i) {
				subdivide_recursively(first_child_index + i, get_child_position(node_pos, i), lod - 1, actions);
			}

		} else if (lod > 0 && actions.can_split(node_pos, lod, node->data)) {
			// Split
			const unsigned int first_child_index = _pool.allocate_children();
			// Get node again because `allocate_children` may invalidate the pointer
			node = get_node(node_index);
			node->first_child = first_child_index;
			// `node` might be invalidated during the loop

			for (unsigned int i = 0; i < 8; ++i) {
				const unsigned int child_index = first_child_index + i;
				const Vector3i child_pos = get_child_position(node_pos, i);
				Node *child = get_node(child_index);
				actions.create_child(child_pos, lod - 1, child->data);
				// `child` might be invalidated
				subdivide_recursively(child_index, child_pos, lod - 1, actions);
			}

			// This is where we would call `hide_parent()`, but not needed so far
		}
	}

	Node _root;
	bool _is_root_created = false;
	unsigned int _max_depth = 0;
	NodePool _pool;
};

} // namespace zylann::voxel

// Notes:
// Population of an octree given its depth, thanks to Sage:
// ((1 << 3 * (depth + 1)) - 1 ) / 7

#endif // VOXEL_LOD_OCTREE_H
