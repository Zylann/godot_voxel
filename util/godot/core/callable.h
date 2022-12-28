#ifndef ZN_GODOT_CALLABLE_H
#define ZN_GODOT_CALLABLE_H

#if defined(ZN_GODOT)
#define ZN_GODOT_CALLABLE_MP(m_obj, m_class, m_method) callable_mp(m_obj, &m_class::m_method)
#elif defined(ZN_GODOT_EXTENSION)
// TODO GDX: GDExtension lacks support for method pointers to create Callables...
// We then have to register these methods again like in pre-Callable era.
// See https://github.com/godotengine/godot-cpp/issues/633#issuecomment-930106863
#define ZN_GODOT_CALLABLE_MP(m_obj, m_class, m_method) godot::Callable(m_obj, #m_method)
#endif

#endif // ZN_GODOT_CALLABLE_H
