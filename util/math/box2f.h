#ifndef ZYLANN_BOX2F_H
#define ZYLANN_BOX2F_H

#include "../containers/small_vector.h"
#include "../string/std_stringstream.h"
#include "funcs.h"
#include "vector2f.h"

namespace zylann {

// Axis-aligned 2D box using float coordinates
class Box2f {
public:
	Vector2f position;
	Vector2f size;

	Box2f() {}

	Box2f(Vector2f p_pos, Vector2f p_size) : position(p_pos), size(p_size) {}

	bool intersects(const Box2f &other) const {
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
	void difference(const Box2f &b, A action) const {
		if (!intersects(b)) {
			action(*this);
			return;
		}

		Box2f a = *this;

		Vector2f a_min = a.position;
		Vector2f a_max = a.position + a.size;

		const Vector2f b_min = b.position;
		const Vector2f b_max = b.position + b.size;

		if (a_min.x < b_min.x) {
			const Vector2f a_rect_size(b_min.x - a_min.x, a.size.y);
			action(Box2f(a_min, a_rect_size));
			a_min.x = b_min.x;
			a.position.x = b.position.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_min.y < b_min.y) {
			const Vector2f a_rect_size(a.size.x, b_min.y - a_min.y);
			action(Box2f(a_min, a_rect_size));
			a_min.y = b_min.y;
			a.position.y = b.position.y;
			a.size.y = a_max.y - a_min.y;
		}

		if (a_max.x > b_max.x) {
			const Vector2f a_rect_pos(b_max.x, a_min.y);
			const Vector2f a_rect_size(a_max.x - b_max.x, a.size.y);
			action(Box2f(a_rect_pos, a_rect_size));
			a_max.x = b_max.x;
			a.size.x = a_max.x - a_min.x;
		}
		if (a_max.y > b_max.y) {
			const Vector2f a_rect_pos(a_min.x, b_max.y);
			const Vector2f a_rect_size(a.size.x, a_max.y - b_max.y);
			action(Box2f(a_rect_pos, a_rect_size));
		}
	}

	inline void difference_to_vec(const Box2f &b, SmallVector<Box2f, 6> &output) const {
		difference(b, [&output](const Box2f &sub_box) { output.push_back(sub_box); });
	}
};

StdStringStream &operator<<(StdStringStream &ss, const Box2f &box);

} // namespace zylann

#endif // ZYLANN_Box2i_H
