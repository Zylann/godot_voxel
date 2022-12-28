#ifndef ZN_GODOT_BINDER_H
#define ZN_GODOT_BINDER_H

#if defined(ZN_GODOT)
#define ZN_GODOT_VARIANT_ENUM_CAST(m_class, m_enum) VARIANT_ENUM_CAST(m_class::m_enum)
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/core/binder_common.hpp>
#define ZN_GODOT_VARIANT_ENUM_CAST(m_class, m_enum) VARIANT_ENUM_CAST(m_class, m_enum)
#endif

#endif // ZN_GODOT_BINDER_H
