#ifndef ZN_BOX_BOUNDS_2I_H
#define ZN_BOX_BOUNDS_2I_H

#include "../std_stringstream.h"
#include "box2i.h"

namespace zylann {

// Alternative implementation of an integer axis-aligned box, storing min and max positions for faster intersection
// checks.
struct BoxBounds2i {
	Vector2i min_pos;
	Vector2i max_pos; // Exclusive

	BoxBounds2i() {}

	BoxBounds2i(Vector2i p_min, Vector2i p_max) : min_pos(p_min), max_pos(p_max) {}

	BoxBounds2i(Box2i box) : min_pos(box.pos), max_pos(box.pos + box.size) {}

	static BoxBounds2i from_position_size(Vector2i pos, Vector2i size) {
		return BoxBounds2i(pos, pos + size);
	}

	static BoxBounds2i from_min_max_included(Vector2i minp, Vector2i maxp) {
		return BoxBounds2i(minp, maxp + Vector2i(1, 1));
	}

	static BoxBounds2i from_position(Vector2i pos) {
		return BoxBounds2i(pos, pos + Vector2i(1, 1));
	}

	static BoxBounds2i from_everywhere() {
		return BoxBounds2i( //
				Vector2iUtil::create(std::numeric_limits<int>::min()),
				Vector2iUtil::create(std::numeric_limits<int>::max()));
	}

	inline bool intersects(const BoxBounds2i &other) const {
		return !( //
				max_pos.x < other.min_pos.x || //
				max_pos.y < other.min_pos.y || //
				min_pos.x > other.max_pos.x || //
				min_pos.y > other.max_pos.y);
	}

	inline bool operator==(const BoxBounds2i &other) const {
		return min_pos == other.min_pos && max_pos == other.max_pos;
	}

	inline bool is_empty() const {
		return min_pos.x >= max_pos.x || min_pos.y >= max_pos.y;
	}

	inline Vector2i get_size() const {
		return max_pos - min_pos;
	}
};

StdStringStream &operator<<(StdStringStream &ss, const BoxBounds2i &box);

} // namespace zylann

#endif // ZN_BOX_BOUNDS_2I_H
