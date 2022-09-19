#include "fast_noise_lite_editor_inspector_plugin.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_gradient.h"

namespace zylann {

#if defined(ZN_GODOT)
bool ZN_FastNoiseLiteEditorInspectorPlugin::can_handle(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
bool ZN_FastNoiseLiteEditorInspectorPlugin::_can_handle(const Variant &p_object_v) const {
	const Object *p_object = p_object_v;
#endif
	return Object::cast_to<ZN_FastNoiseLite>(p_object) != nullptr ||
			Object::cast_to<ZN_FastNoiseLiteGradient>(p_object);
}

void ZN_FastNoiseLiteEditorInspectorPlugin::ZN_GODOT_UNDERSCORE_PREFIX_IF_EXTENSION(parse_begin)(Object *p_object) {
	ZN_FastNoiseLite *noise_ptr = Object::cast_to<ZN_FastNoiseLite>(p_object);
	if (noise_ptr) {
		Ref<ZN_FastNoiseLite> noise(noise_ptr);

		ZN_FastNoiseLiteViewer *viewer = memnew(ZN_FastNoiseLiteViewer);
		viewer->set_noise(noise);
		add_custom_control(viewer);
		return;
	}
	ZN_FastNoiseLiteGradient *noise_gradient_ptr = Object::cast_to<ZN_FastNoiseLiteGradient>(p_object);
	if (noise_gradient_ptr) {
		Ref<ZN_FastNoiseLiteGradient> noise_gradient(noise_gradient_ptr);

		ZN_FastNoiseLiteViewer *viewer = memnew(ZN_FastNoiseLiteViewer);
		viewer->set_noise_gradient(noise_gradient);
		add_custom_control(viewer);
		return;
	}
}

} // namespace zylann
