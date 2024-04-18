#ifndef ZYLANN_BOX3I_H
#define ZYLANN_BOX3I_H

#include "../containers/small_vector.h"
#include "../containers/std_vector.h"
#include "../string/std_stringstream.h"
#include "vector3i.h"

namespace zylann {

// Axis-aligned 3D box using integer coordinates
class Box3i {
public:
	Vector3i position;
	Vector3i size;

	Box3i() {}

	Box3i(Vector3i p_pos, Vector3i p_size) : position(p_pos), size(p_size) {}

	Box3i(int ox, int oy, int oz, int sx, int sy, int sz) : position(ox, oy, oz), size(sx, sy, sz) {}

	// Creates a box centered on a point, specifying half its size.
	// Warning: if you consider the center being a 1x1x1 box which would be extended, instead of a mathematical point,
	// you may want to add 1 to extents.
	static inline Box3i from_center_extents(Vector3i center, Vector3i extents) {
		return Box3i(center - extents, 2 * extents);
	}

	// max is exclusive
	static inline Box3i from_min_max(Vector3i p_min, Vector3i p_max) {
		return Box3i(p_min, p_max - p_min);
	}

	static inline Box3i get_bounding_box(Box3i a, Box3i b) {
		Box3i box;

		box.position.x = math::min(a.position.x, b.position.x);
		box.position.y = math::min(a.position.y, b.position.y);
		box.position.z = math::min(a.position.z, b.position.z);

		Vector3i max_a = a.position + a.size;
		Vector3i max_b = b.position + b.size;

		box.size.x = math::max(max_a.x, max_b.x) - box.position.x;
		box.size.y = math::max(max_a.y, max_b.y) - box.position.y;
		box.size.z = math::max(max_a.z, max_b.z) - box.position.z;

		return box;
	}

	bool inline contains(Vector3i p_pos) const {
		const Vector3i end = position + size;
		return p_pos.x >= position.x && //
				p_pos.y >= position.y && //
				p_pos.z >= position.z && //
				p_pos.x < end.x && //
				p_pos.y < end.y && //
				p_pos.z < end.z;
	}

	bool inline contains(const Box3i other) const {
		const Vector3i other_end = other.position + other.size;
		const Vector3i end = position + size;
		return other.position.x >= position.x && //
				other.position.y >= position.y && //
				other.position.z >= position.z && //
				other_end.x <= end.x && //
				other_end.y <= end.y && //
				other_end.z <= end.z;
	}

	bool intersects(const Box3i &other) const {
		if (position.x >= other.position.x + other.size.x) {
			return false;
		}
		if (position.y >= other.position.y + other.size.y) {
			return false;
		}
		if (position.z >= other.position.z + other.size.z) {
			return false;
		}
		if (other.position.x >= position.x + size.x) {
			return false;
		}
		if (other.position.y >= position.y + size.y) {
			return false;
		}
		if (other.position.z >= position.z + size.z) {
			return false;
		}
		return true;
	}

	struct NoAction {
		inline void operator()(const Vector3i pos) {}
	};

	// Iteration is done in ZYX order.
	template <typename A>
	inline void for_each_cell(A action) const {
		const Vector3i max = position + size;
		Vector3i p;
		for (p.z = position.z; p.z < max.z; ++p.z) {
			for (p.y = position.y; p.y < max.y; ++p.y) {
				for (p.x = position.x; p.x < max.x; ++p.x) {
					action(p);
				}
			}
		}
	}

	// Iteration is done in ZXY order.
	template <typename A>
	inline void for_each_cell_zxy(A action) const {
		const Vector3i max = position + size;
		Vector3i p;
		for (p.z = position.z; p.z < max.z; ++p.z) {
			for (p.x = position.x; p.x < max.x; ++p.x) {
				for (p.y = position.y; p.y < max.y; ++p.y) {
					action(p);
				}
			}
		}
	}

	// Returns true if all cells of the box comply with the given predicate on their position.
	// Iteration is done in ZYX order.
	template <typename A>
	inline bool all_cells_match(A predicate) const {
		const Vector3i max = position + size;
		Vector3i p;
		for (p.z = position.z; p.z < max.z; ++p.z) {
			for (p.y = position.y; p.y < max.y; ++p.y) {
				for (p.x = position.x; p.x < max.x; ++p.x) {
					if (!predicate(p)) {
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

		Vector3i a_min = a.position;
		Vector3i a_max = a.position + a.size;

		const Vector3i b_min = b.position;
		const Vector3i b_max = b.position + b.size;

		if (a_min.x < b_min.x) {
			const Vector3i a_rect_size(b_min.x - a_min.x, a.size.y, a.size.z);
			action(Box3i(a_min, a_rect_size));
			a_min.x = b_min.x;
			a.position.x = b.position.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_min.y < b_min.y) {
			const Vector3i a_rect_size(a.size.x, b_min.y - a_min.y, a.size.z);
			action(Box3i(a_min, a_rect_size));
			a_min.y = b_min.y;
			a.position.y = b.position.y;
			a.size.y = a_max.y - a_min.y;
		}
		if (a_min.z < b_min.z) {
			const Vector3i a_rect_size(a.size.x, a.size.y, b_min.z - a_min.z);
			action(Box3i(a_min, a_rect_size));
			a_min.z = b_min.z;
			a.position.z = b.position.z;
			a.size.z = a_max.z - a_min.z;
		}

		if (a_max.x > b_max.x) {
			const Vector3i a_rect_pos(b_max.x, a_min.y, a_min.z);
			const Vector3i a_rect_size(a_max.x - b_max.x, a.size.y, a.size.z);
			action(Box3i(a_rect_pos, a_rect_size));
			a_max.x = b_max.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_max.y > b_max.y) {
			const Vector3i a_rect_pos(a_min.x, b_max.y, a_min.z);
			const Vector3i a_rect_size(a.size.x, a_max.y - b_max.y, a.size.z);
			action(Box3i(a_rect_pos, a_rect_size));
			a_max.y = b_max.y;
			a.size.y = a_max.y - a_min.y;
		}
		if (a_max.z > b_max.z) {
			const Vector3i a_rect_pos(a_min.x, a_min.y, b_max.z);
			const Vector3i a_rect_size(a.size.x, a.size.y, a_max.z - b_max.z);
			action(Box3i(a_rect_pos, a_rect_size));
		}
	}

	// Subtracts another box from the current box.
	// If any, boxes composing the remaining volume are added to the given vector.
	inline void difference_to_vec(const Box3i &b, StdVector<Box3i> &output) const {
		difference(b, [&output](const Box3i &sub_box) { output.push_back(sub_box); });
	}

	inline void difference_to_vec(const Box3i &b, SmallVector<Box3i, 6> &output) const {
		difference(b, [&output](const Box3i &sub_box) { output.push_back(sub_box); });
	}

	// Calls a function on all side cell positions belonging to the box.
	// Cells don't follow a particular order and may not be relied on.
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

		Vector3i min_pos = position;
		Vector3i max_pos = position + size;

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
		return Box3i(position.x - m, position.y - m, position.z - m, size.x + 2 * m, size.y + 2 * m, size.z + 2 * m);
	}

	// Converts the rectangle into a coordinate system of higher step size,
	// rounding outwards of the area covered by the original rectangle if divided coordinates have remainders.
	inline Box3i downscaled(int step_size) const {
		Box3i o;
		o.position = math::floordiv(position, step_size);
		// TODO Is that ceildiv?
		Vector3i max_pos = math::floordiv(position + size - Vector3i(1, 1, 1), step_size);
		o.size = max_pos - o.position + Vector3i(1, 1, 1);
		return o;
	}

	// Converts the rectangle into a coordinate system of higher step size,
	// rounding inwards of the area covered by the original rectangle if divided coordinates have remainders.
	// This is such that the result is included in the original rectangle (assuming a common coordinate system).
	// The result can be an empty rectangle.
	inline Box3i downscaled_inner(int step_size) const {
		return Box3i::from_min_max(math::ceildiv(position, step_size), math::floordiv(position + size, step_size));
	}

	inline Box3i scaled(int scale) const {
		return Box3i(position * scale, size * scale);
	}

	static inline void clip_range(int &pos, int &size, int lim_pos, int lim_size) {
		int max_pos = pos + size;
		int lim_max_pos = lim_pos + lim_size;

		pos = math::clamp(pos, lim_pos, lim_max_pos);
		max_pos = math::clamp(max_pos, lim_pos, lim_max_pos);

		size = max_pos - pos;
		if (size < 0) {
			size = 0;
		}
	}

	inline void clip(const Box3i lim) {
		clip_range(position.x, size.x, lim.position.x, lim.size.x);
		clip_range(position.y, size.y, lim.position.y, lim.size.y);
		clip_range(position.z, size.z, lim.position.z, lim.size.z);
	}

	inline void clip(const Vector3i &lim_size) {
		clip_range(position.x, size.x, 0, lim_size.x);
		clip_range(position.y, size.y, 0, lim_size.y);
		clip_range(position.z, size.z, 0, lim_size.z);
	}

	inline Box3i clipped(const Box3i lim) const {
		Box3i copy(*this);
		copy.clip(lim);
		return copy;
	}

	inline Box3i clipped(const Vector3i &lim_size) const {
		Box3i copy(*this);
		copy.clip(lim_size);
		return copy;
	}

	inline bool encloses(const Box3i &other) const {
		return position.x <= other.position.x && //
				position.y <= other.position.y && //
				position.z <= other.position.z && //
				position.x + size.x >= other.position.x + other.size.x && //
				position.y + size.y >= other.position.y + other.size.y && //
				position.z + size.z >= other.position.z + other.size.z;
	}

	inline Box3i snapped(int step) const {
		Box3i r = downscaled(step);
		r.position *= step;
		r.size *= step;
		return r;
	}

	inline bool is_empty() const {
		return size.x <= 0 || size.y <= 0 || size.z <= 0;
	}

	void merge_with(const Box3i &other) {
		const Vector3i min_pos( //
				math::min(position.x, other.position.x), //
				math::min(position.y, other.position.y), //
				math::min(position.z, other.position.z));
		const Vector3i max_pos( //
				math::max(position.x + size.x, other.position.x + other.size.x), //
				math::max(position.y + size.y, other.position.y + other.size.y), //
				math::max(position.z + size.z, other.position.z + other.size.z));
		position = min_pos;
		size = max_pos - min_pos;
	}
};

inline bool operator!=(const Box3i &a, const Box3i &b) {
	return a.position != b.position || a.size != b.size;
}

inline bool operator==(const Box3i &a, const Box3i &b) {
	return a.position == b.position && a.size == b.size;
}

StdStringStream &operator<<(StdStringStream &ss, const Box3i &box);

} // namespace zylann

#endif // ZYLANN_BOX3I_H
