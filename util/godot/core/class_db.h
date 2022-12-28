#ifndef ZN_GODOT_CLASS_DB_H
#define ZN_GODOT_CLASS_DB_H

#if defined(ZN_GODOT)
#include <core/object/object.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/core/class_db.hpp>
#endif

namespace zylann {

template <typename T>
inline void register_abstract_class() {
#if defined(ZN_GODOT)
	ClassDB::register_abstract_class<T>();
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: No way to register abstract classes in GDExtension!
	ClassDB::register_class<T>();
#endif
}

} // namespace zylann

#endif // ZN_GODOT_CLASS_DB_H
