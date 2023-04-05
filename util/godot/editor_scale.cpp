// This file is GDExtension-only, tools-only.

#include "editor_scale.h"
#include "../errors.h"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>

namespace zylann {

// TODO GDX: No exact `EDSCALE` equivalent for porting editor tools.
// It is available in `EditorPlugin` by calling `get_editor_interface()->get_editor_scale()`. But it is not a static
// method, so that means it needs to be passed around by parameter to everything that needs it. This is annoying in
// custom controls that cannot have constructors with parameters, and porting modules in general.
// We can workaround this by creating a dummy instance of `EditorPlugin` once and access `EditorInterface` from it. It
// should work because `EditorPlugin` is not abstract, does nothing in its constructor, and `EditorInterface` is
// internally a singleton, so we don't even need to add the plugin to the tree.
float get_editor_scale() {
	using namespace godot;

	static float s_scale = 1.f;
	static bool s_initialized = false;

	if (!s_initialized) {
		EditorPlugin *dummy_plugin = memnew(EditorPlugin);
		EditorInterface *interface = dummy_plugin->get_editor_interface();
		ZN_ASSERT(interface != nullptr);
		s_scale = interface->get_editor_scale();
		s_initialized = true;
		memdelete(dummy_plugin);
	}

	return s_scale;
}

} // namespace zylann
