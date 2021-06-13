#ifndef BOX3I_H
#define BOX3I_H

#include "vector3i.h"
#include <core/variant.h>

// Axis-aligned box using integer coordinates
class Box3i {
public:
	Vector3i pos;
	Vector3i size;

	Box3i() {}

	Box3i(Vector3i p_pos, Vector3i p_size) :
			pos(p_pos),
			size(p_size) {}

	Box3i(int ox, int oy, int oz, int sx, int sy, int sz) :
			pos(ox, oy, oz),
			size(sx, sy, sz) {}

	Box3i(const Box3i &other) :
			pos(other.pos),
			size(other.size) {}

	static inline Box3i from_center_extents(Vector3i center, Vector3i extents) {
		return Box3i(center - extents, 2 * extents);
	}

	// max is exclusive
	static inline Box3i from_min_max(Vector3i p_min, Vector3i p_max) {
		return Box3i(p_min, p_max - p_min);
	}

	static inline Box3i get_bounding_box(Box3i a, Box3i b) {
		Box3i box;

		box.pos.x = MIN(a.pos.x, b.pos.x);
		box.pos.y = MIN(a.pos.y, b.pos.y);
		box.pos.z = MIN(a.pos.z, b.pos.z);

		Vector3i max_a = a.pos + a.size;
		Vector3i max_b = b.pos + b.size;

		box.size.x = MAX(max_a.x, max_b.x) - box.pos.x;
		box.size.y = MAX(max_a.y, max_b.y) - box.pos.y;
		box.size.z = MAX(max_a.z, max_b.z) - box.pos.z;

		return box;
	}

	bool inline contains(Vector3i p_pos) const {
		const Vector3i end = pos + size;
		return p_pos.x >= pos.x &&
			   p_pos.y >= pos.y &&
			   p_pos.z >= pos.z &&
			   p_pos.x < end.x &&
			   p_pos.y < end.y &&
			   p_pos.z < end.z;
	}

	bool inline contains(const Box3i other) const {
		const Vector3i other_end = other.pos + other.size;
		const Vector3i end = pos + size;
		return other.pos.x >= pos.x &&
			   other.pos.y >= pos.y &&
			   other.pos.z >= pos.z &&
			   other_end.x <= end.x &&
			   other_end.y <= end.y &&
			   other_end.z <= end.z;
	}

	String to_string() const {
		return String("(o:{0}, s:{1})").format(varray(pos.to_vec3(), size.to_vec3()));
	}

	bool intersects(Box3i other) const {
		if (pos.x > other.pos.x + other.size.x) {
			return false;
		}
		if (pos.y > other.pos.y + other.size.y) {
			return false;
		}
		if (pos.z > other.pos.z + other.size.z) {
			return false;
		}
		if (other.pos.x > pos.x + size.x) {
			return false;
		}
		if (other.pos.y > pos.y + size.y) {
			return false;
		}
		if (other.pos.z > pos.z + size.z) {
			return false;
		}
		return true;
	}

	struct NoAction {
		inline void operator()(const Vector3i pos) {}
	};

	template <typename A>
	inline void for_each_cell(A a) const {
		Vector3i max = pos + size;
		Vector3i p;
		for (p.z = pos.z; p.z < max.z; ++p.z) {
			for (p.y = pos.y; p.y < max.y; ++p.y) {
				for (p.x = pos.x; p.x < max.x; ++p.x) {
					a(p);
				}
			}
		}
	}

	template <typename A>
	inline void for_each_cell_zxy(A a) const {
		Vector3i max = pos + size;
		Vector3i p;
		for (p.z = pos.z; p.z < max.z; ++p.z) {
			for (p.x = pos.x; p.x < max.x; ++p.x) {
				for (p.y = pos.y; p.y < max.y; ++p.y) {
					a(p);
				}
			}
		}
	}

	// Returns true if all cells of the box comply with the given predicate on their position.
	template <typename A>
	inline bool all_cells_match(A a) const {
		Vector3i max = pos + size;
		Vector3i p;
		for (p.z = pos.z; p.z < max.z; ++p.z) {
			for (p.y = pos.y; p.y < max.y; ++p.y) {
				for (p.x = pos.x; p.x < max.x; ++p.x) {
					if (!a(p)) {
						return false;
					}
				}
			}
		}
		return true;
	}

	// Subtracts another box from the current box,
	// then execute a function on a set of boxes representing the remaining area.
	//
	// For example, seen from 2D, a possible result would be:
	//
	// o-----------o                 o-----o-----o
	// | A         |                 | C1  | C2  |
	// |     o-----+---o             |     o-----o
	// |     |     |   |   A - B =>  |     |
	// o-----+-----o   |             o-----o
	//       | B       |
	//       o---------o
	//
	template <typename A>
	void difference(const Box3i &b, A action) const {
		if (!intersects(b)) {
			action(*this);
			return;
		}

		Box3i a = *this;

		Vector3i a_min = a.pos;
		Vector3i b_min = b.pos;
		Vector3i a_max = a.pos + a.size;
		Vector3i b_max = b.pos + b.size;

		if (a_min.x < b_min.x) {
			Vector3i a_rect_size(b_min.x - a_min.x, a.size.y, a.size.z);
			action(Box3i(a_min, a_rect_size));
			a_min.x = b_min.x;
			a.pos.x = b.pos.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_min.y < b_min.y) {
			Vector3i a_rect_size(a.size.x, b_min.y - a_min.y, a.size.z);
			action(Box3i(a_min, a_rect_size));
			a_min.y = b_min.y;
			a.pos.y = b.pos.y;
			a.size.y = a_max.y - a_min.y;
		}
		if (a_min.z < b_min.z) {
			Vector3i a_rect_size(a.size.x, a.size.y, b_min.z - a_min.z);
			action(Box3i(a_min, a_rect_size));
			a_min.z = b_min.z;
			a.pos.z = b.pos.z;
			a.size.z = a_max.z - a_min.z;
		}

		if (a_max.x > b_max.x) {
			Vector3i a_rect_pos(b_max.x, a_min.y, a_min.z);
			Vector3i a_rect_size(a_max.x - b_max.x, a.size.y, a.size.z);
			action(Box3i(a_rect_pos, a_rect_size));
			a_max.x = b_max.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_max.y > b_max.y) {
			Vector3i a_rect_pos(a_min.x, b_max.y, a_min.z);
			Vector3i a_rect_size(a.size.x, a_max.y - b_max.y, a.size.z);
			action(Box3i(a_rect_pos, a_rect_size));
			a_max.y = b_max.y;
			a.size.y = a_max.y - a_min.y;
		}
		if (a_max.z > b_max.z) {
			Vector3i a_rect_pos(a_min.x, a_min.y, b_max.z);
			Vector3i a_rect_size(a.size.x, a.size.y, a_max.z - b_max.z);
			action(Box3i(a_rect_pos, a_rect_size));
		}
	}

	// Calls a function on all side cell positions belonging to the box.
	// This function was implemented with no particular order in mind.
	template <typename F>
	void for_inner_outline(F f) const {
		//     o-------o
		//    /|      /|
		//   / |     / |
		//  o--+----o  |
		//  |  o----|--o
		//  | /     | /   y z
		//  |/      |/    |/
		//  o-------o     o---x

		Vector3i min_pos = pos;
		Vector3i max_pos = pos + size;

		// Top and bottom
		for (int z = min_pos.z; z < max_pos.z; ++z) {
			for (int x = min_pos.x; x < max_pos.x; ++x) {
				f(Vector3i(x, min_pos.y, z));
				f(Vector3i(x, max_pos.y - 1, z));
			}
		}

		// Exclude top and bottom cells from the sides we'll iterate next
		++min_pos.y;
		--max_pos.y;

		// Z sides
		for (int x = min_pos.x; x < max_pos.x; ++x) {
			for (int y = min_pos.y; y < max_pos.y; ++y) {
				f(Vector3i(x, y, min_pos.z));
				f(Vector3i(x, y, max_pos.z - 1));
			}
		}

		// Exclude cells belonging to edges of Z sides we did before
		++min_pos.z;
		--max_pos.z;

		// X sides
		for (int z = min_pos.z; z < max_pos.z; ++z) {
			for (int y = min_pos.y; y < max_pos.y; ++y) {
				f(Vector3i(min_pos.x, y, z));
				f(Vector3i(max_pos.x - 1, y, z));
			}
		}
	}

	inline Box3i padded(int m) const {
		return Box3i(
				pos.x - m,
				pos.y - m,
				pos.z - m,
				size.x + 2 * m,
				size.y + 2 * m,
				size.z + 2 * m);
	}

	// Converts the rectangle into a coordinate system of higher step size,
	// rounding outwards of the area covered by the original rectangle if divided coordinates have remainders.
	inline Box3i downscaled(int step_size) const {
		Box3i o;
		o.pos = pos.floordiv(step_size);
		// TODO Is that ceildiv?
		Vector3i max_pos = (pos + size - Vector3i(1)).floordiv(step_size);
		o.size = max_pos - o.pos + Vector3i(1);
		return o;
	}

	// Converts the rectangle into a coordinate system of higher step size,
	// rounding inwards of the area covered by the original rectangle if divided coordinates have remainders.
	// This is such that the result is included in the original rectangle (assuming a common coordinate system).
	// The result can be an empty rectangle.
	inline Box3i downscaled_inner(int step_size) const {
		return Box3i::from_min_max(pos.ceildiv(step_size), (pos + size).floordiv(step_size));
	}

	static inline void clip_range(int &pos, int &size, int lim_pos, int lim_size) {
		int max_pos = pos + size;
		int lim_max_pos = lim_pos + lim_size;

		pos = CLAMP(pos, lim_pos, lim_max_pos);
		max_pos = CLAMP(max_pos, lim_pos, lim_max_pos);

		size = max_pos - pos;
		if (size < 0) {
			size = 0;
		}
	}

	inline void clip(const Box3i lim) {
		clip_range(pos.x, size.x, lim.pos.x, lim.size.x);
		clip_range(pos.y, size.y, lim.pos.y, lim.size.y);
		clip_range(pos.z, size.z, lim.pos.z, lim.size.z);
	}

	inline Box3i clipped(const Box3i lim) const {
		Box3i copy(*this);
		copy.clip(lim);
		return copy;
	}

	inline bool encloses(const Box3i &other) const {
		return pos.x <= other.pos.x &&
			   pos.y <= other.pos.y &&
			   pos.x + size.x >= other.pos.x + other.size.x &&
			   pos.y + size.y >= other.pos.y + other.size.y;
	}

	inline Box3i snapped(int step) const {
		Box3i r = downscaled(step);
		r.pos *= step;
		r.size *= step;
		return r;
	}

	inline bool is_empty() const {
		return size.x <= 0 || size.y <= 0 || size.z <= 0;
	}
};

inline bool operator!=(const Box3i &a, const Box3i &b) {
	return a.pos != b.pos || a.size != b.size;
}

#endif // BOX3I_H
