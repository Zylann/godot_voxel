#ifndef ZYLANN_MACROS_H
#define ZYLANN_MACROS_H

// Macros I couldn't put anywhere specific

// TODO Waiting for a fix, Godot's Variant() can't be constructed from `size_t` on JavaScript and OSX builds.
// See https://github.com/godotengine/godot/issues/36690
#define ZN_SIZE_T_TO_VARIANT(s) static_cast<int64_t>(s)

#define ZN_ARRAY_LENGTH(a) (sizeof(a) / sizeof(a[0]))

#ifdef ZN_GODOT_EXTENSION
// TODO GDX: Resource::duplicate() cannot be overriden (while it can in modules). This will lead to performance
// degradation and maybe unexpected behavior!
#define ZN_OVERRIDE_UNLESS_GODOT_EXTENSION
#else
#define ZN_OVERRIDE_UNLESS_GODOT_EXTENSION override
#endif

// Must be used in global space.
#if defined(ZN_GODOT)
#define ZN_GODOT_FORWARD_DECLARE(m_class) m_class;
#elif defined(ZN_GODOT_EXTENSION)
#define ZN_GODOT_FORWARD_DECLARE(m_class)                                                                              \
	namespace godot {                                                                                                  \
	m_class;                                                                                                           \
	}
#endif

// Tell the compiler to favour a certain branch of a condition.
// Until C++20 can be used with the [[likely]] and [[unlikely]] attributes.
#if defined(__GNUC__)
#define ZN_LIKELY(x) __builtin_expect(!!(x), 1)
#define ZN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define ZN_LIKELY(x) x
#define ZN_UNLIKELY(x) x
#endif

// Must be used in global space.
#if defined(ZN_GODOT)
#define ZN_GODOT_NAMESPACE_BEGIN
#define ZN_GODOT_NAMESPACE_END
#elif defined(ZN_GODOT_EXTENSION)
#define ZN_GODOT_NAMESPACE_BEGIN namespace godot {
#define ZN_GODOT_NAMESPACE_END }
#endif

#endif // ZYLANN_MACROS_H
