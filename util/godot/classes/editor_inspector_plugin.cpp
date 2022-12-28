#include "editor_inspector_plugin.h"

namespace zylann {

#if defined(ZN_GODOT)
bool ZN_EditorInspectorPlugin::can_handle(Object *obj) {
#elif defined(ZN_GODOT_EXTENSION)
bool ZN_EditorInspectorPlugin::_can_handle(const Variant &obj_v) const {
	const Object *obj = obj_v;
#endif
	return _zn_can_handle(obj);
}

#if defined(ZN_GODOT)
bool ZN_EditorInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
		const PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide) {
#elif defined(ZN_GODOT_EXTENSION)
bool ZN_EditorInspectorPlugin::_parse_property(Object *p_object, int64_t p_type, const String &p_path,
		const int64_t p_hint, const String &p_hint_text, const int64_t p_usage, const bool p_wide) {
#endif
	return _zn_parse_property(
			p_object, Variant::Type(p_type), p_path, PropertyHint(p_hint), p_hint_text, p_usage, p_wide);
}

bool ZN_EditorInspectorPlugin::_zn_can_handle(const Object *obj) const {
	return false;
}

bool ZN_EditorInspectorPlugin::_zn_parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
		const PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide) {
	return false;
}

} // namespace zylann
