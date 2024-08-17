#ifndef ZN_GODOT_MACROS_H
#define ZN_GODOT_MACROS_H

// Must be used in global space.
#if defined(ZN_GODOT)
#define ZN_GODOT_FORWARD_DECLARE(m_class) m_class;
#elif defined(ZN_GODOT_EXTENSION)
#define ZN_GODOT_FORWARD_DECLARE(m_class)                                                                              \
	namespace godot {                                                                                                  \
	m_class;                                                                                                           \
	}
#endif

// Must be used in global space.
#if defined(ZN_GODOT)
#define ZN_GODOT_NAMESPACE_BEGIN
#define ZN_GODOT_NAMESPACE_END
#elif defined(ZN_GODOT_EXTENSION)
#define ZN_GODOT_NAMESPACE_BEGIN namespace godot {
#define ZN_GODOT_NAMESPACE_END }
#endif

// TODO Waiting for a fix, Godot's Variant() can't be constructed from `size_t` on JavaScript and OSX builds.
// See https://github.com/godotengine/godot/issues/36690
#define ZN_SIZE_T_TO_VARIANT(s) static_cast<int64_t>(s)

#endif // ZN_GODOT_MACROS_H
