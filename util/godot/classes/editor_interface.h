#ifndef ZN_GODOT_EDITOR_INTERFACE_H
#define ZN_GODOT_EDITOR_INTERFACE_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR == 0
#include <editor/editor_plugin.h>
#else
#include <editor/editor_interface.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_interface.hpp>
using namespace godot;
#endif

namespace zylann::godot {
namespace EditorInterfaceShims {

inline EditorFileSystem *get_resource_file_system(EditorInterface &self) {
#if defined(ZN_GODOT)
	return self.get_resource_file_system();
#elif defined(ZN_GODOT_EXTENSION)
	return self.get_resource_filesystem();
#endif
}

} // namespace EditorInterfaceShims
} // namespace zylann::godot

#endif // ZN_GODOT_EDITOR_INTERFACE_H
