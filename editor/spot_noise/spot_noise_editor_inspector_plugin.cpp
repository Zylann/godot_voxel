#include "spot_noise_editor_inspector_plugin.h"
#include "../../util/noise/spot_noise_gd.h"
#include "spot_noise_viewer.h"

namespace zylann {

bool ZN_SpotNoiseEditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<ZN_SpotNoise>(p_object) != nullptr;
}

void ZN_SpotNoiseEditorInspectorPlugin::_zn_parse_begin(Object *p_object) {
	const ZN_SpotNoise *noise_ptr = Object::cast_to<ZN_SpotNoise>(p_object);
	if (noise_ptr != nullptr) {
		Ref<ZN_SpotNoise> noise(noise_ptr);

		ZN_SpotNoiseViewer *viewer = memnew(ZN_SpotNoiseViewer);
		viewer->set_noise(noise);
		add_custom_control(viewer);
		return;
	}
}

} // namespace zylann
