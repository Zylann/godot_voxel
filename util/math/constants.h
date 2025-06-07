#ifndef ZN_MATH_CONSTANTS
#define ZN_MATH_CONSTANTS

namespace zylann::math {

// TODO In C++20 we can use <numbers>? (except for TAU unfortunately)

template <typename TFloat>
inline constexpr TFloat TAU = 6.2831853071795864769252867666;

template <typename TFloat>
inline constexpr TFloat INV_TAU = 0.15915494309189533576888376337148;

template <typename TFloat>
inline constexpr TFloat PI = 3.1415926535897932384626433833;

template <typename TFloat>
inline constexpr TFloat SQRT2 = 1.4142135623730950488016887242097;

template <typename TFloat>
inline constexpr TFloat SQRT3 = 1.7320508075688772935274463415059;

enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };

} // namespace zylann::math

#endif // ZN_MATH_CONSTANTS
