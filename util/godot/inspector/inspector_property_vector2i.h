#ifndef ZN_INSPECTOR_PROPERTY_VECTOR2I_H
#define ZN_INSPECTOR_PROPERTY_VECTOR2I_H

#include "../macros.h"
#include "inspector_property.h"

ZN_GODOT_FORWARD_DECLARE(class EditorSpinSlider);

namespace zylann {

class ZN_InspectorPropertyVector2i : public ZN_InspectorProperty {
	GDCLASS(ZN_InspectorPropertyVector2i, ZN_InspectorProperty)
public:
	ZN_InspectorPropertyVector2i();

	void set_value(Variant value) override;
	Variant get_value() const override;

	void set_enabled(bool enabled) override;

	void set_range(const Rect2i rect);

	bool is_wide() override;

private:
	void on_x_value_changed(float new_value);
	void on_y_value_changed(float new_value);

	static void _bind_methods();

	EditorSpinSlider *_x = nullptr;
	EditorSpinSlider *_y = nullptr;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_VECTOR2I_H
