#ifndef ZN_SPOT_NOISE_EDITOR_INSPECTOR_PLUGIN_H
#define ZN_SPOT_NOISE_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/macros.h"

namespace zylann {

class ZN_SpotNoiseEditorInspectorPlugin : public ZN_EditorInspectorPlugin {
	GDCLASS(ZN_SpotNoiseEditorInspectorPlugin, ZN_EditorInspectorPlugin)
protected:
	bool _zn_can_handle(const Object *p_object) const override;
	void _zn_parse_begin(Object *p_object) override;

private:
	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}
};

} // namespace zylann

#endif // ZN_SPOT_NOISE_EDITOR_INSPECTOR_PLUGIN_H
