#ifndef ZYLANN_MACROS_H
#define ZYLANN_MACROS_H

// Macros I couldn't put anywhere specific

// Tell the compiler to favour a certain branch of a condition.
// Until C++20 can be used with the [[likely]] and [[unlikely]] attributes.
#if defined(__GNUC__)
#define ZN_LIKELY(x) __builtin_expect(!!(x), 1)
#define ZN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define ZN_LIKELY(x) x
#define ZN_UNLIKELY(x) x
#endif

#define ZN_INTERNAL_CONCAT(x, y) x##y
// Helper to concatenate macro arguments if one of them is itself a macro like `__LINE__`,
// otherwise doing `x##y` directly would not expand the arguments that are a macro
// https://stackoverflow.com/questions/1597007/creating-c-macro-with-and-line-token-concatenation-with-positioning-macr
#define ZN_CONCAT(x, y) ZN_INTERNAL_CONCAT(x, y)

#endif // ZYLANN_MACROS_H
