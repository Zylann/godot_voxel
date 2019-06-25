#ifndef LOD_OCTREE_H
#define LOD_OCTREE_H

#include "../math/vector3i.h"
#include "../octree_tables.h"
#include "../util/object_pool.h"

template <class T>
class LodOctree {
public:
	struct Node {
		Node *children[8];
		// Whatever data to associate to the node, when it's a leaf.
		// It needs to be booleanizable, where `true` means presence of data, and `false` means no data.
		// Typically a pointer, but can be a straight boolean too.
		T block;
		// Position divided by chunk size at the current LOD,
		// so it is sequential within each LOD, which makes it usable for grid storage
		Vector3i position;

		Node() {
			init();
		}

		inline bool has_children() const {
			return children[0] != nullptr;
		}

		inline void init() {
			position = Vector3i();
			block = T();
			children[0] = nullptr;
			//			for (int i = 0; i < 8; ++i) {
			//				children[i] = nullptr;
			//			}
		}
	};

	struct NoDestroyAction {
		inline void operator()(Node *node, int lod) {}
	};

	template <typename A>
	void clear(A &destroy_action) {
		join_all_recursively(&_root, _max_depth, destroy_action);
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
		ERR_FAIL_COND(lod_count > 32);
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
		update(&_root, _max_depth, view_pos, create_action, destroy_action);
	}

	template <typename A>
	void foreach_node(A &action) {
		action(action, &_root, _max_depth);
	}

	static inline Vector3i get_child_position(Vector3i parent_position, int i) {
		return Vector3i(
				parent_position.x * 2 + OctreeTables::g_octant_position[i][0],
				parent_position.y * 2 + OctreeTables::g_octant_position[i][1],
				parent_position.z * 2 + OctreeTables::g_octant_position[i][2]);
	}

private:
	template <typename A>
	void foreach_node(A action, Node *node, int lod) {
		action(node, lod);
		if (node->has_children()) {
			for (int i = 0; i < 8; ++i) {
				foreach_node(action, node->children[i], lod - 1);
			}
		}
	}

	template <typename A, typename B>
	void update(Node *node, int lod, Vector3 view_pos, A &create_action, B &destroy_action) {
		// This function should be called regularly over frames.

		int lod_factor = get_lod_factor(lod);
		int chunk_size = _base_size * lod_factor;
		Vector3 world_center = static_cast<real_t>(chunk_size) * (node->position.to_vec3() + Vector3(0.5, 0.5, 0.5));
		float split_distance = chunk_size * _split_scale;

		if (!node->has_children()) {

			// If it's not the last LOD, if close enough and custom conditions get fulfilled
			if (lod > 0 && world_center.distance_to(view_pos) < split_distance && create_action.can_do(node, lod)) {
				// Split
				for (int i = 0; i < 8; ++i) {

					Node *child = _pool.create();

					child->position = get_child_position(node->position, i);
					child->block = create_action(child, lod - 1);

					node->children[i] = child;
					// If the node needs to split more, we'll ask more recycling at the next frame...
					// That means the initialization of the game should do some warm up and fetch all leaves,
					// otherwise it's gonna be rough
				}

				if (node->block) {
					destroy_action(node, lod);
					node->block = T();
				}
			}

		} else {

			bool no_split_child = true;

			for (int i = 0; i < 8; ++i) {
				Node *child = node->children[i];
				update(child, lod - 1, view_pos, create_action, destroy_action);
				no_split_child |= child->has_children();
			}

			if (no_split_child && world_center.distance_to(view_pos) > split_distance && destroy_action.can_do(node, lod)) {
				// Join
				if (node->has_children()) {

					for (int i = 0; i < 8; ++i) {
						Node *child = node->children[i];
						destroy_action(child, lod - 1);
						child->block = T();
						_pool.recycle(child);
					}

					node->children[0] = nullptr;

					CRASH_COND(node->block);
					node->block = create_action(node, lod);
				}
			}
		}
	}

	template <typename A>
	void join_all_recursively(Node *node, int lod, A &destroy_action) {

		if (node->has_children()) {
			Node **children = node->children;

			for (int i = 0; i < 8; ++i) {
				Node *child = children[i];
				join_all_recursively(child, lod - 1, destroy_action);
				_pool.recycle(child);
				children[0] = nullptr;
			}

		} else {
			if (node->block) {
				destroy_action(node, lod);
				node->block = T();
			}
		}
	}

	Node _root;
	int _max_depth = 0;
	float _base_size = 16;
	float _split_scale = 2.0;
	ObjectPool<Node> _pool;
};

// Notes:
// Population of an octree given its depth, thanks to Sage:
// ((1 << 3 * (depth + 1)) - 1 ) / 7

#endif // LOD_OCTREE_H
