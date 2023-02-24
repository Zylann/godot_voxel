#include "editor_inspector_plugin.h"

namespace zylann {

#if defined(ZN_GODOT)
bool ZN_EditorInspectorPlugin::can_handle(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
bool ZN_EditorInspectorPlugin::_can_handle(Object *p_object) const {
#endif
	return _zn_can_handle(p_object);
}

#if defined(ZN_GODOT)
void ZN_EditorInspectorPlugin::parse_begin(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
void ZN_EditorInspectorPlugin::_parse_begin(Object *p_object) {
#endif
	_zn_parse_begin(p_object);
}

#if defined(ZN_GODOT)
void ZN_EditorInspectorPlugin::parse_group(Object *p_object, const String &p_group) {
#elif defined(ZN_GODOT_EXTENSION)
void ZN_EditorInspectorPlugin::_parse_group(Object *p_object, const String &p_group) {
#endif
	_zn_parse_group(p_object, p_group);
}

#if defined(ZN_GODOT)
bool ZN_EditorInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
		const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage,
		const bool p_wide) {
#elif defined(ZN_GODOT_EXTENSION)
bool ZN_EditorInspectorPlugin::_parse_property(Object *p_object, Variant::Type p_type, const String &p_path,
		PropertyHint p_hint, const String &p_hint_text, BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
#endif
	return _zn_parse_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage, p_wide);
}

bool ZN_EditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	return false;
}

void ZN_EditorInspectorPlugin::_zn_parse_begin(Object *p_object) {}

void ZN_EditorInspectorPlugin::_zn_parse_group(Object *p_object, const String &p_group) {}

bool ZN_EditorInspectorPlugin::_zn_parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
		const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage,
		const bool p_wide) {
	return false;
}

} // namespace zylann
