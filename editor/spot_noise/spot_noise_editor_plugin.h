#ifndef ZN_SPOT_NOISE_EDITOR_PLUGIN_H
#define ZN_SPOT_NOISE_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann {

class ZN_SpotNoiseEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(ZN_SpotNoiseEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	ZN_SpotNoiseEditorPlugin();

protected:
	String _zn_get_plugin_name() const override;

private:
	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}
};

} // namespace zylann

#endif // ZN_SPOT_NOISE_EDITOR_PLUGIN_H
