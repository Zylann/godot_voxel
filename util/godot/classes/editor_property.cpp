#include "editor_property.h"

namespace zylann {

Span<const Color> editor_property_get_colors(EditorProperty &self) {
	static FixedArray<Color, 4> s_colors;
	const StringName sn_editor("Editor");
	s_colors[0] = self.get_theme_color(StringName("property_color_x"), sn_editor);
	s_colors[1] = self.get_theme_color(StringName("property_color_y"), sn_editor);
	s_colors[2] = self.get_theme_color(StringName("property_color_z"), sn_editor);
	s_colors[3] = self.get_theme_color(StringName("property_color_w"), sn_editor);
	return to_span(s_colors);
}

#if defined(ZN_GODOT)

void ZN_EditorProperty::update_property() {
	_zn_update_property();
}

#elif defined(ZN_GODOT_EXTENSION)

void ZN_EditorProperty::_update_property() {
	_zn_update_property();
}

#endif

void ZN_EditorProperty::_set_read_only(bool p_read_only) {
	_zn_set_read_only(p_read_only);
}

void ZN_EditorProperty::_zn_update_property() {}
void ZN_EditorProperty::_zn_set_read_only(bool p_read_only) {}

} // namespace zylann
