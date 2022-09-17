#include "resource_loader.h"
#include "resource.h"

namespace zylann {

PackedStringArray get_recognized_extensions_for_type(const String &type_name) {
#if defined(ZN_GODOT)
#error "Implement `get_recognized_extensions_for_type` for modules"
#elif defined(ZN_GODOT_EXTENSION)
	return ResourceLoader::get_singleton()->get_recognized_extensions_for_type(type_name);
#endif
}

Ref<Resource> load_resource(const String &path) {
#if defined(ZN_GODOT)
	return ResourceLoader::load(path);
#elif defined(ZN_GODOT_EXTENSION)
	return ResourceLoader::get_singleton()->load(path);
#endif
}

} // namespace zylann
