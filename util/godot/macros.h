#ifndef ZN_GODOT_MACROS_H
#define ZN_GODOT_MACROS_H

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

// Must be used in global space.
#if defined(ZN_GODOT)
#define ZN_GODOT_NAMESPACE_BEGIN
#define ZN_GODOT_NAMESPACE_END
#elif defined(ZN_GODOT_EXTENSION)
#define ZN_GODOT_NAMESPACE_BEGIN namespace godot {
#define ZN_GODOT_NAMESPACE_END }
#endif

#endif // ZN_GODOT_MACROS_H
