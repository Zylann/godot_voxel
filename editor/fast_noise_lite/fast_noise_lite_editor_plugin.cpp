#include "fast_noise_lite_editor_plugin.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_gradient.h"
#include "fast_noise_lite_editor_inspector_plugin.h"
#include "fast_noise_lite_viewer.h"

namespace zylann {

ZN_FastNoiseLiteEditorPlugin::ZN_FastNoiseLiteEditorPlugin() {
	Ref<ZN_FastNoiseLiteEditorInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

#ifdef ZN_GODOT
String ZN_FastNoiseLiteEditorPlugin::get_name() const {
	return ZN_FastNoiseLite::get_class_static();
}
#endif

} // namespace zylann
