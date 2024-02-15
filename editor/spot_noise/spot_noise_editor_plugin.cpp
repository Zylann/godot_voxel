#include "spot_noise_editor_plugin.h"
#include "../../util/noise/spot_noise.h"
#include "spot_noise_editor_inspector_plugin.h"

namespace zylann {

ZN_SpotNoiseEditorPlugin::ZN_SpotNoiseEditorPlugin() {
	Ref<ZN_SpotNoiseEditorInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

#ifdef ZN_GODOT
String ZN_SpotNoiseEditorPlugin::get_name() const {
	return ZN_SpotNoiseEditorPlugin::get_class_static();
}
#endif

} // namespace zylann
