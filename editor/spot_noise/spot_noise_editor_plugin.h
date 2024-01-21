#ifndef ZN_SPOT_NOISE_EDITOR_PLUGIN_H
#define ZN_SPOT_NOISE_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann {

class ZN_SpotNoiseEditorPlugin : public EditorPlugin {
	GDCLASS(ZN_SpotNoiseEditorPlugin, EditorPlugin)
public:
// Apparently `get_name` is a thing when compiling as a module only
#ifdef ZN_GODOT
	String get_name() const override;
#endif

	ZN_SpotNoiseEditorPlugin();

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}
};

} // namespace zylann

#endif // ZN_SPOT_NOISE_EDITOR_PLUGIN_H
