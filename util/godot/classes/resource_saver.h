#ifndef ZN_GODOT_RESOURCE_SAVER_H
#define ZN_GODOT_RESOURCE_SAVER_H

#if defined(ZN_GODOT)
#include <core/io/resource_saver.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/resource_saver.hpp>
using namespace godot;
#endif

namespace zylann {

inline Error save_resource(const Ref<Resource> &resource, const String &path, ResourceSaver::SaverFlags flags) {
#if defined(ZN_GODOT)
	return ResourceSaver::save(resource, path, flags);
#elif defined(ZN_GODOT_EXTENSION)
	return ResourceSaver::get_singleton()->save(resource, path, flags);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_RESOURCE_SAVER_H
