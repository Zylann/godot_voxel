#include "fast_noise_lite_editor_plugin.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_gradient.h"
#include "../fast_noise_2/noise_analysis_window.h"
#include "fast_noise_lite_editor_inspector_plugin.h"
#include "fast_noise_lite_viewer.h"

namespace zylann {

ZN_FastNoiseLiteEditorPlugin::ZN_FastNoiseLiteEditorPlugin() {}

String ZN_FastNoiseLiteEditorPlugin::_zn_get_plugin_name() const {
	return ZN_FastNoiseLite::get_class_static();
}

void ZN_FastNoiseLiteEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Control *base_control = get_editor_interface()->get_base_control();

			_noise_analysis_window = memnew(ZN_NoiseAnalysisWindow);
			base_control->add_child(_noise_analysis_window);

			Ref<ZN_FastNoiseLiteEditorInspectorPlugin> plugin;
			plugin.instantiate();
			plugin->set_noise_analysis_window(_noise_analysis_window);
			add_inspector_plugin(plugin);
		} break;

		default:
			break;
	}
}

} // namespace zylann
