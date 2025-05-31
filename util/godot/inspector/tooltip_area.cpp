#include "tooltip_area.h"

namespace zylann {

ZN_TooltipArea::ZN_TooltipArea() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	set_mouse_filter(Control::MOUSE_FILTER_PASS);
	set_anchors_preset(Control::PRESET_FULL_RECT);
}

void ZN_TooltipArea::set_factory(Factory f) {
	_factory = f;
}

#if defined(ZN_GODOT)
Control *ZN_TooltipArea::make_custom_tooltip(const String &for_text) const {
#elif defined(ZN_GODOT_EXTENSION)
Object *ZN_TooltipArea::_make_custom_tooltip(const String &for_text) const {
#endif
	if (_factory == nullptr) {
		return nullptr;
	}
	Control *custom_tooltip = _factory(*this, for_text);
	return custom_tooltip;
}

void ZN_TooltipArea::_bind_methods() {
	//
}

} // namespace zylann
