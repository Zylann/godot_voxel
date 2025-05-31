#ifndef ZN_GODOT_LABEL_WITH_CUSTOM_TOOLTIP_H
#define ZN_GODOT_LABEL_WITH_CUSTOM_TOOLTIP_H

#include "../classes/control.h"

namespace zylann {

// Empty control used to spawn a custom tooltip when the  mouse goes over it. Add as child of another control.
// This is because in Godot, implementing a custom tooltip requires inheritance.
class ZN_TooltipArea : public Control {
	GDCLASS(ZN_TooltipArea, Control)
public:
	typedef Control *(*Factory)(const ZN_TooltipArea &area, const String &text);

	ZN_TooltipArea();

	void set_factory(Factory f);

protected:
#if defined(ZN_GODOT)
	Control *make_custom_tooltip(const String &for_text) const override;
#elif defined(ZN_GODOT_EXTENSION)
	Object *_make_custom_tooltip(const String &for_text) const override;
#endif

private:
	static void _bind_methods();

	Factory _factory = nullptr;
};

} // namespace zylann

#endif // ZN_GODOT_LABEL_WITH_CUSTOM_TOOLTIP_H
