#ifndef ZN_MATH_CONSTANTS
#define ZN_MATH_CONSTANTS

namespace zylann::math {

static constexpr float TAU_32 = 6.2831853071795864769252867666;
static constexpr double TAU_64 = 6.2831853071795864769252867666;

static constexpr float PI_32 = 3.1415926535897932384626433833;
static constexpr double PI_64 = 3.1415926535897932384626433833;

static const float INV_TAU_32 = 1.f / math::TAU_32;
static const float SQRT3_32 = 1.73205080757;

enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };

} // namespace zylann::math

#endif // ZN_MATH_CONSTANTS
