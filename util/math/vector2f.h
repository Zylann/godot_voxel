#ifndef ZYLANN_VECTOR2F_H
#define ZYLANN_VECTOR2F_H

#include "../errors.h"
#include "vector2t.h"

namespace zylann {

// 32-bit float precision 2D vector.
// Because Godot's `Vector2` uses `real_t`, so when `real_t` is `double` it forces some things to use double-precision
// vectors while they dont need that amount of precision.
typedef Vector2T<float> Vector2f;

} // namespace zylann

#endif // ZYLANN_VECTOR2F_H
