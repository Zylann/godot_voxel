#ifndef ZN_GODOT_RESOURCE_LOADER_H
#define ZN_GODOT_RESOURCE_LOADER_H

#if defined(ZN_GODOT)
#include <core/io/resource_loader.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/resource_loader.hpp>
using namespace godot;
#endif

namespace zylann {

PackedStringArray get_recognized_extensions_for_type(const String &type_name);
Ref<Resource> load_resource(const String &path);

} // namespace zylann

#endif // ZN_GODOT_RESOURCE_LOADER_H
