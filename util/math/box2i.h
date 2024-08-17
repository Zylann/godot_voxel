#ifndef ZYLANN_BOX2I_H
#define ZYLANN_BOX2I_H

#include "../containers/std_vector.h"
#include "../string/std_stringstream.h"
#include "funcs.h"
#include "vector2i.h"

namespace zylann {

// Axis-aligned 2D box using integer coordinates
class Box2i {
public:
	Vector2i position;
	Vector2i size;

	Box2i() {}

	Box2i(Vector2i p_pos, Vector2i p_size) : position(p_pos), size(p_size) {}

	Box2i(int ox, int oy, int sx, int sy) : position(ox, oy), size(sx, sy) {}

	// Creates a box centered on a point, specifying half its size.
	// Warning: if you consider the center being a 1x1x1 box which would be extended, instead of a mathematical point,
	// you may want to add 1 to extents.
	static inline Box2i from_center_extents(Vector2i center, Vector2i extents) {
		return Box2i(center - extents, 2 * extents);
	}

	// max is exclusive
	static inline Box2i from_min_max(Vector2i p_min, Vector2i p_max) {
		return Box2i(p_min, p_max - p_min);
	}

	static inline Box2i get_bounding_box(Box2i a, Box2i b) {
		Box2i box;

		box.position.x = math::min(a.position.x, b.position.x);
		box.position.y = math::min(a.position.y, b.position.y);

		Vector2i max_a = a.position + a.size;
		Vector2i max_b = b.position + b.size;

		box.size.x = math::max(max_a.x, max_b.x) - box.position.x;
		box.size.y = math::max(max_a.y, max_b.y) - box.position.y;

		return box;
	}

	bool inline contains(Vector2i p_pos) const {
		const Vector2i end = position + size;
		return p_pos.x >= position.x && //
				p_pos.y >= position.y && //
				p_pos.x < end.x && //
				p_pos.y < end.y;
	}

	bool inline contains(const Box2i other) const {
		const Vector2i other_end = other.position + other.size;
		const Vector2i end = position + size;
		return other.position.x >= position.x && //
				other.position.y >= position.y && //
				other_end.x <= end.x && //
				other_end.y <= end.y;
	}

	bool intersects(const Box2i &other) const {
		if (position.x >= other.position.x + other.size.x) {
			return false;
		}
		if (position.y >= other.position.y + other.size.y) {
			return false;
		}
		if (other.position.x >= position.x + size.x) {
			return false;
		}
		if (other.position.y >= position.y + size.y) {
			return false;
		}
		return true;
	}

	struct NoAction {
		inline void operator()(const Vector2i pos) {}
	};

	// Iteration is done in YX order.
	template <typename A>
	inline void for_each_cell_yx(A action) const {
		const Vector2i max = position + size;
		Vector2i p;
		for (p.y = position.y; p.y < max.y; ++p.y) {
			for (p.x = position.x; p.x < max.x; ++p.x) {
				action(p);
			}
		}
	}

	// Returns true if all cells of the box comply with the given predicate on their position.
	// Iteration is done in YX order.
	template <typename A>
	inline bool all_cells_match(A predicate) const {
		const Vector2i max = position + size;
		Vector2i p;
		for (p.y = position.y; p.y < max.y; ++p.y) {
			for (p.x = position.x; p.x < max.x; ++p.x) {
				if (!predicate(p)) {
					return false;
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
	void difference(const Box2i &b, A action) const {
		if (!intersects(b)) {
			action(*this);
			return;
		}

		Box2i a = *this;

		Vector2i a_min = a.position;
		Vector2i a_max = a.position + a.size;

		const Vector2i b_min = b.position;
		const Vector2i b_max = b.position + b.size;

		if (a_min.x < b_min.x) {
			const Vector2i a_rect_size(b_min.x - a_min.x, a.size.y);
			action(Box2i(a_min, a_rect_size));
			a_min.x = b_min.x;
			a.position.x = b.position.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_min.y < b_min.y) {
			const Vector2i a_rect_size(a.size.x, b_min.y - a_min.y);
			action(Box2i(a_min, a_rect_size));
			a_min.y = b_min.y;
			a.position.y = b.position.y;
			a.size.y = a_max.y - a_min.y;
		}

		if (a_max.x > b_max.x) {
			const Vector2i a_rect_pos(b_max.x, a_min.y);
			const Vector2i a_rect_size(a_max.x - b_max.x, a.size.y);
			action(Box2i(a_rect_pos, a_rect_size));
			a_max.x = b_max.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_max.y > b_max.y) {
			const Vector2i a_rect_pos(a_min.x, b_max.y);
			const Vector2i a_rect_size(a.size.x, a_max.y - b_max.y);
			action(Box2i(a_rect_pos, a_rect_size));
		}
	}

	// Subtracts another box from the current box.
	// If any, boxes composing the remaining volume are added to the given vector.
	inline void difference_to_vec(const Box2i &b, StdVector<Box2i> &output) const {
		difference(b, [&output](const Box2i &sub_box) { output.push_back(sub_box); });
	}

	inline Box2i padded(int m) const {
		return Box2i(position.x - m, position.y - m, size.x + 2 * m, size.y + 2 * m);
	}

	// Converts the rectangle into a coordinate system of higher step size,
	// rounding outwards of the area covered by the original rectangle if divided coordinates have remainders.
	inline Box2i downscaled(int step_size) const {
		Box2i o;
		o.position = math::floordiv(position, step_size);
		// TODO Is that ceildiv?
		Vector2i max_pos = math::floordiv(position + size - Vector2i(1, 1), step_size);
		o.size = max_pos - o.position + Vector2i(1, 1);
		return o;
	}

	// Converts the rectangle into a coordinate system of higher step size,
	// rounding inwards of the area covered by the original rectangle if divided coordinates have remainders.
	// This is such that the result is included in the original rectangle (assuming a common coordinate system).
	// The result can be an empty rectangle.
	inline Box2i downscaled_inner(int step_size) const {
		return Box2i::from_min_max(math::ceildiv(position, step_size), math::floordiv(position + size, step_size));
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

	inline void clip(const Box2i lim) {
		clip_range(position.x, size.x, lim.position.x, lim.size.x);
		clip_range(position.y, size.y, lim.position.y, lim.size.y);
	}

	inline Box2i clipped(const Box2i lim) const {
		Box2i copy(*this);
		copy.clip(lim);
		return copy;
	}

	inline bool encloses(const Box2i &other) const {
		return position.x <= other.position.x && //
				position.y <= other.position.y && //
				position.x + size.x >= other.position.x + other.size.x && //
				position.y + size.y >= other.position.y + other.size.y;
	}

	inline Box2i snapped(int step) const {
		Box2i r = downscaled(step);
		r.position *= step;
		r.size *= step;
		return r;
	}

	inline bool is_empty() const {
		return size.x <= 0 || size.y <= 0;
	}

	void merge_with(const Box2i &other) {
		const Vector2i min_pos( //
				math::min(position.x, other.position.x), //
				math::min(position.y, other.position.y)
		);
		const Vector2i max_pos( //
				math::max(position.x + size.x, other.position.x + other.size.x), //
				math::max(position.y + size.y, other.position.y + other.size.y)
		);
		position = min_pos;
		size = max_pos - min_pos;
	}
};

inline bool operator!=(const Box2i &a, const Box2i &b) {
	return a.position != b.position || a.size != b.size;
}

inline bool operator==(const Box2i &a, const Box2i &b) {
	return a.position == b.position && a.size == b.size;
}

StdStringStream &operator<<(StdStringStream &ss, const Box2i &box);

} // namespace zylann

#endif // ZYLANN_Box2i_H
