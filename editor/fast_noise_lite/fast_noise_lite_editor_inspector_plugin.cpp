#include "fast_noise_lite_editor_inspector_plugin.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_gradient.h"

namespace zylann {

bool ZN_FastNoiseLiteEditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return Object::cast_to<ZN_FastNoiseLite>(p_object) != nullptr ||
			Object::cast_to<ZN_FastNoiseLiteGradient>(p_object) != nullptr;
}

void ZN_FastNoiseLiteEditorInspectorPlugin::_zn_parse_begin(Object *p_object) {
	const ZN_FastNoiseLite *noise_ptr = Object::cast_to<ZN_FastNoiseLite>(p_object);
	if (noise_ptr != nullptr) {
		Ref<ZN_FastNoiseLite> noise(noise_ptr);

		ZN_FastNoiseLiteViewer *viewer = memnew(ZN_FastNoiseLiteViewer);
		viewer->set_noise(noise);
		add_custom_control(viewer);
		return;
	}
	const ZN_FastNoiseLiteGradient *noise_gradient_ptr = Object::cast_to<ZN_FastNoiseLiteGradient>(p_object);
	if (noise_gradient_ptr != nullptr) {
		Ref<ZN_FastNoiseLiteGradient> noise_gradient(noise_gradient_ptr);

		ZN_FastNoiseLiteViewer *viewer = memnew(ZN_FastNoiseLiteViewer);
		viewer->set_noise_gradient(noise_gradient);
		add_custom_control(viewer);
		return;
	}
}

} // namespace zylann
