#ifndef ZN_GODOT_CALLABLE_H
#define ZN_GODOT_CALLABLE_H

// TODO Eventually remove this once we check that it's working
#if defined(ZN_GODOT)
#define ZN_GODOT_CALLABLE_MP(m_obj, m_class, m_method) callable_mp(m_obj, &m_class::m_method)
#elif defined(ZN_GODOT_EXTENSION)
#define ZN_GODOT_CALLABLE_MP(m_obj, m_class, m_method) callable_mp(m_obj, &m_class::m_method)
#endif

#endif // ZN_GODOT_CALLABLE_H
