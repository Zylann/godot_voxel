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

} // namespace zylann
