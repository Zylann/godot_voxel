#ifndef ZN_GODOT_ENGINE_H
#define ZN_GODOT_ENGINE_H

#if defined(ZN_GODOT)
#include <core/config/engine.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/engine.hpp>
using namespace godot;
#endif

namespace zylann::godot {

inline void add_singleton(const char *name, Object *object) {
#if defined(ZN_GODOT)
	Engine::get_singleton()->add_singleton(Engine::Singleton(name, object));
#elif defined(ZN_GODOT_EXTENSION)
	Engine::get_singleton()->register_singleton(StringName(name), object);
#endif
}

inline void remove_singleton(const char *name) {
#if defined(ZN_GODOT)
	Engine::get_singleton()->remove_singleton(name);
#elif defined(ZN_GODOT_EXTENSION)
	Engine::get_singleton()->unregister_singleton(name);
#endif
}

} // namespace zylann::godot

#endif // ZN_GODOT_ENGINE_H
