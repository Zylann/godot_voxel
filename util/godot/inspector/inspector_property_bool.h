#ifndef ZN_INSPECTOR_PROPERTY_BOOL_H
#define ZN_INSPECTOR_PROPERTY_BOOL_H

#include "../macros.h"
#include "inspector_property.h"

ZN_GODOT_FORWARD_DECLARE(class CheckBox);

namespace zylann {

class ZN_InspectorPropertyBool : public ZN_InspectorProperty {
	GDCLASS(ZN_InspectorPropertyBool, ZN_InspectorProperty)
public:
	ZN_InspectorPropertyBool();

	void set_value(Variant value) override;
	Variant get_value() const override;

	void set_enabled(bool enabled) override;

private:
	void on_check_box_toggled(bool value);

	static void _bind_methods();

	CheckBox *_check_box = nullptr;
};

} // namespace zylann

#endif // ZN_INSPECTOR_PROPERTY_BOOL_H
